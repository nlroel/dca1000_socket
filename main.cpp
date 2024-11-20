#include <iostream>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio.hpp>
#include <iomanip>
#include <cstring>
#include <complex>
#include <fftw3.h>
#include <cmath>

#include "lock_free_queue_peek.h"
#include "mmw_message.h"

#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))

// const char *INTERFACE = "ens1";
const char *SRC_IP_ADDRESS = "192.168.33.180";
const char *LISTEN_IP_ADDRESS = "0.0.0.0";  // 监听所有网卡
const int PORT = 4098;
const uint32_t packetSize_d = 1466;
constexpr uint32_t dataSize_d = packetSize_d - 10;

// raw data size = (2 components) * (2 Bytes) * (# ADC samples) * (# antennas) * (# chirps) * (# frames)
const uint32_t ARRAY_SIZE = 2*2*128*4*3*32;  // 定长数组大小

// 数据包结构
PACK(typedef struct {
    uint32_t seqNum;
    uint8_t byteCnt[6];
    uint8_t payload[dataSize_d];
}) packet_t;


std::array<uint8_t, ARRAY_SIZE> dataArray;
std::array<uint8_t, ARRAY_SIZE> dataArrayGo;
std::mutex mtx;
std::condition_variable cond_var;
std::atomic<bool> trigger(false);

const int ADCSampleSize = 128;
const int FFT_Size = 128;  // 每个信号的长度
const int Signal_Cnt = 4*3*32;    // 批量信号的数量

// 创建一个二维数组来存储 M 个复数信号
fftw_complex* in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_Size * Signal_Cnt);
fftw_complex* out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * FFT_Size * Signal_Cnt);

// 创建 FFTW 计划
fftw_plan plan = fftw_plan_many_dft(1, &FFT_Size, Signal_Cnt, in, NULL, 1, FFT_Size, out, NULL, 1, FFT_Size, FFTW_FORWARD, FFTW_ESTIMATE);

LockFreeQueue<MmwDemo_message> *pgQueue;  // 创建队列对象
std::vector<double> hanning_win;

std::vector<double> generateHanningWindow(int size) {
    std::vector<double> hanningWindow(size);
    for (int i = 0; i < size; ++i) {
        hanningWindow[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (size - 1)));
    }
    return hanningWindow;
}

void fftCompute(
    const std::array<std::complex<double>, ARRAY_SIZE / 4>& arrayIn,
    const std::vector<double>& hanningWindow
) {
    for (int m = 0; m < Signal_Cnt; ++m) {
        for (int n = 0; n < FFT_Size; ++n) {
            // 应用 Hanning 窗
            double realPart = arrayIn[m * ADCSampleSize + n].real() * hanningWindow[n];
            double imagPart = arrayIn[m * ADCSampleSize + n].imag() * hanningWindow[n];

            // 填充 FFTW 的输入数组
            in[m * FFT_Size + n][0] = realPart; // 实部
            in[m * FFT_Size + n][1] = imagPart; // 虚部
        }
    }

    // 执行傅里叶变换
    fftw_execute(plan);
}




std::array<std::complex<double>, ARRAY_SIZE/4> parseDataArray(std::array<uint8_t, ARRAY_SIZE> data) {

    // std::cout << "data: ";
    // for (int i = 0; i < data.size(); ++i) {
    //     std::cout << (int)data[i] << " ";
    // }
    // std::cout << std::endl;
    std::array<std::complex<double>, ARRAY_SIZE/4> dataCplx;
    std::array<int16_t, ARRAY_SIZE/2> dataInt16{};
    memcpy(dataInt16.data(), data.data(), data.size());

    for (int i = 0; i < ARRAY_SIZE/8; i++) {
        int cplxIdx0 = 2*i;
        int cplxIdx1 = 2*i+1;
        int int16Idx0 = 4*i;
        int int16Idx1 = 4*i+1;
        int int16Idx2 = 4*i+2;
        int int16Idx3 = 4*i+3;
        dataCplx[cplxIdx0] = std::complex<double>(dataInt16[int16Idx0], dataInt16[int16Idx2]);
        dataCplx[cplxIdx1] = std::complex<double>(dataInt16[int16Idx1], dataInt16[int16Idx3]);
    }

    return dataCplx;
}


void performComplexOperation() {
    trigger = false;
    // 模拟复杂操作
    auto dataCplx = parseDataArray(dataArrayGo);
    fftCompute(dataCplx, hanning_win);

    std::array<std::complex<double>, 32*128*12> dataTrans{};
    // 数据排列 32*128*12*2(Real/Imag)
    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 128; ++j) {
            for (int k = 0; k < 12; ++k) {
                dataTrans[i*(128*12)+j*12+k] = {out[(i*12+k)*128+j][0], out[(i*12+k)*128+j][1]};
            }
        }
    }

    // memcpy(dataTrans.data(), dataCplx.data(), dataCplx.size() * sizeof(std::complex<double>));

    MmwDemo_message message;
    message.type = ADC_RAW_DATA;  // 这里使用假设的类型 ADC_RAW_DATA
    message.size = sizeof(fftw_complex) * FFT_Size * Signal_Cnt;
    memcpy(message.data, dataTrans.data(), sizeof(dataTrans));

    // 入队
    if (pgQueue->enqueue_with_overwrite(message)) {
        std::cout << "Enqueued message " << std::endl;
    } else {
        std::cout << "Queue is full, unable to enqueue message " << std::endl;
    }
    // for (size_t i = 0; i < 32; ++i) {
    //     std::cout << std::sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]) << " " ;
    // }
    // std::cout << "\n";
}

void udpListener(boost::asio::io_service& io_service) {
    using boost::asio::ip::udp;

    udp::socket socket(io_service, udp::endpoint(boost::asio::ip::address::from_string(LISTEN_IP_ADDRESS), PORT));

    int recv_buffer_size_max = 32*1024; // 32K
    socket.set_option(boost::asio::socket_base::receive_buffer_size(recv_buffer_size_max));
    // socket.set_option(boost::asio::socket_base::receive_buffer_size(sizeof(packet_t)));
    boost::asio::socket_base::reuse_address option(true);
    socket.set_option(option);

    std::cout << "Listening for UDP packets on " << LISTEN_IP_ADDRESS << ":" << PORT << "\n";

    while (true) {
        packet_t packet;
        udp::endpoint senderEndpoint;
        boost::system::error_code error;

        size_t length = socket.receive_from(boost::asio::buffer(&packet, sizeof(packet_t)), senderEndpoint, 0, error);

        if (error && error != boost::asio::error::message_size) {
            std::cerr << "Receive failed: " << error.message() << "\n";
            continue;
        }

        if (length == sizeof(packet_t)) {
            uint32_t seqNum = packet.seqNum;

            int currIndex = ((seqNum-1)*dataSize_d % ARRAY_SIZE);
            // std::cout << "seqNum: " << seqNum << std::endl;
            // std::cout << "currIndex: " << currIndex << std::endl;
            int fitBytes = ARRAY_SIZE - currIndex;
            int remainBytes = dataSize_d;
            // 计算数据拷贝位置
            if (fitBytes < remainBytes) {
                // 合并数据拷贝操作，减少不必要的 memcpy
                std::memcpy(dataArray.data()+currIndex, packet.payload, fitBytes);
                remainBytes -= fitBytes;
                if (currIndex + fitBytes == ARRAY_SIZE) {
                    std::memcpy(dataArrayGo.data(), dataArray.data(), dataArray.size());
                    trigger = true;
                }
                // dataArray.fill(0);
                std::memcpy(dataArray.data(), packet.payload+fitBytes, remainBytes);
            } else {
                std::memcpy(dataArray.data()+currIndex, packet.payload, remainBytes);
            }
            cond_var.notify_one();  // 通知 dataProcessor 执行复杂操作
        } else {
            std::cerr << "Received incomplete packet.\n";
        }
    }
}

void dataProcessor() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cond_var.wait(lock, [] { return trigger.load(); });

        performComplexOperation();
    }
}

int main() {
    boost::asio::io_service io_service;

    pgQueue = new LockFreeQueue<MmwDemo_message>("shared_queue",10);
    pgQueue->clear();

    hanning_win = generateHanningWindow(128);

    std::thread listenerThread([&io_service]() { udpListener(io_service); });
    std::thread processorThread(dataProcessor);

    if (ARRAY_SIZE <= dataSize_d) {
        std::cerr << "Data size too small." << std::endl;
        return 12345;
    }

    dataArray.fill(0);
    dataArrayGo.fill(0);

    listenerThread.join();
    processorThread.join();

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return 0;
}

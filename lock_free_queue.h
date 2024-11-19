//
// Created by lroel on 24-11-15.
//

#ifndef LOCK_FREE_QUEUE_H
#define LOCK_FREE_QUEUE_H

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <utility>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_MESSAGE_SIZE 5242880  // 定长数据块大小

// 定义枚举类型
typedef enum MmwDemo_message_type_e {
    ADC_RAW_DATA = 0,
} MmwDemo_message_type;

// 定义消息结构体
typedef struct MmwDemo_message_t {
    MmwDemo_message_type type;
    uint32_t size;            // 实际数据大小
    uint8_t data[MAX_MESSAGE_SIZE];    // 定长数据区（最大 5MB）
} MmwDemo_message;

// 无锁环形队列类
template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue(const std::string& shm_name, size_t size);  // 构造函数，初始化队列
    ~LockFreeQueue();            // 析构函数，清理资源

    bool enqueue(const T& item);  // 入队方法
    bool dequeue(T& item);        // 出队方法

    size_t get_capacity() const;  // 获取队列容量
    void clear();                 // 清空队列

private:
    int shm_fd;                   // 共享内存文件描述符
    void* shm_addr;               // 映射的共享内存地址
    T *data; // 队列数据
    std::atomic<size_t>* head;    // 队列头指针
    std::atomic<size_t> *tail; // 队列尾指针
    std::string shm_name;
    size_t size;
    size_t capacity; // 队列容量
};

// 队列构造函数
template <typename T>
LockFreeQueue<T>::LockFreeQueue(const std::string& name, const size_t size) : size(size), capacity(size + 1) {
    shm_name = "/shm_" + name;
    shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to open shared memory");
        exit(1);
    }
    if (ftruncate(shm_fd, 2 * sizeof(std::atomic<size_t>) + capacity * sizeof(T)) == -1) {
        perror("Failed to set size for shared memory");
        exit(1);
    }
    shm_addr = mmap(nullptr, 2 * sizeof(std::atomic<size_t>) + capacity * sizeof(T),
                     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("Failed to map shared memory");
        exit(1);
    }

    std::cout << "Created shared memory at " << shm_addr << std::endl;
    head = reinterpret_cast<std::atomic<size_t>*>(shm_addr);
    tail = head + 1;
    data = reinterpret_cast<T*>(tail + 1);

    if (head->load(std::memory_order_relaxed) == 0 && tail->load(std::memory_order_relaxed) == 0) {
        head->store(0, std::memory_order_relaxed);
        tail->store(0, std::memory_order_relaxed);
    }
    close(shm_fd);
}

// 队列析构函数
template <typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    if (shm_addr != MAP_FAILED) {
        munmap(shm_addr, 2 * sizeof(std::atomic<size_t>) + capacity * sizeof(T));
    }
}

// 入队操作
template <typename T>
bool LockFreeQueue<T>::enqueue(const T& item) {
    size_t current_tail = tail->load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % capacity;

    if (next_tail == head->load(std::memory_order_acquire)) {
        return false;  // 队列满
    }
    std::memcpy(&data[current_tail], &item, sizeof(T));
    tail->store(next_tail, std::memory_order_release);
    return true;
}

// 出队操作
template <typename T>
bool LockFreeQueue<T>::dequeue(T& item) {
    size_t current_head = head->load(std::memory_order_relaxed);

    if (current_head == tail->load(std::memory_order_acquire)) {
        return false;  // 队列空
    }
    std::memcpy(&item, &data[current_head], sizeof(T));
    head->store((current_head + 1) % capacity, std::memory_order_release);
    return true;
}

// 获取队列容量
template <typename T>
size_t LockFreeQueue<T>::get_capacity() const {
    return capacity - 1;
}

// 清空队列
template <typename T>
void LockFreeQueue<T>::clear() {
    head->store(0, std::memory_order_relaxed);
    tail->store(0, std::memory_order_relaxed);
}

// 明确实例化模板类（对于 MmwDemo_message 类型）
template class LockFreeQueue<MmwDemo_message>;

#endif // LOCK_FREE_QUEUE_H

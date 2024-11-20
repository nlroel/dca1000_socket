//
// Created by peng.liu06 on 24-10-28.
//

#ifndef MMW_K26_V7_3_FILE_HANDLE_H_
#define MMW_K26_V7_3_FILE_HANDLE_H_

#include <fstream>
#include <iomanip>
#include <string>


size_t convertToBytes(const std::string& sizeStr);
std::string generateTimestampedFileName(const std::string& prefix = "radar_sd_", const std::string& extension = ".dat");

class FileHandle {
    private:
    std::string fileName;
    std::string filePath;
    std::ofstream fileStream;
    size_t bytesWritten; // 记录写入的总字节数

    public:
    // 构造函数
    FileHandle(std::string  fileName, std::string  filePath);

    // 析构函数
    ~FileHandle();

    // 打开文件，支持传入打开模式
    bool open(std::ios::openmode mode = std::ios::out | std::ios::app);

    // 写入数据
    bool write(const std::string& data);

    // 写入数据块
    bool write(const char* data, size_t size);

    // 关闭文件
    void close();

    // 获取写入的总字节数
    size_t getBytesWritten() const;

    // 重置写入字节统计
    void resetBytesWritten();

    // 检查文件是否存在
    bool fileExists() const;

    // 获取完整文件路径
    std::string getFullPath() const;

    // 更新文件路径
    void setFilePath(const std::string& newFilePath);

    // 更新文件名
    void setFileName(const std::string& newFileName);
};



#endif //MMW_K26_V7_3_FILE_HANDLE_H_

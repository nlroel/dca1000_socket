//
// Created by peng.liu06 on 24-10-28.
//



#include <utility>
#include <string>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <sys/stat.h>

#include "file_handle.h"

size_t convertToBytes(const std::string& sizeStr) {
    if (sizeStr.empty()) {
        throw std::invalid_argument("Size string is empty.");
    }

    // 提取数值部分
    size_t i = 0;
    while (i < sizeStr.size() && (std::isdigit(sizeStr[i]) || sizeStr[i] == '.')) {
        ++i;
    }

    double size = std::stod(sizeStr.substr(0, i)); // 将数值部分转换为 double
    std::string unit = sizeStr.substr(i); // 提取单位部分

    // 转换单位（默认是字节B）
    bool isBit = false;

    // 处理单位并转换为字节数
    if (unit == "K" || unit == "k" || unit == "KB" || unit == "kb") {
        size *= 1024;
    } else if (unit == "M" || unit == "m" || unit == "MB" || unit == "mb") {
        size *= 1024 * 1024;
    } else if (unit == "G" || unit == "g" || unit == "GB" || unit == "gb") {
        size *= 1024 * 1024 * 1024;
    } else if (unit == "T" || unit == "t" || unit == "TB" || unit == "tb") {
        size *= 1024LL * 1024 * 1024 * 1024;
    } else if (unit == "B" || unit.empty()) {
        // 默认是字节，无需转换
    } else if (unit == "b") {
        isBit = true; // 是比特，需要转换为字节
    } else {
        throw std::invalid_argument("Invalid unit in size string.");
    }

    // 如果是比特（b），则除以8转换为字节
    if (isBit) {
        size /= 8;
    }

    return static_cast<size_t>(size);
}


std::string generateTimestampedFileName(const std::string& prefix , const std::string& extension) {
    // 获取当前时间
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    // 格式化时间字符串
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", t);

    // 生成完整文件名
    std::ostringstream fileName;
    fileName << prefix << timestamp << extension;

    return fileName.str();
}

// 构造函数
FileHandle::FileHandle(std::string  fileName, std::string  filePath)
    : fileName(std::move(fileName)), filePath(std::move(filePath)), bytesWritten(0) {}

// 析构函数，确保文件在销毁时关闭
FileHandle::~FileHandle() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

// 打开文件，接受可选的打开模式
bool FileHandle::open(std::ios::openmode mode) {
    fileStream.open(getFullPath(), mode);
    return fileStream.is_open();
}

// 写入数据
bool FileHandle::write(const std::string& data) {
    if (!fileStream.is_open()) {
        return false;
    }
    fileStream << data;
    // 检查写入是否成功，并更新字节数
    if (fileStream.good()) {
        bytesWritten += data.size(); // 更新写入的字节数
        return true;
    }
    return false;
}

// 写入二进制数据块
bool FileHandle::write(const char* data, size_t size) {
    if (!fileStream.is_open()) {
        return false;
    }
    fileStream.write(data, size);
    if (fileStream.good()) {
        bytesWritten += size; // 更新写入的字节数
        return true;
    }
    return false;
}

// 关闭文件
void FileHandle::close() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

// 获取写入的总字节数
size_t FileHandle::getBytesWritten() const {
    return bytesWritten;
}

// 重置写入字节统计
void FileHandle::resetBytesWritten() {
    bytesWritten = 0;
}

// 检查文件是否存在
bool FileHandle::fileExists() const {
    struct stat buffer;
    return (stat(getFullPath().c_str(), &buffer) == 0);
}

// 获取完整文件路径
std::string FileHandle::getFullPath() const {
    return filePath + "/" + fileName;
}

// 更新文件路径
void FileHandle::setFilePath(const std::string& newFilePath) {
    if (fileStream.is_open()) {
        fileStream.close();
    }
    filePath = newFilePath;
}

// 更新文件名
void FileHandle::setFileName(const std::string& newFileName)
{
    if (fileStream.is_open()) {
        fileStream.close();
    }
    fileName = newFileName;
}
//
// Created by lroel on 24-11-15.
//

#ifndef LOCK_FREE_QUEUE_PEEK_H
#define LOCK_FREE_QUEUE_PEEK_H

#include <atomic>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <utility>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>


// 无锁环形队列类
template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue(const std::string& shm_name, size_t size, size_t max_element_size = sizeof(T));  // 构造函数，初始化队列
    ~LockFreeQueue();            // 析构函数，清理资源

    bool enqueue(const T& item);  // 入队方法
    bool enqueue_with_overwrite(const T& item); // 覆盖入队方法
    bool dequeue(T& item);        // 出队方法
    bool peek(T& item) const;           // 查看方法

    size_t get_capacity() const;  // 获取队列容量
    size_t get_max_element_size() const;  // 获取单个元素最大存储大小
    void clear();                 // 清空队列

private:
    int shm_fd;                   // 共享内存文件描述符
    void* shm_addr;               // 映射的共享内存地址
    T *data; // 队列数据
    std::atomic<size_t>* head;    // 队列头指针
    std::atomic<size_t> *tail; // 队列尾指针
    std::string shm_name;// 共享内存名称
    size_t size;
    size_t capacity; // 队列容量
    size_t element_size;          // 单个元素最大存储大小
};

// 队列构造函数
template <typename T>
LockFreeQueue<T>::LockFreeQueue(const std::string& name, const size_t size, const size_t max_element_size)
    : size(size), capacity(size + 1), element_size(max_element_size) {
    if (max_element_size < sizeof(T)) {
        throw std::invalid_argument("Max element size cannot be smaller than the size of T.");
    }

    shm_name = "/shm_" + name;
    shm_fd = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to open shared memory");
        throw std::runtime_error("shm_open failed.");
    }

    size_t total_size = 2 * sizeof(std::atomic<size_t>) + capacity * max_element_size;
    if (ftruncate(shm_fd, total_size) == -1) {
        perror("Failed to set size for shared memory");
        close(shm_fd);
        shm_unlink(shm_name.c_str());
        throw std::runtime_error("ftruncate failed.");
    }
    shm_addr = mmap(nullptr, total_size,PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_addr == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        shm_unlink(shm_name.c_str());
        throw std::runtime_error("mmap failed.");
    }

    std::cout << "Created shared memory at " << shm_addr << std::endl;
    head = reinterpret_cast<std::atomic<size_t>*>(shm_addr);
    tail = head + 1;
    // 确保 data 起始地址的对齐
    data = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(tail + 1));
    if (reinterpret_cast<uintptr_t>(data) % alignof(T) != 0) {
        munmap(shm_addr, total_size);
        close(shm_fd);
        shm_unlink(shm_name.c_str());
        throw std::runtime_error("Shared memory alignment failed.");
    }

    // 初始化 head 和 tail
    if (head->load(std::memory_order_relaxed) == 0 && tail->load(std::memory_order_relaxed) == 0) {
        head->store(0, std::memory_order_relaxed);
        tail->store(0, std::memory_order_relaxed);
    }
    close(shm_fd);
}

// 队列析构函数
template <typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    size_t total_size = 2 * sizeof(std::atomic<size_t>) + capacity * element_size;
    if (shm_addr != MAP_FAILED) {
        if (munmap(shm_addr, total_size) == -1) {
            perror("Failed to unmap shared memory");
        }
    }

    if (!shm_name.empty() && shm_unlink(shm_name.c_str()) == -1) {
        perror("Failed to unlink shared memory");
    }
}

// 入队操作
template <typename T>
bool LockFreeQueue<T>::enqueue(const T& item) {
    if (sizeof(item) > element_size) {
        std::cerr << "Item size exceeds the maximum allowed size of " << element_size << " bytes." << std::endl;
        return false;
    }

    size_t current_tail = tail->load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % capacity;

    if (next_tail == head->load(std::memory_order_acquire)) {
        return false;  // 队列满
    }

    void* target = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(data) + current_tail * element_size);
    std::memcpy(target, &item, sizeof(T));
    tail->store(next_tail, std::memory_order_release);
    return true;
}

// 覆盖入队操作
template <typename T>
bool LockFreeQueue<T>::enqueue_with_overwrite(const T& item) {
    if (sizeof(item) > element_size) {
        std::cerr << "Item size exceeds the maximum allowed size of " << element_size << " bytes." << std::endl;
        return false;
    }
    size_t current_tail = tail->load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % capacity;

    if (next_tail == head->load(std::memory_order_acquire)) {
        // 队列满了，覆盖最旧数据
        head->store((head->load(std::memory_order_relaxed) + 1) % capacity, std::memory_order_release);
    }

    void* target = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(data) + current_tail * element_size);
    std::memcpy(target, &item, sizeof(T));
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

    void* source = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(data) + current_head * element_size);
    std::memcpy(&item, source, sizeof(T));
    head->store((current_head + 1) % capacity, std::memory_order_release);
    return true;
}

// 查看最新内容（不移除）
template <typename T>
bool LockFreeQueue<T>::peek(T& item) const {
    size_t current_tail = tail->load(std::memory_order_acquire);

    if (current_tail == head->load(std::memory_order_acquire)) {
        return false;  // 队列为空
    }

    size_t latest_index = (current_tail == 0) ? (capacity - 1) : (current_tail - 1);
    void* source = reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(data) + latest_index * element_size);
    std::memcpy(&item, source, sizeof(T));

    return true;
}

// 获取队列容量
template <typename T>
size_t LockFreeQueue<T>::get_capacity() const {
    return capacity - 1;
}

// 获取单个元素最大存储大小
template <typename T>
size_t LockFreeQueue<T>::get_max_element_size() const {
    return element_size;
}

// 清空队列
template <typename T>
void LockFreeQueue<T>::clear() {
    if (!head || !tail) {
        std::cerr << "Cannot clear queue: head or tail is null." << std::endl;
        return;
    }

    head->store(0, std::memory_order_relaxed);
    tail->store(0, std::memory_order_relaxed);
    std::cout << "Queue cleared successfully." << std::endl;
}

#endif // LOCK_FREE_QUEUE_PEEK_H

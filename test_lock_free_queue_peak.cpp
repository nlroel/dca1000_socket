//
// Created by lroel on 24-11-20.
//

#include "lock_free_queue_peek.h"
#include <iostream>
#include <vector>

#define MAX_MESSAGE_SIZE 5242880  // 定长数据块大小

// 定义枚举类型
typedef enum MmwDemo_message_type_e {
    ADC_RAW_DATA = 0,
} MmwDemo_message_type;

// 定义消息结构体
typedef struct MmwDemo_message_t {
    MmwDemo_message_type type;
    uint32_t size;            // 实际数据大小
    uint8_t data[MAX_MESSAGE_SIZE];    // 变长数据区（最大 5MB）
} MmwDemo_message;

LockFreeQueue<MmwDemo_message> *pqueue = new LockFreeQueue<MmwDemo_message>("example", 3);

// 定义测试消息
MmwDemo_message msg1 = {ADC_RAW_DATA, 100, {1, 2, 3}};
MmwDemo_message msg2 = {ADC_RAW_DATA, 200, {4, 5, 6}};
MmwDemo_message msg3 = {ADC_RAW_DATA, 300, {7, 8, 9}};
MmwDemo_message msg4 = {ADC_RAW_DATA, 400, {10, 11, 12}};

MmwDemo_message peeked;
MmwDemo_message dequeued;



int main() {
    try {
        // 创建队列，容量为 3，每个元素最大大小为 512 字节
        std::cout << "==== Enqueue Operations ====\n";

        // 入队操作
        pqueue->enqueue(msg1);
        pqueue->enqueue(msg2);
        pqueue->enqueue(msg3);

        // Peek 查看最新数据
        if (pqueue->peek(peeked)) {
            std::cout << "Peeked: type=" << peeked.type << ", size=" << peeked.size << "\n";
        }

        // 尝试再入队，触发覆盖逻辑
        std::cout << "Adding msg4, should overwrite oldest data.\n";
        pqueue->enqueue(msg4);

        // 出队并打印队列数据
        std::cout << "==== Dequeue Operations ====\n";
        while (pqueue->dequeue(dequeued)) {
            std::cout << "Dequeued: type=" << dequeued.type << ", size=" << dequeued.size << "\n";
        }

        std::cout << "==== Enqueue with Overwrite ====\n";

        // 使用覆盖逻辑的入队操作
        pqueue->enqueue_with_overwrite(msg1);
        pqueue->enqueue_with_overwrite(msg2);
        pqueue->enqueue_with_overwrite(msg3);

        // Peek 再次查看最新数据
        if (pqueue->peek(peeked)) {
            std::cout << "Peeked after overwrite: type=" << peeked.type << ", size=" << peeked.size << "\n";
        }

        // 再次触发覆盖逻辑
        std::cout << "Adding msg4 with overwrite.\n";
        pqueue->enqueue_with_overwrite(msg4);

        // 出队并打印队列数据
        std::cout << "==== Dequeue After Overwrite ====\n";
        while (pqueue->dequeue(dequeued)) {
            std::cout << "Dequeued: type=" << dequeued.type << ", size=" << dequeued.size << "\n";
        }

        // 清空队列测试
        std::cout << "Clearing queue...\n";
        pqueue->clear();

        if (!pqueue->dequeue(dequeued)) {
            std::cout << "Queue is empty after clear.\n";
        }

        // 销毁队列
        delete pqueue;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

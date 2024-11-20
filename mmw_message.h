//
// Created by lroel on 24-11-20.
//

#ifndef MMW_MESSAGE_H
#define MMW_MESSAGE_H

#include <cstdint>

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

#endif //MMW_MESSAGE_H

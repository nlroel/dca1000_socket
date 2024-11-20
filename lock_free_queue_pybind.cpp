//
// Created by lroel on 24-11-15.
//

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "lock_free_queue_peek.h"
#include "mmw_message.h"

namespace py = pybind11;

PYBIND11_MODULE(lock_free_queue, m) {
    // 绑定枚举类型 MmwDemo_message_type
    py::enum_<MmwDemo_message_type>(m, "MmwDemo_message_type")
        .value("ADC_RAW_DATA", MmwDemo_message_type::ADC_RAW_DATA);

    // 暴露 MAX_MESSAGE_SIZE 常量到 Python
    m.attr("MAX_MESSAGE_SIZE") = MAX_MESSAGE_SIZE;

    // 绑定 LockFreeQueue 类型
    py::class_<LockFreeQueue<MmwDemo_message>>(m, "LockFreeQueueMessage")
        .def(py::init<std::string, size_t>(), py::arg("shm_name"), py::arg("size"))  // 绑定构造函数
        .def("enqueue", [](LockFreeQueue<MmwDemo_message>& self, MmwDemo_message_type type, const std::vector<uint8_t>& data) {
            if (data.size() > MAX_MESSAGE_SIZE) {
                throw std::invalid_argument("Data size exceeds MAX_MESSAGE_SIZE");
            }
            MmwDemo_message message;
            message.type = type;
            message.size = data.size();
            std::memcpy(message.data, data.data(), data.size());
            return self.enqueue(message);
        })
        .def("enqueue_with_overwrite", [](LockFreeQueue<MmwDemo_message>& self, MmwDemo_message_type type, const std::vector<uint8_t>& data) {
            if (data.size() > MAX_MESSAGE_SIZE) {
                throw std::invalid_argument("Data size exceeds MAX_MESSAGE_SIZE");
            }
            MmwDemo_message message;
            message.type = type;
            message.size = data.size();
            std::memcpy(message.data, data.data(), data.size());
            return self.enqueue_with_overwrite(message);
        })
        .def("dequeue", [](LockFreeQueue<MmwDemo_message>& self) {
            MmwDemo_message message;
            if (self.dequeue(message)) {
                return std::make_tuple(reinterpret_cast<uintptr_t>(&message), message.size, message.type);
            } else {
                return std::make_tuple<uintptr_t, uint32_t, MmwDemo_message_type>(0, 0, MmwDemo_message_type::ADC_RAW_DATA); // 返回默认值
            }
        })
        .def("peek", [](LockFreeQueue<MmwDemo_message>& self) {
            MmwDemo_message message;
            if (self.dequeue(message)) {
                return std::make_tuple(reinterpret_cast<uintptr_t>(&message), message.size, message.type);
            } else {
                return std::make_tuple<uintptr_t, uint32_t, MmwDemo_message_type>(0, 0, MmwDemo_message_type::ADC_RAW_DATA); // 返回默认值
            }
        })
        .def("get_capacity", &LockFreeQueue<MmwDemo_message>::get_capacity)
        .def("get_max_element_size", &LockFreeQueue<MmwDemo_message>::get_max_element_size)
        .def("clear", &LockFreeQueue<MmwDemo_message>::clear);
}

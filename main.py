import ctypes
import numpy as np
import threading
import queue
from matplotlib import pyplot as plt
import lock_free_queue

# 创建队列对象
lockfreequeue = lock_free_queue.LockFreeQueueMessage("shared_queue", 10)
lockfreequeue.clear()

## 用于线程间通信的队列
# data_queue = queue.Queue()

# def data_processing_thread():
#     while True:
#         try:
#             ptr, size, msg_type = lockfreequeue.dequeue()
#             if ptr != 0:
#                 class MmwDemoMessage(ctypes.Structure):
#                     _fields_ = [("type", ctypes.c_int),
#                                 ("size", ctypes.c_uint32)]
#                 message_ptr = ctypes.cast(ptr, ctypes.POINTER(MmwDemoMessage))
#                 message = message_ptr.contents
#
#                 if message.size > 0:
#                     # 计算数据开始的位置
#                     data_address = ctypes.addressof(message) + ctypes.sizeof(MmwDemoMessage)
#                     # 以双精度复数形式计算数据数量（每个复数由两个double组成）
#                     num_complexes = message.size // (2 * ctypes.sizeof(ctypes.c_double))
#
#                     # 直接从内存创建复数数组视图
#                     data_ptr = (ctypes.c_double * (2 * num_complexes)).from_address(data_address)
#                     actual_data = np.frombuffer(data_ptr, dtype=np.complex128, count=num_complexes)
#
#                     # 重塑数组为期望的形状
#                     return actual_data.reshape((32, 128, 12))
#                 else:
#                     print("No data in message")
#         except Exception as e:
#             print(f"Exception in data processing: {e}")

def data_get_frame():
    try:
        ptr, size, msg_type = lockfreequeue.peek()
        if ptr != 0:
            class MmwDemoMessage(ctypes.Structure):
                _fields_ = [("type", ctypes.c_int),
                            ("size", ctypes.c_uint32)]
            message_ptr = ctypes.cast(ptr, ctypes.POINTER(MmwDemoMessage))
            message = message_ptr.contents

            if message.size > 0:
                # 计算数据开始的位置
                data_address = ctypes.addressof(message) + ctypes.sizeof(MmwDemoMessage)
                # 以双精度复数形式计算数据数量（每个复数由两个double组成）
                num_complexes = message.size // (2 * ctypes.sizeof(ctypes.c_double))

                # 直接从内存创建复数数组视图
                data_ptr = (ctypes.c_double * (2 * num_complexes)).from_address(data_address)
                actual_data = np.frombuffer(data_ptr, dtype=np.complex128, count=num_complexes)

                # 重塑数组为期望的形状
                return actual_data.reshape((32, 128, 12))
            else:
                print("No data in message")
    except Exception as e:
        print(f"Exception in data processing: {e}")
    return None

def plot_data():
    plt.figure(figsize=(10, 6))
    while True:
        try:
            # data_np_cplx = data_queue.get(timeout=10)  # 尝试从队列中获取数据
            data_np_cplx = data_get_frame()
            absdata = np.abs(data_np_cplx[0, :, 0])
            plt.plot(absdata, 'y-')
            plt.title("Magnitude of Complex Data")
            plt.xlabel("Sample Index")
            plt.ylabel("Magnitude")
            plt.grid(True)
            plt.pause(1)  # 暂停一秒钟
            plt.clf()  # 清除当前图形
        except queue.Empty:
            print("No data available for plotting.")
            plt.pause(1)
            plt.clf()

def main():
    # processing_thread = threading.Thread(target=data_processing_thread)
    # processing_thread.daemon = True
    # processing_thread.start()

    plot_data()  # 开始绘图，持续更新

if __name__ == '__main__':
    main()

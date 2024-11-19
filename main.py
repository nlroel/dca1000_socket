import ctypes
import numpy as np
import threading
import queue
from matplotlib import pyplot as plt
import lock_free_queue

# 创建队列对象
lockfreequeue = lock_free_queue.LockFreeQueueMessage("shared_queue", 10)
lockfreequeue.clear()

# 用于线程间通信的队列
data_queue = queue.Queue()

def data_processing_thread():
    while True:
        try:
            ptr, size, msg_type = lockfreequeue.dequeue()
            if ptr != 0:
                class MmwDemoMessage(ctypes.Structure):
                    _fields_ = [("type", ctypes.c_int),
                                ("size", ctypes.c_uint32)]
                message_ptr = ctypes.cast(ptr, ctypes.POINTER(MmwDemoMessage))
                message = message_ptr.contents

                if message.size > 0:
                    data_type = ctypes.POINTER(ctypes.c_double)
                    data_ptr = ctypes.cast(
                        ctypes.addressof(message_ptr.contents) + ctypes.sizeof(MmwDemoMessage), data_type
                    )
                    actual_data = [data_ptr[i] for i in range(message.size // ctypes.sizeof(ctypes.c_double))]

                    try:
                        data_np = np.array(actual_data).reshape(32, 128, 12, -1)
                        data_np_cplx = data_np[:, :, :, 0] + 1j * data_np[:, :, :, 1]
                        data_queue.put(data_np_cplx)  # 将处理后的数据放入队列
                    except ValueError as e:
                        print(f"Error reshaping data: {e}")
                else:
                    print("No data in message")
        except Exception as e:
            print(f"Exception in data processing: {e}")

def plot_data():
    plt.figure(figsize=(10, 6))
    while True:
        try:
            data_np_cplx = data_queue.get(timeout=10)  # 尝试从队列中获取数据
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
    processing_thread = threading.Thread(target=data_processing_thread)
    processing_thread.daemon = True
    processing_thread.start()

    plot_data()  # 开始绘图，持续更新

if __name__ == '__main__':
    main()

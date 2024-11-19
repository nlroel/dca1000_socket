import ctypes
import numpy as np
import threading
from matplotlib import pyplot as plt
from matplotlib.animation import FuncAnimation
import lock_free_queue

# 创建队列对象
lockfreequeue = lock_free_queue.LockFreeQueueMessage("shared_queue", 10)
lockfreequeue.clear()

# 全局变量
data_np_cplx = np.zeros((32, 128, 12), dtype=np.complex64)

def data_processing_thread():
    global data_np_cplx
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
                    except ValueError as e:
                        print(f"Error reshaping data: {e}")
                else:
                    print("No data in message")
        except Exception as e:
            print(f"Exception in data processing: {e}")

def update_plot(frame):
    global data_np_cplx
    absdata = np.abs(data_np_cplx[0, :, 0])
    line.set_ydata(absdata)
    return line,



if __name__ == '__main__':
    fig, ax = plt.subplots()
    xdata = np.arange(128)  # Adjust according to your actual x-axis data
    line, = ax.plot(xdata, np.zeros(128), 'y-')
    ax.set_xlim(0, 127)  # Set x-axis limit
    ax.set_ylim(0, 10)  # Set y-axis limit according to expected magnitude range

    # Start data processing thread
    processing_thread = threading.Thread(target=data_processing_thread)
    processing_thread.daemon = True
    processing_thread.start()

    # Start plotting animation
    ani = FuncAnimation(fig, update_plot, blit=True, interval=50)

    plt.show()

    # Wait for the data processing thread to finish
    processing_thread.join()

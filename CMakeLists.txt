cmake_minimum_required(VERSION 3.29)
project(dca1000_socket)

set(CMAKE_CXX_STANDARD 23)

add_executable(dca1000_socket main.cpp file_handle.cpp)

target_link_libraries(dca1000_socket
        boost_system
        pthread
        fftw3
        rt)


add_executable(test_lock_free_queue test_lock_free_queue_peak.cpp)
target_link_libraries(test_lock_free_queue rt)


SET(PYTHON_EXECUTABLE /home/lroel/tool/miniconda3/envs/mmw/bin/python3.10)
add_subdirectory(extern/pybind11)
include_directories(extern/pybind11/include)
pybind11_add_module(lock_free_queue lock_free_queue_pybind.cpp)
target_link_libraries(lock_free_queue PUBLIC rt)




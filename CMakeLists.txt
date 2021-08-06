cmake_minimum_required(VERSION 3.19)
project(pytroch_ddsp_rt)

set(CMAKE_CXX_STANDARD 14)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/modules)

set(CMAKE_PREFIX_PATH "libtorch")

find_package(Torch REQUIRED)
find_package(JACK REQUIRED)

add_executable(pytroch_ddsp_rt main.cpp JackConnection.cpp JackConnection.h)

target_link_libraries(pytroch_ddsp_rt "${TORCH_LIBRARIES}" ${JACK_LIBRARIES})

set_property(TARGET pytroch_ddsp_rt PROPERTY CXX_STANDARD 14)

cmake_minimum_required(VERSION 2.8)
project(waldorfbot)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include(conanbuildinfo.cmake)
CONAN_BASIC_SETUP()

include_directories(SYSTEM
)

set(SOURCE_FILES main.cpp event_receiver.cpp event_receiver.h route_set.cpp route_set.h)
add_executable(waldorfbot ${SOURCE_FILES})
target_link_libraries(waldorfbot ${CONAN_LIBS})

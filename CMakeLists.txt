cmake_minimum_required(VERSION 2.8)
project(waldorfbot)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include(conanbuildinfo.cmake)
CONAN_BASIC_SETUP()

set(SOURCE_FILES main.cpp event_receiver.cpp event_receiver.h logging.h beep_boop_persist.cpp beep_boop_persist.h team_info.cpp team_info.h team_info.cpp team_info.h beep_boop_persist.cpp beep_boop_persist.h)
add_executable(waldorfbot ${SOURCE_FILES})

message(STATUS Conan libs: ${CONAN_LIBS})
target_link_libraries(waldorfbot ${CONAN_LIBS})

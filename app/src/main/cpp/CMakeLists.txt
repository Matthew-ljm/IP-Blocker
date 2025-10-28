cmake_minimum_required(VERSION 3.5)

set(CMAKE_BUILD_TYPE Release)

if ((CMAKE_BUILD_TYPE STREQUAL "Release") OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
    add_definitions(-DRELEASE_MODE=1)
endif()

set(CMAKE_CXX_STANDARD 20)  # 注意：Android NDK对C++20的支持有限，若编译报错，可降为17

project(TunMode)

# 头文件目录（原配置正确，保留）
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src/)
include_directories(${CMAKE_SOURCE_DIR}/src/internal)

# （可选）如果有预编译库，保留以下配置；否则注释
# if (${ANDROID_ABI} STREQUAL "armeabi-v7a")
#     set(TUNMODE_LIBS "${CMAKE_SOURCE_DIR}/lib/armeabi-v7a")
# elseif (${ANDROID_ABI} STREQUAL "arm64-v8a")
#     set(TUNMODE_LIBS "${CMAKE_SOURCE_DIR}/lib/arm64-v8a")
# elseif (${ANDROID_ABI} STREQUAL "x86_64")
#     set(TUNMODE_LIBS "${CMAKE_SOURCE_DIR}/lib/x86_64")
# else()
#     message(WARNING "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
#     set(TUNMODE_LIBS "")
# endif()
# link_directories(${TUNMODE_LIBS})

# 源码文件列表（原配置已包含所有文件，保留）
add_library(tunmode SHARED
    main.cxx

    src/tunmode/tunmode.cxx

    src/tunmode/common/inbuffer.cxx
    src/tunmode/common/buffer.cxx
    src/tunmode/common/packet.cxx
    src/tunmode/common/utils.cxx

    src/tunmode/session/session.cxx
    src/tunmode/session/tcpsession.cxx
    src/tunmode/session/udpsession.cxx

    src/tunmode/manager/sessionmanager.cxx
    src/tunmode/manager/tcpmanager.cxx
    src/tunmode/manager/udpmanager.cxx

    src/tunmode/socket/socket.cxx
    src/tunmode/socket/sessionsocket.cxx
    src/tunmode/socket/tunsocket.cxx
    src/tunmode/socket/tcpsocket.cxx
    src/tunmode/socket/udpsocket.cxx

    src/RawSocket/CheckSum.cpp
)

# 链接系统库（原配置正确，保留）
target_link_libraries(tunmode
    log  # 日志库
    android  # Android系统库（比如JNI相关函数）
)
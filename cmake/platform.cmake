if(WIN32)
    add_definitions(-D_WIN32)
    list(APPEND DEPEND_LIBS ws2_32)
endif()

if(WIN32)
    add_definitions(-D_WIN32 -DPTW32_STATIC_LIB)  # 静态链接 pthreads-win32
    list(APPEND DEPEND_LIBS ws2_32 pthreads-win32)  # 添加 pthreads-win32 库
else()
    find_package(Threads REQUIRED)
    list(APPEND DEPEND_LIBS Threads::Threads)  # Linux/macOS 使用原生 pthread
endif()
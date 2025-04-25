# 设置CMake最低版本要求  
cmake_minimum_required(VERSION 3.10)  
  
# 设置项目名称  
project(api_test)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath=.:../lib:./lib")

# 添加编译输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)
  
# 添加源代码文件  
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")  
  
# 添加头文件目录  
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src/api
)

# 编译生成可执行程序
add_executable(${PROJECT_NAME} ${SOURCES})

# 添加需要链接的库文件  
target_link_libraries(${PROJECT_NAME} 
    tcp_udp_communicate
)
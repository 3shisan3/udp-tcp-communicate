# 设置CMake最低版本要求  
cmake_minimum_required(VERSION 3.11)  
  
# 设置项目名称  
project(tcp_udp_communicate)

# 在 Unix-like 系统中使用 $HOME 环境变量  
execute_process(COMMAND sh -c "echo $HOME" OUTPUT_VARIABLE USER_HOME_DIR)  
string(STRIP ${USER_HOME_DIR} USER_HOME_DIR)  

set(CMAKE_INSTALL_PREFIX "${USER_HOME_DIR}/program_running/${PROJECT_NAME}")

# 设置C++标准为
set(CMAKE_CXX_STANDARD 17)

# 导入配置
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/thirddepend.cmake)

# 添加编译输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 添加头文件
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)

# 添加源代码文件  
file(GLOB_RECURSE SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# 编译生成可执行程序
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# 链接 zlib 库
target_link_libraries(${PROJECT_NAME} PRIVATE z)

# 为特定目标添加编译时的宏定义，包括值
target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_NAME="${PROJECT_NAME} ") 
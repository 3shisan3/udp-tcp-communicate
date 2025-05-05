# 设置CMake最低版本要求  
cmake_minimum_required(VERSION 3.11)  
  
# 设置项目名称  
project(tcp_udp_communicate)

# # 在 Unix-like 系统中使用 $HOME 环境变量  
# execute_process(COMMAND sh -c "echo $HOME" OUTPUT_VARIABLE USER_HOME_DIR)  
# string(STRIP ${USER_HOME_DIR} USER_HOME_DIR)  

# set(CMAKE_INSTALL_PREFIX "${USER_HOME_DIR}/program_running/${PROJECT_NAME}")

#设置构建中用到的变量
set(PROJECT_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_HEADER_DIR "")
set(DEPEND_LIBS "")

# 设置构建选项前提
option(DUAL_ENDPOINT_MODE "部署在双端（接收/发送）使用" OFF)
option(THREAD_POOL_MODE "使用线程池处理" OFF)
option(ENABLE_LOGGING "开启日志打印功能" ON)
option(BUILD_TEST "编译测试部分用例"	ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)     # 和 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC") 类似

# 设置C++标准为
set(CMAKE_CXX_STANDARD 17)

# 导入配置
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/third_depend.cmake)

# 添加编译输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 添加头文件
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/logger
)

# 添加基础模板源代码文件  
file(GLOB_RECURSE SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
)

# 编译生成可执行程序
add_library(${PROJECT_NAME} SHARED ${SOURCES})

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/expand_ability.cmake)

# 打印必要信息
message(STATUS "CMAKE_BUILD_TYPE:${CMAKE_BUILD_TYPE}")
message(STATUS "EXTRA_HEADER:${PROJECT_HEADER_DIR}")

# 添加头文件
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_HEADER_DIR}
)

# 链接依赖库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    yaml-cpp
    ${DEPEND_LIBS}
)

# 为特定目标添加编译时的宏定义，包括值
target_compile_definitions(${PROJECT_NAME} PRIVATE PROJECT_NAME=${PROJECT_NAME})

# 测试项目
if (BUILD_TEST)
	message(STATUS "build test modules")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test/api_test ${CMAKE_BINARY_DIR}/api_test)
endif()

# 打包安装
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/install.cmake)
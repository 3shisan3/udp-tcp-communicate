# 两侧均部署该库场景
if(DUAL_ENDPOINT_MODE)
    message(STATUS "Building in DUAL_ENDPOINT_MODE")
    
    # 添加双端模式专用源代码
    file(GLOB_RECURSE DUAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/*.cpp
    )
    target_sources(${PROJECT_NAME} PRIVATE ${DUAL_SOURCES})
    
    # 添加双端模式专用头文件路径
    list(APPEND PROJECT_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/impl)
endif()

# 使用线程池
if (THREAD_POOL_MODE)
    message(STATUS "Building with THREAD_POOL_MODE")

    if (WIN32)
        file(GLOB_RECURSE THREADPOOL_SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool/windows/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool/windows/*.cc
        )
        target_sources(${PROJECT_NAME} PRIVATE ${THREADPOOL_SOURCES})
        list(APPEND PROJECT_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool/windows)
    else()
        file(GLOB_RECURSE THREADPOOL_SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool/pthread/*.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool/pthread/*.cc
        )
        target_sources(${PROJECT_NAME} PRIVATE ${THREADPOOL_SOURCES})
        list(APPEND PROJECT_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool/pthread)
    endif()

    list(APPEND PROJECT_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/expand/threadpool)
endif()

# 添加宏，便于代码中的统一
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<BOOL:${DUAL_ENDPOINT_MODE}>:DUAL_ENDPOINT_MODE>
    $<$<BOOL:${THREAD_POOL_MODE}>:THREAD_POOL_MODE>
)

# 如果是双端模式，添加额外的链接库
if(DUAL_ENDPOINT_MODE)

endif()
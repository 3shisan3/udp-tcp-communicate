# FetchContent 需要 CMake 3.11 或更高版本。
option(FETCHCONTENT_MANAGE_DEPS  "使用 FetchContent 来管理项目依赖" OFF)

if (FETCHCONTENT_MANAGE_DEPS)
  # 包含 FetchContent 模块
  # 内部调用 find_package 风格的导出逻辑：自动导出依赖项的所有目标（包括头文件路径）
  # 即后文cmakefile中不用再手动添加头文件路径，只用添加库文件路径
  include(FetchContent)
  # 设置基础下载目录
  set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps")  # 默认是 `${CMAKE_BINARY_DIR}/_deps`

  # 下载并构建
  FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.7.0
    GIT_SHALLOW TRUE          # 只克隆最新提交（节省时间）
  )
  FetchContent_MakeAvailable(yaml-cpp)

  if(NOT TARGET spdlog AND NOT TARGET spdlog::spdlog)
    FetchContent_Declare(
      spdlog
      GIT_REPOSITORY https://github.com/gabime/spdlog.git
      GIT_TAG v1.15.2
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)
  endif()

  # 基本都是直接包含头文件使用（此处提供项目地址）
  # FetchContent_Declare(
  #   nlohmann_json
  #   GIT_REPOSITORY https://github.com/nlohmann/json.git
  #   GIT_TAG v3.12.0           # 指定版本
  #   GIT_SHALLOW TRUE
  # )
  # FetchContent_MakeAvailable(nlohmann_json)

  # FetchContent_Declare(
  #     zlib
  #     GIT_REPOSITORY https://github.com/madler/zlib.git
  #     GIT_TAG master
  #     GIT_SHALLOW TRUE        # 只克隆最近的历史记录
  # )
  # FetchContent_MakeAvailable(zlib)
else()
  set(YAML_BUILD_SHARED_LIBS ${UDPTCP_BUILD_SHARED})
  add_subdirectory(${PROJECT_ROOT_PATH}/thirdparty/yaml-cpp)
  # list(APPEND PROJECT_HEADER_DIR ${PROJECT_ROOT_PATH}/thirdparty/yaml-cpp/include)

  if(NOT TARGET spdlog AND NOT TARGET spdlog::spdlog)
    set(SPDLOG_BUILD_SHARED ${UDPTCP_BUILD_SHARED}) # 设置spdlog编译为动态库
    add_subdirectory(${PROJECT_ROOT_PATH}/thirdparty/spdlog)
  endif()
endif()

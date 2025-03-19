# FetchContent 需要 CMake 3.11 或更高版本。
# 手动编译导出库和头文件（cmakefile中引用）亦可

# 包含 FetchContent 模块
include(FetchContent)

# 下载并构建 zlib
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG master
)
FetchContent_MakeAvailable(zlib)


cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_C_STANDARD 11)

# 包含 include/ 目錄
include_directories(include)

# 自動收集所有 c 檔案
file(GLOB SRC_FILES src/*.c)

# 生成可執行檔
add_executable(main ${SRC_FILES})

# ---------------------------
# 編譯選項
#   cmake -DCMAKE_BUILD_TYPE=Debug ..
#   cmake -DCMAKE_BUILD_TYPE=Release ..
# ---------------------------

# Debug 編譯選項
set(CMAKE_C_FLAGS_DEBUG "-g -O1 -fsanitize=undefined")

# Release 編譯選項
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
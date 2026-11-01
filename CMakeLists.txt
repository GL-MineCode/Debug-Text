cmake_minimum_required(VERSION 3.16)
project(DebugText LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 通用编译选项（Debug / Release 均生效）
# ============================================================
add_compile_options(
    -fexec-charset=UTF-8
    -fdiagnostics-color=always
    -Wno-narrowing
    -Wno-unused-variable
    -Wno-unknown-pragmas
    -Wno-unused-parameter
    -Wno-conversion-null
    -Wno-missing-field-initializers
    -Wno-sign-compare
    -Wno-subobject-linkage
    -Wno-unused-function
    "-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=."
)

# 区分 Debug 构建与 Release 构建（选取合适的编译参数）
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "[CMake] 构建类型: Release")
    add_compile_options(-O2)
    add_definitions(-DNDEBUG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release")
else()
    message(STATUS "[CMake] 构建类型: Debug（或未指定，默认按 Debug 处理）")
    add_compile_options(-g -O0 -Wall -Wextra)
    add_definitions(-DDEBUG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/debug")
endif()

# 静态链接运行时库，减少对系统 DLL 的依赖
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")

# ============================================================
# 第三方依赖（SDL3 / SDL3_ttf 与公共头文件目录）
# ============================================================
set(SDL_KIT_DIR "D:\\AppInstallers\\RecentlyC++Programs")

# 头文件搜索路径（项目自身的 include 优先）
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL_KIT_DIR}
    ${SDL_KIT_DIR}/SDL3-3.4.14/include
    ${SDL_KIT_DIR}/SDL3_ttf-3.2.2/include
)

# 库文件搜索路径
link_directories(
    ${SDL_KIT_DIR}/SDL3-3.4.14/lib/x64
    ${SDL_KIT_DIR}/SDL3_ttf-3.2.2/lib/x64
)

# 两个程序共用的系统库
# dwmapi / imm32 / comctl32 : SDL_WindowIM.hpp
# comdlg32 / shell32 / gdi32 / ole32 / uuid : GL_Commdlg.hpp
set(PLATFORM_LIBS
    dwmapi
    imm32
    comctl32
    comdlg32
    shell32
    gdi32
    user32
    ole32
    oleaut32
    uuid
)

# 运行期需要的 DLL（构建后复制到可执行文件旁边）
set(SDL_RUNTIME_DLLS
    "${SDL_KIT_DIR}/SDL3-3.4.14/lib/x64/SDL3.dll"
    "${SDL_KIT_DIR}/SDL3_ttf-3.2.2/lib/x64/SDL3_ttf.dll"
)

# ============================================================
# generator —— 一站式字库生成器：TTF -> 字库 header（依赖 SDL3 + SDL3_ttf）
# ============================================================
set(GENERATOR_SOURCES
    src/generator/main.cpp
    src/generator/FontBuilder.cpp
    src/generator/HeaderEmitter.cpp
    src/generator/Preview.cpp
)

add_executable(generator ${GENERATOR_SOURCES})

target_include_directories(generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/generator
)

# 注意：GNU ld 为单遍链接，被依赖的库（SDL3）必须放在依赖它的库之后
target_link_libraries(generator
    SDL3_ttf
    SDL3
    ${PLATFORM_LIBS}
)

add_custom_command(TARGET generator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL_RUNTIME_DLLS} "$<TARGET_FILE_DIR:generator>"
    COMMENT "复制 SDL3 运行库 DLL 到输出目录"
)

# ============================================================
# fonts —— 由 generator 直接生成字库 header（内容不变时不会改写文件）
# ============================================================
set(FONT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/fonts")

set(FONT_HEADERS
    "${FONT_HEADER_DIR}/fusion_pixel_8px_mono_zh_hans.hpp"
    "${FONT_HEADER_DIR}/fusion_pixel_10px_mono_zh_hans.hpp"
    "${FONT_HEADER_DIR}/fusion_pixel_12px_mono_zh_hans.hpp"
)

file(GLOB FONT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/font/*/fusion-pixel-*-monospaced-zh_hans.ttf")

add_custom_command(
    OUTPUT ${FONT_HEADERS}
    COMMAND $<TARGET_FILE:generator> --all --root "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS ${FONT_SOURCES} ${GENERATOR_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/include/DebugText.hpp
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "生成点阵字库 header"
    VERBATIM
)

add_custom_target(fonts ALL DEPENDS ${FONT_HEADERS})
add_dependencies(fonts generator)

# ============================================================
# loadtest —— 位图字体渲染测试（仅依赖 SDL3）
# ============================================================
add_executable(loadtest
    test/loadtest.cpp
    test/odr_compat.cpp
)

target_link_libraries(loadtest
    SDL3
    ${PLATFORM_LIBS}
)

# 只有 GUI 演示程序需要 -mwindows，generator 保持控制台程序
set_target_properties(loadtest PROPERTIES WIN32_EXECUTABLE TRUE)

# 确保字库 header 先于 loadtest 编译
add_dependencies(loadtest fonts)

add_custom_command(TARGET loadtest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SDL_RUNTIME_DLLS} "$<TARGET_FILE_DIR:loadtest>"
    COMMENT "复制 SDL3 运行库 DLL 到输出目录"
)

# Makefile for Debug Font project
# 目标: generator (src/generator.cpp) 和 loadtest (test/loadtest.cpp)

CXX      := C:/Users/Administrator/mingw64/bin/g++.exe
CXXFLAGS := -std=c++20 -O2 -g -Wall -Wextra
INCLUDES := -Iinclude -ISDL2-2.24.0/include -ISDL2_ttf-2.20.1/include
LDFLAGS  := -LSDL2-2.24.0/lib/x64 -LSDL2_ttf-2.20.1/lib/x64
LDLIBS   := -lSDL2main -lSDL2 -lSDL2_ttf -ldwmapi -limm32 -lcomctl32 -lcomdlg32 -lole32 -luuid -mwindows

# 输出目录
BUILD_DIR := build

# 目标文件
TARGETS := $(BUILD_DIR)/generator.exe $(BUILD_DIR)/loadtest.exe

.PHONY: all clean generator loadtest

all: $(TARGETS)

# 确保输出目录存在
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# generator - 字形文件生成器 (依赖 SDL2 + SDL2_ttf)
$(BUILD_DIR)/generator.exe: src/generator.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) $(LDLIBS)

# loadtest - 字体加载测试 (依赖 SDL2, 不含 SDL2_ttf)
$(BUILD_DIR)/loadtest.exe: test/loadtest.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS) -lSDL2main -lSDL2 -ldwmapi -limm32 -lcomctl32 -lcomdlg32 -lole32 -luuid -mwindows

generator: $(BUILD_DIR)/generator.exe

loadtest: $(BUILD_DIR)/loadtest.exe

clean:
	-if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)

# 私有 MP4 帧搜索项目

## 项目简介

本项目使用 C++11 实现一个面向私有 MP4 文件格式的帧搜索程序。

程序支持：

- 输入单个帧号，例如 `42`
- 输入多个离散帧号，例如 `1,8,15`
- 输入闭区间帧号，例如 `10:20`
- 输出目标帧所在的 `group` 偏移和 `block data` 偏移
- 基于合成样本执行正确性测试与性能 benchmark

项目约束参考 `AGENT.md`：

- 编译器：`D:\mingw64\bin\g++.exe`
- C++ 标准：`C++11`
- 构建工具：`D:\CMake\bin\cmake.exe`
- 测试框架：`third_party/gtest-1.8.1`

## 目录说明

- `src/`：核心实现，包括格式解析、查询解析、内存映射、帧定位逻辑
- `tools/`：合成样本生成器与 benchmark 工具
- `tests/`：单元测试与分支覆盖测试
- `docs/`：私有文件格式定义说明
- `build/`：常规构建输出目录
- `build_cov/`：覆盖率构建输出目录

## 合成 MP4 文件会出现在哪里

合成文件不会自动写到固定目录，而是写到你在命令行里传入的第一个路径参数。

### 1. 样本生成器

程序入口见 [generate_synthetic_mp4.cpp](file:///e:/cpp_mp4/tools/generate_synthetic_mp4.cpp#L50-L85)

用法：

```bash
.\build\private_mp4_generate_sample.exe <output_path> [target_frames] [audio_period] [payload_bytes]
```

例如：

```bash
.\build\private_mp4_generate_sample.exe .\build\synthetic_20k.mp4 20000 11 48
```

那么生成的文件会出现在：

```text
e:\cpp_mp4\build\synthetic_20k.mp4
```

### 2. Benchmark 工具

程序入口见 [benchmark_locate_frame.cpp](file:///e:/cpp_mp4/tools/benchmark_locate_frame.cpp#L156-L220)

用法：

```bash
.\build\private_mp4_benchmark.exe <sample_path> [target_frames] [iterations] [probe_window_bytes]
```

注意：

- `benchmark` 会先调用 `WriteSyntheticFile()` 生成样本文件
- 生成位置就是你传入的 `<sample_path>`
- 如果该路径已有同名文件，会被覆盖

例如：

```bash
.\build\private_mp4_benchmark.exe .\build\synthetic_bench.mp4 20000 65536 524288
```

那么 benchmark 会先生成：

```text
e:\cpp_mp4\build\synthetic_bench.mp4
```

然后再 mmap 打开这个文件并执行性能测试。

## 构建方法

### 常规构建

```bash
"D:\CMake\bin\cmake.exe" -S "." -B "build" -G "MinGW Makefiles" "-DCMAKE_CXX_COMPILER=D:/mingw64/bin/g++.exe" "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
"D:\CMake\bin\cmake.exe" --build "build" --config Release -- -j4
```

### 覆盖率构建

```bash
"D:\CMake\bin\cmake.exe" -S "." -B "build_cov" -G "MinGW Makefiles" "-DCMAKE_CXX_COMPILER=D:/mingw64/bin/g++.exe" "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" "-DCMAKE_BUILD_TYPE=Debug" "-DENABLE_COVERAGE=ON"
"D:\CMake\bin\cmake.exe" --build "build_cov" --config Debug --target private_mp4_tests -- -j4
```

## 运行方法

### 1. 帧搜索程序

程序入口见 [main.cpp](file:///e:/cpp_mp4/src/main.cpp)

示例：

```bash
.\build\private_mp4_search.exe "e:\path\to\sample.mp4" "12,24,36:40"
```

程序会输出每个目标帧的：

- 是否找到
- `group` 起始帧信息
- `group` 文件偏移
- `block header` 偏移
- `block data` 偏移
- 单次查询耗时 `lookup_us`

### 2. 运行单元测试

```bash
"D:\CMake\bin\ctest.exe" --test-dir "build" --output-on-failure -C Release
```

### 3. 运行覆盖率测试

```bash
"D:\CMake\bin\ctest.exe" --test-dir "build_cov" --output-on-failure -C Debug
```

如果想直接看具体卡在哪条测试，可以直接运行：

```bash
.\build_cov\private_mp4_tests.exe --gtest_color=no
```

## 已知说明

- `benchmark` 的样本文件属于临时测试数据，建议放在 `build/` 或 `build_cov/` 目录下，避免污染源码目录
- 覆盖率测试相比 Release 构建更慢，因为使用了 `--coverage` 和 `Debug`
- 合成样本仅用于验证算法正确性、边界条件和性能趋势，不等价于真实私有 MP4 全量数据分布

## 相关源码入口

- 核心查询接口：[frame_locator.h](file:///e:/cpp_mp4/src/frame_locator.h)
- 内部定位辅助逻辑：[frame_locator_internal.cpp](file:///e:/cpp_mp4/src/frame_locator_internal.cpp)
- 帧表达式解析：[frame_query.cpp](file:///e:/cpp_mp4/src/frame_query.cpp)
- Windows 内存映射：[mapped_file_win32.cpp](file:///e:/cpp_mp4/src/mapped_file_win32.cpp)
- 合成样本支持：[synthetic_mp4_support.cpp](file:///e:/cpp_mp4/tools/synthetic_mp4_support.cpp)

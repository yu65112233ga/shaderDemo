# Shader Demo

## 项目概述

这是一个基于ANGLE (Almost Native Graphics Layer Engine) 的OpenGL ES 2.0着色器演示程序，用于展示和播放图像序列，并应用着色器效果。该项目在Windows环境下运行，使用CMake构建系统和MSVC编译器。

## 主要功能

- 从指定目录加载图像序列
- 使用OpenGL ES 2.0着色器渲染图像
- 提供简单的播放控制功能：播放/暂停、前进、后退
- 使用ANGLE库在Windows平台上实现OpenGL ES 2.0

## 项目结构

```
/
├── CMakeLists.txt       # CMake构建配置文件
├── main.cpp             # 主程序入口，包含窗口创建和应用逻辑
├── reader/              # 图像加载相关代码
│   ├── ImageLoader.h    # 图像加载器头文件
│   └── ImageLoader.cpp  # 图像加载器实现
├── render/              # 渲染相关代码
│   ├── Renderer.h       # 渲染器头文件
│   └── Renderer.cpp     # 渲染器实现，包含着色器和OpenGL逻辑
├── photo/               # 存放要加载的图像序列
└── thirdparty/          # 第三方库
    └── angle/           # ANGLE库
        ├── include/     # ANGLE头文件
        └── libs/        # ANGLE库文件
    └── stb/             # stb库
```

## 编译指南

### 先决条件

- Windows操作系统
- CMake (3.10或更高版本)
- Visual Studio 2019或更高版本 (MSVC编译器)
- ANGLE库 (已包含在thirdparty目录中)

### 使用CMake和MSVC编译

1. 创建构建目录：

```powershell
mkdir -p build
cd build
```

2. 配置CMake项目：

```powershell
cmake -G "Visual Studio 17 2022" -A x64 ..
```

3. 构建项目：

```powershell
cmake --build . --config Release
```

或者直接在Visual Studio中打开生成的解决方案文件并构建。

## 使用方法

1. 将图像序列放置在`photo`目录中
2. 运行编译好的程序
3. 使用以下键盘控制：
   - 空格键：暂停/继续播放
   - 左箭头：前一帧
   - 右箭头：后一帧

## 技术细节

- 使用ANGLE库将OpenGL ES 2.0 API转换为DirectX，以在Windows上获得更好的兼容性
- 使用EGL创建渲染上下文
- 实现简单的顶点和片段着色器进行图像渲染
- 多线程设计：UI线程处理窗口消息，渲染线程负责OpenGL渲染
- 使用STB库加载图像文件

## 更新日志

### 2025-03-30
- 初始项目设置，使用CMake构建系统和MSVC编译器
- 集成ANGLE库，实现OpenGL ES 2.0在Windows平台上的渲染
- 实现基本的图像加载功能，支持从目录中读取图像序列
- 实现基本的OpenGL渲染功能，包括纹理加载和渲染
- 添加简单的播放控制功能：播放/暂停、前进、后退
- 实现基础的顶点和片段着色器，用于图像渲染
- 添加性能监控功能，包括帧率统计

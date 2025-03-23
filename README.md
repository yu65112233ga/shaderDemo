# Shader Demo

## 项目概述

这是一个基于OpenGL ES的图像渲染演示程序，可以加载并以幻灯片方式显示PNG图像。该程序使用ANGLE库在Windows环境下提供OpenGL ES支持，并提供了交互式播放控制功能。

## 主要功能

- **图像加载**：从指定目录加载PNG图像
- **有序显示**：按照文件名数字顺序（适用于四位数字命名的图片）显示图像
- **播放控制**：
  - 空格键：暂停/继续播放
  - 左方向键：回退一帧（暂停状态下）
  - 右方向键：前进一帧（暂停状态下）
- **平滑渲染**：以每秒30帧的速率显示图像

## 技术实现

### 核心组件

1. **ImageLoader**：
   - 负责加载和管理图像数据
   - 使用`std::map`和自定义比较器实现按数字顺序排序
   - 支持PNG格式图像的加载和处理

2. **渲染引擎**：
   - 使用OpenGL ES 2.0进行图像渲染
   - 通过EGL创建和管理渲染上下文
   - 实现了基本的着色器程序用于纹理渲染

3. **窗口和输入处理**：
   - 使用Windows API创建和管理应用程序窗口
   - 处理键盘输入以实现播放控制功能

### 技术栈

- C++17
- OpenGL ES 2.0
- ANGLE库
- EGL
- Windows API
- STB Image库（用于图像加载）

## 构建与运行

### 环境要求

- Windows操作系统
- Visual Studio（支持C++17）
- CMake 3.10或更高版本

### 构建步骤

1. 克隆或下载项目代码
2. 使用CMake生成Visual Studio项目文件：

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

3. 使用Visual Studio打开生成的解决方案文件或直接构建：

```powershell
cmake --build . --config Release
```

4. 运行生成的可执行文件

### 使用说明

1. 将PNG图像放置在`photo`目录中（可在`main.cpp`中修改路径）
2. 推荐使用四位数字命名图片（如`0001.png`、`0002.png`等）以确保正确排序
3. 运行程序后，图像将以每秒30帧的速率自动播放
4. 使用空格键暂停/继续播放
5. 在暂停状态下，使用左右方向键进行逐帧浏览

## 项目结构

- `main.cpp`：主程序入口，包含窗口创建、渲染循环和播放控制逻辑
- `ImageLoader.h/cpp`：图像加载和管理类
- `thirdparty/`：第三方库（ANGLE、STB Image等）
- `photo/`：存放图像文件的目录

## 扩展与定制

- 可以修改`main.cpp`中的`WINDOW_WIDTH`和`WINDOW_HEIGHT`常量来调整窗口大小
- 可以调整`frameTime`变量来更改播放帧率
- 可以在`ImageLoadOptions`中修改`maxImages`来限制加载的图像数量

## 注意事项

- 程序默认从`E:\code\shaderDemo\photo`目录加载图像
- 图像应为PNG格式，支持RGB和RGBA（带透明通道）
- 为获得最佳性能，建议使用分辨率适中的图像
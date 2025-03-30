#pragma once

#include <iostream>
#include <angle_gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include "../reader/ImageLoader.h"

class Renderer {
public:
    Renderer(HWND hWnd, int width, int height, ImageLoader& imageLoader);
    ~Renderer();

    // Start the rendering loop in a separate thread
    bool start();
    // Stop the rendering loop
    void stop();

    // Playback control
    void togglePause();
    void stepForward();
    void stepBackward();
    bool isPaused() const;

private:
    // Window properties
    HWND hWnd;
    int width;
    int height;
    
    // Image loader reference
    ImageLoader& imageLoader;
    std::vector<std::string> imageNames;
    size_t currentImageIndex;

    // Playback control variables
    std::atomic<bool> paused;
    std::atomic<bool> shouldStepForward;
    std::atomic<bool> shouldStepBackward;
    std::atomic<bool> running;

    // EGL variables
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;

    // Shader program and texture variables
    GLuint shaderProgram;
    GLuint texture;
    
    // Uniform location
    GLint uTextureLocation;
    
    // Performance measurement
    std::chrono::high_resolution_clock::time_point frameStartTime;
    std::chrono::high_resolution_clock::time_point frameEndTime;
    std::chrono::steady_clock::time_point lastFrameTimePoint;
    double lastFrameTime; // in milliseconds
    
    // Frame statistics
    int frameCount;
    double totalRenderTime; // Total render time in milliseconds
    const int statsResetInterval = 60; // Reset statistics every 100 frames

    // Shader sources
    const char* vertexShaderSource = R"(
        attribute vec4 aPosition;
        attribute vec2 aTexCoord;
        varying vec2 vTexCoord;
        void main() {
            gl_Position = aPosition;
            vTexCoord = aTexCoord;
        }
    )";

    const char* fragmentShaderSourceOld = R"(
        precision mediump float;
        varying vec2 vTexCoord;
        uniform sampler2D uTexture;
        void main() {
            gl_FragColor = texture2D(uTexture, vTexCoord);
        }
    )";

    const char* fragmentShaderSourcePart1 = R"(
        precision highp float;
        varying vec2 vTexCoord;
        uniform sampler2D uTexture;
        
        // 常量定义，用于增加计算复杂度
        #define PI 3.14159265359
        #define E 2.71828182846
        #define PHI 1.61803398875
        #define MAX_ITERATIONS 15
        
        void main() {
            vec4 texColor = texture2D(uTexture, vTexCoord);
            
            // 基础计算值
            float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
            float colorSum = texColor.r + texColor.g + texColor.b;
            float colorDiff = max(max(texColor.r, texColor.g), texColor.b) - min(min(texColor.r, texColor.g), texColor.b);
            
            // 创建条件变量，增加分支复杂性
            float condition1 = luminance > 0.5 ? 1.0 : 0.0;
            float condition2 = texColor.r > texColor.g ? 1.0 : 0.0;
            float condition3 = texColor.g > texColor.b ? 1.0 : 0.0;
            float condition4 = texColor.b > texColor.r ? 1.0 : 0.0;
            float condition5 = vTexCoord.x > 0.5 ? 1.0 : 0.0;
            
            // 计算临时变量
            float temp1 = sin(texColor.r * PI) * cos(texColor.g * PI);
            
            // 复杂的嵌套分支结构
            if (condition1 > 0.5) { // 第1层
                if (condition2 > 0.5) { // 第2层
                    if (condition3 > 0.5) { // 第3层
                        if (condition5 > 0.5) { // 第4层
                            if (temp1 > 0.0) { // 第5层
                                // 分支1: 计算密集型操作
                                float result = 0.0;
                                for (int i = 0; i < MAX_ITERATIONS; i++) {
                                    float t = float(i) / float(MAX_ITERATIONS);
                                    result += sin(t * PI * texColor.r) * cos(t * E * texColor.g);
                                }
                                texColor.r = mix(texColor.r, result * 0.5 + 0.5, 0.7);
                                texColor.g *= 1.2;
                                texColor.b *= 0.8;
                            } else {
                                // 分支2: 不同的矩阵变换
                                float angle = -luminance * PI;
                                float c = cos(angle);
                                float s = sin(angle);
                                float newR = texColor.r;
                                float newG = c * texColor.g - s * texColor.b;
                                float newB = s * texColor.g + c * texColor.b;
                                texColor.r = mix(texColor.r, newR, 0.6);
                                texColor.g = mix(texColor.g, newG, 0.6);
                                texColor.b = mix(texColor.b, newB, 0.6);
                            }
                        } else {
                            // 分支3: 缩放变换
                            float scaleX = texColor.r + 0.5;
                            float scaleY = texColor.g + 0.5;
                            float scaleZ = texColor.b + 0.5;
                            texColor.r *= scaleX;
                            texColor.g *= scaleY;
                            texColor.b *= scaleZ;
                            texColor = clamp(texColor, 0.0, 1.0);
                        }
                    } else {
                        // 分支4: 自定义矩阵变换
                        float m11 = texColor.r;
                        float m12 = texColor.g * 0.5;
                        float m21 = texColor.g * 0.5;
                        float m22 = texColor.b;
                        
                        float newR = m11 * texColor.r + m12 * texColor.g;
                        float newG = m21 * texColor.r + m22 * texColor.g;
                        
                        texColor.r = mix(texColor.r, newR, 0.7);
                        texColor.g = mix(texColor.g, newG, 0.7);
                    }
                } else {
    )";

    const char* fragmentShaderSourcePart2 = R"(
                    // 分支5: 基于颜色差异的变换
                    if (condition4 > 0.5) {
                        // 分支6: 旋转变换
                        float angle = colorDiff * PI * 4.0;
                        float c = cos(angle);
                        float s = sin(angle);
                        float newR = c * texColor.r - s * texColor.b;
                        float newG = texColor.g;
                        float newB = s * texColor.r + c * texColor.b;
                        texColor.r = mix(texColor.r, newR, 0.6);
                        texColor.g = mix(texColor.g, newG, 0.6);
                        texColor.b = mix(texColor.b, newB, 0.6);
                    } else {
                        // 分支7: 复杂的颜色混合
                        float weight = colorSum * 0.5;
                        vec3 targetColor = vec3(1.0 - texColor.r, 1.0 - texColor.g, 1.0 - texColor.b);
                        texColor.rgb = mix(texColor.rgb, targetColor, weight);
                    }
                }
            } else {
                // 第1层的另一分支
                if (colorDiff > 0.3) {
                    // 分支8: 基于颜色差异的复杂处理
                    float result = 0.0;
                    for (int i = 0; i < MAX_ITERATIONS; i++) {
                        float t = float(i) / float(MAX_ITERATIONS);
                        result += sin(t * PI * colorDiff) * cos(t * E * luminance);
                    }
                    texColor.r = mix(texColor.r, result * 0.5 + 0.5, 0.7);
                    texColor.g *= 0.9;
                    texColor.b *= 1.1;
                } else {
                    // 分支9: 基于纹理坐标的复杂处理
                    float angle = vTexCoord.x * PI * 2.0;
                    float scale = vTexCoord.y + 0.5;
                    float c = cos(angle);
                    float s = sin(angle);
                    float newR = scale * (c * texColor.r - s * texColor.g);
                    float newG = scale * (s * texColor.r + c * texColor.g);
                    float newB = texColor.b;
                    texColor.r = mix(texColor.r, newR, 0.8);
                    texColor.g = mix(texColor.g, newG, 0.8);
                    texColor.b = mix(texColor.b, newB, 0.8);
                }
            }
            
            // 额外的颜色处理层 - 基于颜色通道的排序
            if (texColor.r > texColor.g) {
                if (texColor.g > texColor.b) {
                    // R > G > B
                    texColor.r = texColor.r * 1.1;
                    texColor.g = texColor.g * 0.95;
                    texColor.b = texColor.b * 0.9;
                } else if (texColor.r > texColor.b) {
                    // R > B > G
                    texColor.r = texColor.r * 1.05;
                    texColor.g = texColor.g * 0.9;
                    texColor.b = texColor.b * 0.95;
                } else {
                    // B > R > G
                    texColor.r = texColor.r * 0.95;
                    texColor.g = texColor.g * 0.9;
                    texColor.b = texColor.b * 1.05;
                }
            } else {
                if (texColor.r > texColor.b) {
                    // G > R > B
                    texColor.r = texColor.r * 0.95;
                    texColor.g = texColor.g * 1.05;
                    texColor.b = texColor.b * 0.9;
                } else if (texColor.g > texColor.b) {
                    // G > B > R
                    texColor.r = texColor.r * 0.9;
                    texColor.g = texColor.g * 1.05;
                    texColor.b = texColor.b * 0.95;
                } else {
                    // B > G > R
                    texColor.r = texColor.r * 0.9;
                    texColor.g = texColor.g * 0.95;
                    texColor.b = texColor.b * 1.05;
                }
            }
            
            // 最后一层处理 - 基于纹理坐标的网格效果
            float gridX = step(0.1, fract(vTexCoord.x * 10.0));
            float gridY = step(0.1, fract(vTexCoord.y * 10.0));
            
            if (gridX < 0.5 && gridY < 0.5) {
                // 网格交叉点
                texColor.rgb *= 0.8;
            } else if (gridX < 0.5) {
                // 垂直线
                texColor.r *= 0.9;
            } else if (gridY < 0.5) {
                // 水平线
                texColor.g *= 0.9;
            }
            
            // 确保颜色在有效范围内
            texColor = clamp(texColor, 0.0, 1.0);
            gl_FragColor = texColor;
        }
    )";

    // 组合着色器代码片段
    const char* fragmentShaderSource = fragmentShaderSourcePart1;
    const char* fragmentShaderSourcePart2Ptr = fragmentShaderSourcePart2;

    // Private methods
    void renderLoop();
    bool initializeGL();
    void cleanupGL();
    GLuint compileShader(GLenum type, const char* source);
    GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);
    void updateTexture();
    void renderTexturedQuad();
    void nextFrame();
    void previousFrame();

    // Helper functions
    void checkEGLError(const char* msg);
    void checkGLError(const char* msg);
};

﻿#pragma once

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

    // 组合着色器代码片段
//    const char* fragmentShaderSource = fragmentShaderSourcePart1;
//    const char* fragmentShaderSourcePart2Ptr = fragmentShaderSourcePart2;
     const char* fragmentShaderSource = fragmentShaderSourceOld;
     const char* fragmentShaderSourcePart2Ptr = nullptr;

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

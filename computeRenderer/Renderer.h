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
    GLuint computeShaderProgram;
    GLuint renderShaderProgram;
    GLuint inputTexture;  // Input texture containing the image
    GLuint outputTexture; // Output texture for compute shader results
    GLuint framebuffer;   // Framebuffer for rendering the final result
    
    // Uniform locations
    GLint uInputTextureLocation;
    GLint uOutputTextureLocation;
    
    // Performance measurement
    std::chrono::high_resolution_clock::time_point frameStartTime;
    std::chrono::high_resolution_clock::time_point frameEndTime;
    std::chrono::steady_clock::time_point lastFrameTimePoint;
    double lastFrameTime; // in milliseconds
    
    // Frame statistics
    int frameCount;
    double totalRenderTime; // Total render time in milliseconds
    const int statsResetInterval = 60; // Reset statistics every 60 frames

    // Shader sources
    // Simple vertex and fragment shaders for final display
    const char* vertexShaderSource = R"(
        attribute vec4 aPosition;
        attribute vec2 aTexCoord;
        varying vec2 vTexCoord;
        void main() {
            gl_Position = aPosition;
            vTexCoord = aTexCoord;
        }
    )";

    const char* fragmentShaderSource = R"(
        precision mediump float;
        varying vec2 vTexCoord;
        uniform sampler2D uTexture;
        void main() {
            gl_FragColor = texture2D(uTexture, vTexCoord);
        }
    )";

    // Compute shader source
    const char* computeShaderSource = R"(
        #version 310 es
        layout(local_size_x = 16, local_size_y = 16) in;
        layout(binding = 0, rgba8) uniform readonly highp image2D inputImage;
        layout(binding = 1, rgba8) uniform writeonly highp image2D outputImage;
        
        void main() {
            // Get the pixel coordinate
            ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
            
            // Read the input pixel
            vec4 texColor = imageLoad(inputImage, pixelCoord);
            
            // Basic image processing - you can add more complex logic here
            float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
            
            // Apply some effects based on pixel position and luminance
            if (luminance > 0.5) {
                // Brighten bright areas
                texColor.rgb *= 1.2;
            } else {
                // Apply a color tint to dark areas
                texColor.r *= 0.8;
                texColor.g *= 0.9;
                texColor.b *= 1.1;
            }
            
            // Ensure values are in valid range
            texColor = clamp(texColor, 0.0, 1.0);
            
            // Write the output pixel
            imageStore(outputImage, pixelCoord, texColor);
        }
    )";

    // Private methods
    void renderLoop();
    bool initializeGL();
    void cleanupGL();
    GLuint compileShader(GLenum type, const char* source);
    GLuint createComputeShaderProgram(const char* computeSource);
    GLuint createRenderShaderProgram(const char* vertexSource, const char* fragmentSource);
    void updateTexture();
    void processImageWithCompute();
    void renderProcessedImage();
    void nextFrame();
    void previousFrame();

    // Helper functions
    void checkEGLError(const char* msg);
    void checkGLError(const char* msg);
};

#include "Renderer.h"

Renderer::Renderer(HWND hWnd, int width, int height, ImageLoader& imageLoader)
    : hWnd(hWnd), width(width), height(height), imageLoader(imageLoader),
      currentImageIndex(0), paused(false), shouldStepForward(false),
      shouldStepBackward(false), running(false),
      display(EGL_NO_DISPLAY), config(nullptr), context(EGL_NO_CONTEXT),
      surface(EGL_NO_SURFACE), computeShaderProgram(0), renderShaderProgram(0), 
      inputTexture(0), outputTexture(0), framebuffer(0), lastFrameTime(0.0),
      frameCount(0), totalRenderTime(0.0), lastFrameTimePoint(std::chrono::steady_clock::now()),
      frameStartTime(std::chrono::steady_clock::now()), frameEndTime(std::chrono::steady_clock::now()), statsResetInterval(60) {
    // Get all loaded image names
    imageNames = imageLoader.getImageNames();
}

Renderer::~Renderer() {
    stop();
}

bool Renderer::start() {
    if (running) {
        return true; // Already running
    }

    // Get EGL display
    display = eglGetDisplay(GetDC(hWnd));
    if (display == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display" << std::endl;
        return false;
    }
    
    // Initialize EGL
    EGLint majorVersion, minorVersion;
    if (!eglInitialize(display, &majorVersion, &minorVersion)) {
        std::cerr << "Failed to initialize EGL" << std::endl;
        checkEGLError("eglInitialize");
        return false;
    }
    
    std::cout << "EGL Version: " << majorVersion << "." << minorVersion << std::endl;
    
    // Print EGL client APIs
    const char* apis = eglQueryString(display, EGL_CLIENT_APIS);
    std::cout << "EGL Client APIs: " << (apis ? apis : "<null>") << std::endl;
    
    // Print EGL extensions
    const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
    std::cout << "EGL Extensions: " << (extensions ? extensions : "<null>") << std::endl;
    
    // EGL configuration attributes
    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, // Use ES 3.0 for compute shaders
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    // Choose EGL configuration
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs <= 0) {
        std::cerr << "Failed to choose EGL configuration" << std::endl;
        checkEGLError("eglChooseConfig");
        eglTerminate(display);
        return false;
    }
    
    // Create window surface
    surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)hWnd, NULL);
    if (surface == EGL_NO_SURFACE) {
        std::cerr << "Failed to create EGL surface" << std::endl;
        checkEGLError("eglCreateWindowSurface");
        eglTerminate(display);
        return false;
    }
    
    // EGL context attributes for ES 3.0
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3, // Request ES 3.0 context
        EGL_NONE
    };
    
    // Create EGL context
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        std::cerr << "Failed to create EGL context" << std::endl;
        checkEGLError("eglCreateContext");
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }
    
    // 在主线程中初始化OpenGL资源
    if (!initializeGL()) {
        std::cerr << "Failed to initialize OpenGL resources" << std::endl;
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }
    
    // 确保主线程不再使用EGL上下文
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglReleaseThread();
    
    // Start render loop in a new thread
    running = true;
    std::thread renderThread(&Renderer::renderLoop, this);
    renderThread.detach(); // Detach the thread so it runs independently

    std::cout << "Compute Renderer started" << std::endl;
    std::cout << "Controls: Space = Pause/Resume, Left Arrow = Previous Frame, Right Arrow = Next Frame" << std::endl;
    
    return true;
}

void Renderer::stop() {
    if (!running) {
        return;
    }

    // Signal the render loop to stop
    running = false;
    
    // Allow the thread to exit gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Clean up OpenGL resources
    if (display != EGL_NO_DISPLAY) {
        // 不需要在这里调用eglMakeCurrent，因为渲染线程会自己解绑
        
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
            context = EGL_NO_CONTEXT;
        }
        
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
            surface = EGL_NO_SURFACE;
        }
        
        eglTerminate(display);
        display = EGL_NO_DISPLAY;
    }

    std::cout << "Renderer stopped" << std::endl;
}

void Renderer::togglePause() {
    paused = !paused;
    std::cout << (paused ? "Playback paused" : "Playback resumed") << std::endl;
}

void Renderer::stepForward() {
    if (paused) {
        shouldStepForward = true;
        std::cout << "Step forward" << std::endl;
    }
}

void Renderer::stepBackward() {
    if (paused) {
        shouldStepBackward = true;
        std::cout << "Step backward" << std::endl;
    }
}

bool Renderer::isPaused() const {
    return paused;
}

void Renderer::renderLoop() {
    // 在渲染线程中绑定 EGL context
    if (!eglMakeCurrent(display, surface, surface, context)) {
        std::cerr << "Failed to bind EGL context in render thread" << std::endl;
        checkEGLError("eglMakeCurrent in renderLoop");
        running = false;
        return;
    }
    
    // 设置视口
    glViewport(0, 0, width, height);
    
    // 加载初始纹理
    updateTexture();
    
    // Frame timing variables for 30 FPS
    const std::chrono::milliseconds frameTime(33); // ~30 FPS (1000ms / 30 = 33.33ms)
    
    while (running) {
        // Handle single step controls
        if (shouldStepForward) {
            nextFrame();
            shouldStepForward = false;
        } else if (shouldStepBackward) {
            previousFrame();
            shouldStepBackward = false;
        }
        
        // Measure time since last frame
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastFrameTimePoint);
        
        // Only render a new frame if not paused or if we're stepping
        if (!paused || shouldStepForward || shouldStepBackward) {
            // Process the current image with compute shader
            processImageWithCompute();
            
            // Render the processed image to the screen
            renderProcessedImage();
            
            // Swap buffers
            eglSwapBuffers(display, surface);
            
            // If not paused, advance to next frame based on timing
            if (!paused && elapsedTime >= frameTime) {
                nextFrame();
                lastFrameTimePoint = currentTime;
            }
        }
        
        // Sleep to avoid excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // Clean up OpenGL resources
    cleanupGL();
    
    // Unbind EGL context
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglReleaseThread();
}

bool Renderer::initializeGL() {
    // 在主线程中临时绑定 EGL context 以初始化 OpenGL 资源
    if (!eglMakeCurrent(display, surface, surface, context)) {
        std::cerr << "Failed to bind EGL context in initializeGL" << std::endl;
        checkEGLError("eglMakeCurrent in initializeGL");
        return false;
    }
    
    // Create compute shader program
    computeShaderProgram = createComputeShaderProgram(computeShaderSource);
    if (!computeShaderProgram) {
        std::cerr << "Failed to create compute shader program" << std::endl;
        return false;
    }
    
    // Create render shader program
    renderShaderProgram = createRenderShaderProgram(vertexShaderSource, fragmentShaderSource);
    if (!renderShaderProgram) {
        std::cerr << "Failed to create render shader program" << std::endl;
        return false;
    }
    
    // Get uniform locations
    glUseProgram(renderShaderProgram);
    uOutputTextureLocation = glGetUniformLocation(renderShaderProgram, "uTexture");
    
    // Create input texture
    glGenTextures(1, &inputTexture);
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Create output texture
    glGenTextures(1, &outputTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Allocate storage for output texture (will be resized when updateTexture is called)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    checkGLError("initializeGL");
    
    // 初始化完成后解绑 context，让渲染线程去绑定
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    
    return true;
}

void Renderer::cleanupGL() {
    glDeleteTextures(1, &inputTexture);
    glDeleteTextures(1, &outputTexture);
    glDeleteProgram(computeShaderProgram);
    glDeleteProgram(renderShaderProgram);
    glDeleteFramebuffers(1, &framebuffer);
}

GLuint Renderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        std::cerr << "Failed to create shader" << std::endl;
        return 0;
    }
    
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog.data());
            std::cerr << "Error compiling shader: " << infoLog.data() << std::endl;
        }
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint Renderer::createComputeShaderProgram(const char* computeSource) {
    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);
    if (!computeShader) {
        return 0;
    }
    
    GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "Failed to create program" << std::endl;
        glDeleteShader(computeShader);
        return 0;
    }
    
    glAttachShader(program, computeShader);
    glLinkProgram(program);
    
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(program, infoLen, NULL, infoLog.data());
            std::cerr << "Error linking program: " << infoLog.data() << std::endl;
        }
        glDeleteProgram(program);
        glDeleteShader(computeShader);
        return 0;
    }
    
    // Shader can be detached and deleted after linking
    glDetachShader(program, computeShader);
    glDeleteShader(computeShader);
    
    return program;
}

GLuint Renderer::createRenderShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader) {
        return 0;
    }
    
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "Failed to create program" << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            std::vector<char> infoLog(infoLen);
            glGetProgramInfoLog(program, infoLen, NULL, infoLog.data());
            std::cerr << "Error linking program: " << infoLog.data() << std::endl;
        }
        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    // Shaders can be detached and deleted after linking
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return program;
}

void Renderer::updateTexture() {
    if (imageNames.empty()) {
        return;
    }
    
    // Get the current image data
    const ImageData* imageData = imageLoader.getImage(imageNames[currentImageIndex]);
    if (!imageData || !imageData->isValid()) {
        std::cerr << "Invalid image data for " << imageNames[currentImageIndex] << std::endl;
        return;
    }
    
    // Bind the input texture
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    
    // Determine the format based on the number of channels
    GLenum format;
    switch (imageData->channels) {
        case 1: format = GL_LUMINANCE; break;
        case 3: format = GL_RGB; break;
        case 4: format = GL_RGBA; break;
        default:
            std::cerr << "Unsupported number of channels: " << imageData->channels << std::endl;
            return;
    }
    
    // Upload the image data to the texture
    glTexImage2D(GL_TEXTURE_2D, 0, format, imageData->width, imageData->height, 0,
                 format, GL_UNSIGNED_BYTE, imageData->data.data());
    
    // Resize the output texture to match the input texture
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageData->width, imageData->height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    checkGLError("updateTexture");
}

void Renderer::processImageWithCompute() {
    if (!computeShaderProgram || !inputTexture || !outputTexture) {
        return;
    }
    
    // Start timing this frame
    frameStartTime = std::chrono::high_resolution_clock::now();
    
    // Use the compute shader program
    glUseProgram(computeShaderProgram);
    
    // Bind the input texture to image unit 0
    glBindImageTexture(0, inputTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    
    // Bind the output texture to image unit 1
    glBindImageTexture(1, outputTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    
    // Get the image dimensions
    GLint width, height;
    glBindTexture(GL_TEXTURE_2D, inputTexture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    
    // Dispatch the compute shader
    // We use work groups of 16x16, so we need to calculate how many groups we need
    GLuint numGroupsX = (width + 15) / 16;
    GLuint numGroupsY = (height + 15) / 16;
    
    // 强制 GPU 同步以确保准确的性能测量
    glFinish();
    
    // Dispatch compute shader
    glDispatchCompute(numGroupsX, numGroupsY, 1);
    
    // Make sure writing to the image has finished before we use it
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    
    // 确保所有计算完成
    glFinish();
    
    checkGLError("processImageWithCompute");
}

void Renderer::renderProcessedImage() {
    if (!renderShaderProgram || !outputTexture) {
        return;
    }
    
    // Use the render shader program
    glUseProgram(renderShaderProgram);
    
    // Get attribute locations
    GLint positionLoc = glGetAttribLocation(renderShaderProgram, "aPosition");
    GLint texCoordLoc = glGetAttribLocation(renderShaderProgram, "aTexCoord");
    
    // Vertex data for a quad (x, y, z, u, v)
    const float vertices[] = {
        // Positions    // Texture Coords
        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,  // Top-left
         1.0f,  1.0f, 0.0f,  1.0f, 0.0f,  // Top-right
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,  // Bottom-left
         1.0f, -1.0f, 0.0f,  1.0f, 1.0f   // Bottom-right
    };
    
    // Enable vertex attributes
    glEnableVertexAttribArray(positionLoc);
    glEnableVertexAttribArray(texCoordLoc);
    
    // Set vertex attribute pointers
    glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), vertices + 3);
    
    // Set active texture and uniform
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glUniform1i(uOutputTextureLocation, 0);
    
    // Draw the quad
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // 结束渲染计时
    auto renderEndTime = std::chrono::high_resolution_clock::now();
    double renderTime = std::chrono::duration<double, std::milli>(renderEndTime - frameStartTime).count();
    
    // Disable vertex attributes
    glDisableVertexAttribArray(positionLoc);
    glDisableVertexAttribArray(texCoordLoc);
    
    // Unbind shader program
    glUseProgram(0);
    
    // End timing this frame
    frameEndTime = std::chrono::high_resolution_clock::now();
    lastFrameTime = std::chrono::duration<double, std::milli>(frameEndTime - frameStartTime).count();
    
    // Update frame statistics
    frameCount++;
    totalRenderTime += lastFrameTime;

    // Check if we need to report and reset statistics
    if (frameCount >= statsResetInterval) {
        double averageRenderTime = totalRenderTime / frameCount;
        std::cout << "\n===== Performance Statistics =====" << std::endl;
        std::cout << "Frames rendered: " << frameCount << std::endl;
        std::cout << "Total render time: " << totalRenderTime << " ms" << std::endl;
        std::cout << "Average render time per iteration: " << averageRenderTime << " ms" << std::endl;
        std::cout << "================================\n" << std::endl;
        
        // Reset statistics
        frameCount = 0;
        totalRenderTime = 0.0;
    }
    
    checkGLError("renderProcessedImage");
}

void Renderer::nextFrame() {
    if (imageNames.empty()) {
        return;
    }
    
    // Move to the next image
    currentImageIndex = (currentImageIndex + 1) % imageNames.size();
    
    // Update the texture with the new image
    updateTexture();
}

void Renderer::previousFrame() {
    if (imageNames.empty()) {
        return;
    }
    
    // Move to the previous image
    if (currentImageIndex == 0) {
        currentImageIndex = imageNames.size() - 1;
    } else {
        currentImageIndex--;
    }
    
    // Update the texture with the new image
    updateTexture();
}

void Renderer::checkEGLError(const char* msg) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        std::cerr << "EGL error at " << msg << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

void Renderer::checkGLError(const char* msg) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "GL error at " << msg << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

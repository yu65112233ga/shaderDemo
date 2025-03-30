#include "Renderer.h"

Renderer::Renderer(HWND hWnd, int width, int height, ImageLoader& imageLoader)
    : hWnd(hWnd), width(width), height(height), imageLoader(imageLoader),
      currentImageIndex(0), paused(false), shouldStepForward(false),
      shouldStepBackward(false), running(false),
      display(EGL_NO_DISPLAY), config(nullptr), context(EGL_NO_CONTEXT),
      surface(EGL_NO_SURFACE), shaderProgram(0), texture(0), lastFrameTime(0.0),
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
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
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
    
    // EGL context attributes
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
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

    std::cout << "Renderer started" << std::endl;
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
        
        // Calculate time since last frame
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastFrameTimePoint);
        
        // Only render if enough time has passed (for 30 FPS) and not paused
        if (elapsedTime.count() >= frameTime.count() && !paused) {
            // Update texture with next image
            nextFrame();
            
            // Update last frame time
            lastFrameTimePoint = currentTime;
        }
        
        // Always render the current frame
        // Clear the screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render the textured quad
        renderTexturedQuad();
        
        // Swap buffers
        eglSwapBuffers(display, surface);
        
        // Sleep to avoid excessive CPU usage
        if (paused) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    // 在线程结束前解绑 EGL context
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool Renderer::initializeGL() {
    // 在主线程中临时绑定 EGL context 以初始化 OpenGL 资源
    if (!eglMakeCurrent(display, surface, surface, context)) {
        std::cerr << "Failed to bind EGL context in initializeGL" << std::endl;
        checkEGLError("eglMakeCurrent in initializeGL");
        return false;
    }
    
    // Create shader program
    shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    if (!shaderProgram) {
        std::cerr << "Failed to create shader program" << std::endl;
        return false;
    }
    
    // Create texture
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    checkGLError("initializeGL");
    
    // 初始化完成后解绑 context，让渲染线程去绑定
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    
    return true;
}

void Renderer::cleanupGL() {
    // 注意：此方法不再调用 OpenGL 函数，因为它可能在不同的线程中被调用
    // 所有的 OpenGL 资源清理都应该在渲染线程中完成
    shaderProgram = 0;
    texture = 0;
}

GLuint Renderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    // Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint Renderer::createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    // Compile vertex shader
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader) {
        std::cerr << "Failed to compile vertex shader" << std::endl;
        return 0;
    }

    // Compile fragment shader
    // If fragmentShaderSourcePart2Ptr is not null, combine it with fragmentSource
//    GLuint fragmentShader =  compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint fragmentShader;
    if (fragmentShaderSourcePart2Ptr) {
        // Combine fragment shader sources
        std::string combinedSource = fragmentSource;
        combinedSource += fragmentShaderSourcePart2Ptr;
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, combinedSource.c_str());
    } else {
        // Use single fragment shader source
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    }
    
    if (!fragmentShader) {
        std::cerr << "Failed to compile fragment shader" << std::endl;
        glDeleteShader(vertexShader);
        return 0;
    }

    // Create program and attach shaders
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    // Link program
    glLinkProgram(program);

    // Check link status
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

    // Detach and delete shaders (no longer needed after linking)
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Get uniform locations
    uTextureLocation = glGetUniformLocation(program, "uTexture");

    return program;
}

void Renderer::updateTexture() {
    if (imageNames.empty()) {
        std::cerr << "No images available" << std::endl;
        return;
    }
    
    const std::string& currentImageName = imageNames[currentImageIndex];
    const ImageData* imageData = imageLoader.getImage(currentImageName);
    
    if (imageData && imageData->isValid()) {
        // Bind the texture
        glBindTexture(GL_TEXTURE_2D, texture);
        
        // Determine the format based on channels
        GLenum format = (imageData->channels == 4) ? GL_RGBA : GL_RGB;
        
        // Upload the image data
        glTexImage2D(GL_TEXTURE_2D, 0, format, imageData->width, imageData->height, 0, 
                     format, GL_UNSIGNED_BYTE, imageData->data.data());
        
        checkGLError("updateTexture");
    } else {
        std::cerr << "Invalid image data for: " << currentImageName << std::endl;
    }
}

void Renderer::nextFrame() {
    if (imageNames.empty()) {
        return;
    }
    
    // Advance to next image
    currentImageIndex = (currentImageIndex + 1) % imageNames.size();
    updateTexture();
}

void Renderer::previousFrame() {
    if (imageNames.empty()) {
        return;
    }
    
    // Go to previous image
    if (currentImageIndex == 0) {
        currentImageIndex = imageNames.size() - 1;
    } else {
        currentImageIndex--;
    }
    
    updateTexture();
}

void Renderer::renderTexturedQuad() {
    if (!shaderProgram || !texture) {
        return;
    }
    
    // Start timing this frame
    frameStartTime = std::chrono::high_resolution_clock::now();
    
    // Use the shader program
    glUseProgram(shaderProgram);
    
    // Get attribute locations
    GLint positionLoc = glGetAttribLocation(shaderProgram, "aPosition");
    GLint texCoordLoc = glGetAttribLocation(shaderProgram, "aTexCoord");
    
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
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(uTextureLocation, 0);
    
    // 强制 GPU 同步以确保准确的性能测量
    glFinish();
    
    // 开始实际的渲染计时
    auto renderStartTime = std::chrono::high_resolution_clock::now();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // 确保所有渲染完成
    glFinish();
    
    // 结束渲染计时
    auto renderEndTime = std::chrono::high_resolution_clock::now();
    double renderTime = std::chrono::duration<double, std::milli>(renderEndTime - renderStartTime).count();
    
    // Disable vertex attributes
    glDisableVertexAttribArray(positionLoc);
    glDisableVertexAttribArray(texCoordLoc);
    
    // Unbind shader program
    glUseProgram(0);
    
    // End timing this frame
    frameEndTime = std::chrono::high_resolution_clock::now();
    lastFrameTime = std::chrono::duration<double, std::milli>(frameEndTime - renderStartTime).count();
    
    // Update frame statistics
    frameCount++;
    totalRenderTime += lastFrameTime; // 使用每次迭代的平均时间

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
    
    checkGLError("renderTexturedQuad");
}

void Renderer::checkEGLError(const char* msg) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        std::cerr << "EGL Error at " << msg << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

void Renderer::checkGLError(const char* msg) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "GL Error at " << msg << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

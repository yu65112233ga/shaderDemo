#include "Renderer.h"

Renderer::Renderer(HWND hWnd, int width, int height, ImageLoader& imageLoader)
    : hWnd(hWnd), width(width), height(height), imageLoader(imageLoader),
      currentImageIndex(0), paused(false), shouldStepForward(false),
      shouldStepBackward(false), running(false),
      display(EGL_NO_DISPLAY), config(nullptr), context(EGL_NO_CONTEXT),
      surface(EGL_NO_SURFACE), shaderProgram(0), texture(0) {
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
    std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
    
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
            currentTime - lastFrameTime);
        
        // Only render if enough time has passed (for 30 FPS) and not paused
        if (elapsedTime >= frameTime && !paused) {
            // Update texture with next image
            nextFrame();
            
            // Update last frame time
            lastFrameTime = currentTime;
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
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader) return 0;
    
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    
    // Check linking status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Shader program linking error: " << infoLog << std::endl;
        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    
    // Shaders can be deleted after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
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
    
    // Use the shader program
    glUseProgram(shaderProgram);
    
    // Get attribute and uniform locations
    GLint positionLoc = glGetAttribLocation(shaderProgram, "aPosition");
    GLint texCoordLoc = glGetAttribLocation(shaderProgram, "aTexCoord");
    GLint textureLoc = glGetUniformLocation(shaderProgram, "uTexture");
    
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
    glUniform1i(textureLoc, 0);
    
    // Draw quad
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // Disable vertex attributes
    glDisableVertexAttribArray(positionLoc);
    glDisableVertexAttribArray(texCoordLoc);
    
    // Unbind shader program
    glUseProgram(0);
    
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

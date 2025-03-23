#include <iostream>
#include <angle_gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <Windows.h>
#include "ImageLoader.h"
#include <filesystem>
#include <chrono>
#include <thread>

// Global variables
HWND hWnd = nullptr;
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
ImageLoader imageLoader;

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

const char* fragmentShaderSource = R"(
    precision mediump float;
    varying vec2 vTexCoord;
    uniform sampler2D uTexture;
    void main() {
        gl_FragColor = texture2D(uTexture, vTexCoord);
    }
)";

// Shader program and texture variables
GLuint shaderProgram = 0;
GLuint texture = 0;

// Current image index
size_t currentImageIndex = 0;
std::vector<std::string> imageNames;

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Create Windows window
HWND CreateWin32Window(HINSTANCE hInstance, int nCmdShow, int width, int height) {
    // Register window class
    const char CLASS_NAME[] = "ShaderDemoClass";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClass(&wc);
    
    // Create window
    HWND hWnd = CreateWindowEx(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class
        "ANGLE Shader Demo",            // Window title
        WS_OVERLAPPEDWINDOW,            // Window style
        
        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );
    
    if (hWnd == NULL) {
        std::cerr << "Failed to create window" << std::endl;
        return NULL;
    }
    
    // Show window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    
    return hWnd;
}

// Helper function to check EGL errors
void checkEGLError(const char* msg) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        std::cerr << "EGL Error at " << msg << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

// Helper function to check GL errors
void checkGLError(const char* msg) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "GL Error at " << msg << ": 0x" << std::hex << error << std::dec << std::endl;
    }
}

// Compile shader
GLuint compileShader(GLenum type, const char* source) {
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

// Create shader program
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
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

// Initialize OpenGL resources
bool initializeGL() {
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
    
    checkGLError("Texture setup");
    return true;
}

// Update texture with current image data
void updateTexture() {
    if (imageNames.empty()) return;
    
    // Get current image
    const std::string& imageName = imageNames[currentImageIndex];
    const ImageData* imageData = imageLoader.getImage(imageName);
    
    if (imageData && imageData->isValid()) {
        glBindTexture(GL_TEXTURE_2D, texture);
        
        // Determine format based on number of channels
        GLenum format = (imageData->channels == 4) ? GL_RGBA : GL_RGB;
        
        // Update texture with image data
        glTexImage2D(
            GL_TEXTURE_2D, 0, format,
            imageData->width, imageData->height, 0,
            format, GL_UNSIGNED_BYTE, imageData->data.data()
        );
        
        checkGLError("Texture update");
    }
    
    // Move to next image for the next frame
    currentImageIndex = (currentImageIndex + 1) % imageNames.size();
    std::cout << "current image index: " << currentImageIndex << std::endl;
}

// Render a textured quad
void renderTexturedQuad() {
    // Vertex data for a quad (x, y, z, u, v)
    const GLfloat vertices[] = {
        // Positions     // Texture coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // Top left
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,  // Top right
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,  // Bottom right
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f   // Bottom left
    };
    
    // Indices for the quad
    const GLushort indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };
    
    // Use the shader program
    glUseProgram(shaderProgram);
    
    // Get attribute locations
    GLint posAttrib = glGetAttribLocation(shaderProgram, "aPosition");
    GLint texCoordAttrib = glGetAttribLocation(shaderProgram, "aTexCoord");
    
    // Enable attributes
    glEnableVertexAttribArray(posAttrib);
    glEnableVertexAttribArray(texCoordAttrib);
    
    // Set vertex attribute pointers
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(texCoordAttrib, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), vertices + 3);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Set texture uniform
    GLint texUniform = glGetUniformLocation(shaderProgram, "uTexture");
    glUniform1i(texUniform, 0);
    
    // Draw the quad
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
    
    // Disable attributes
    glDisableVertexAttribArray(posAttrib);
    glDisableVertexAttribArray(texCoordAttrib);
    
    checkGLError("Render quad");
}

// Clean up OpenGL resources
void cleanupGL() {
    if (texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    
    if (shaderProgram) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
}

int main() {
    try {
        std::string photoDir = R"(E:\code\shaderDemo\photo2)";
        std::cout << "Looking for photos in: " << photoDir << std::endl;

        // Load images from photo directory
        ImageLoadOptions opt;
        opt.maxImages = 10;
        if (imageLoader.loadImagesFromDirectory(photoDir, opt)) {
            std::cout << "Successfully loaded images from " << photoDir << std::endl;
            
            // Get all loaded image names
            imageNames = imageLoader.getImageNames();
            std::cout << "Loaded images:" << std::endl;
            for (const auto& name : imageNames) {
                const ImageData* img = imageLoader.getImage(name);
                if (img) {
                    std::cout << "  - " << name << ": " << img->width << "x" << img->height 
                              << ", " << img->channels << " channels, " 
                              << img->data.size() << " bytes" << std::endl;
                }
            }
        } else {
            std::cout << "No images found in " << photoDir << " directory" << std::endl;
            return -1;
        }
        
        // Create window
        HINSTANCE hInstance = GetModuleHandle(NULL);
        hWnd = CreateWin32Window(hInstance, SW_SHOW, WINDOW_WIDTH, WINDOW_HEIGHT);
        if (!hWnd) {
            std::cerr << "Failed to create window" << std::endl;
            return -1;
        }
        
        // EGL variables
        EGLDisplay display;
        EGLConfig config;
        EGLContext context;
        EGLSurface surface;
        EGLint numConfigs;
        EGLint majorVersion;
        EGLint minorVersion;
        
        std::cout << "Getting EGL display..." << std::endl;
        // Get EGL display
        display = eglGetDisplay(GetDC(hWnd));
        if (display == EGL_NO_DISPLAY) {
            std::cerr << "Failed to get EGL display" << std::endl;
            return -1;
        }
        
        std::cout << "Initializing EGL..." << std::endl;
        // Initialize EGL
        if (!eglInitialize(display, &majorVersion, &minorVersion)) {
            std::cerr << "Failed to initialize EGL" << std::endl;
            checkEGLError("eglInitialize");
            return -1;
        }
        
        std::cout << "EGL Version: " << majorVersion << "." << minorVersion << std::endl;
        
        // Print EGL client APIs
        const char* apis = eglQueryString(display, EGL_CLIENT_APIS);
        std::cout << "EGL Client APIs: " << (apis ? apis : "<null>") << std::endl;
        
        // Print EGL extensions
        const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
        std::cout << "EGL Extensions: " << (extensions ? extensions : "<null>") << std::endl;
        
        std::cout << "Choosing EGL configuration..." << std::endl;
        // EGL configuration attributes
        const EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,  // Changed from PBUFFER_BIT to WINDOW_BIT
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_NONE
        };
        
        // Choose EGL configuration
        if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs <= 0) {
            std::cerr << "Failed to choose EGL configuration" << std::endl;
            checkEGLError("eglChooseConfig");
            eglTerminate(display);
            return -1;
        }
        
        std::cout << "Creating window surface..." << std::endl;
        // Create window surface
        surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)hWnd, NULL);
        if (surface == EGL_NO_SURFACE) {
            std::cerr << "Failed to create EGL surface" << std::endl;
            checkEGLError("eglCreateWindowSurface");
            eglTerminate(display);
            return -1;
        }
        
        std::cout << "Creating EGL context..." << std::endl;
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
            return -1;
        }
        
        std::cout << "Binding EGL context..." << std::endl;
        // Bind EGL context
        if (!eglMakeCurrent(display, surface, surface, context)) {
            std::cerr << "Failed to bind EGL context" << std::endl;
            checkEGLError("eglMakeCurrent");
            eglDestroyContext(display, context);
            eglDestroySurface(display, surface);
            eglTerminate(display);
            return -1;
        }

        // Set viewport
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        // Initialize OpenGL resources
        if (!initializeGL()) {
            std::cerr << "Failed to initialize OpenGL resources" << std::endl;
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
            eglDestroySurface(display, surface);
            eglTerminate(display);
            return -1;
        }
        
        // Message loop
        MSG msg = {};
        bool running = true;
        
        std::cout << "Starting render loop..." << std::endl;
        
        // Frame timing variables for 30 FPS
        const std::chrono::milliseconds frameTime(33); // ~30 FPS (1000ms / 30 = 33.33ms)
        std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
        
        while (running) {
            // Process Windows messages
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            // Calculate time since last frame
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastFrameTime);
            
            // Only render if enough time has passed (for 30 FPS)
            if (elapsedTime >= frameTime) {
                // Update texture with current image
                updateTexture();
                
                // Clear the screen
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                
                // Render the textured quad
                renderTexturedQuad();
                
                // Swap buffers
                eglSwapBuffers(display, surface);
                
                // Update last frame time
                lastFrameTime = currentTime;
            } else {
                // Sleep to avoid excessive CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        std::cout << "Cleaning up resources..." << std::endl;
        
        // Clean up OpenGL resources
        cleanupGL();
        
        // Clean up EGL resources
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        
        std::cout << "Program exited normally" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return -1;
    }
    
    return 0;
}
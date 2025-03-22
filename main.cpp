#include <iostream>
#include <angle_gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <Windows.h>
#include "ImageLoader.h"
#include <filesystem>

// Global variables
HWND hWnd = nullptr;
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
ImageLoader imageLoader;

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

int main() {
    try {
        // Print current directory
        char buffer[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, buffer);
        std::cout << "Current directory: " << buffer << std::endl;
        
        // Get the executable directory
        std::filesystem::path exePath(buffer);
        std::filesystem::path projectRoot = exePath;
        
        // If we're in the build directory, go up one level
        if (exePath.filename() == "build" || exePath.filename() == "Debug" || exePath.filename() == "Release") {
            projectRoot = exePath.parent_path();
        }
        if (projectRoot.filename() == "Debug" || projectRoot.filename() == "Release") {
            projectRoot = projectRoot.parent_path().parent_path();
        }
        
        // Construct the absolute path to the photo directory
        std::filesystem::path photoPath = projectRoot / "photo";
        std::string photoDir = photoPath.string();
        
        std::cout << "Looking for photos in: " << photoDir << std::endl;
        
        // Load images from photo directory
        if (imageLoader.loadImagesFromDirectory(photoDir)) {
            std::cout << "Successfully loaded images from " << photoDir << std::endl;
            
            // Print all loaded image names
            std::vector<std::string> imageNames = imageLoader.getImageNames();
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
        
        // Message loop
        MSG msg = {};
        bool running = true;
        
        std::cout << "Starting render loop..." << std::endl;
        
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
            
            // Render frame
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            
            // Swap buffers
            eglSwapBuffers(display, surface);
        }
        
        std::cout << "Cleaning up resources..." << std::endl;
        
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
#include <iostream>
#include <Windows.h>
#include <thread>
#include "reader/ImageLoader.h"
#include "render/Renderer.h"

// Global variables
HWND hWnd = nullptr;
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
ImageLoader imageLoader;

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static Renderer* renderer = nullptr;
    
    switch (message) {
    case WM_CLOSE:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (renderer) {
            switch (wParam) {
            case VK_SPACE:  // Space key - toggle pause
                renderer->togglePause();
                break;
            case VK_LEFT:  // Left arrow key - step backward
                renderer->stepBackward();
                break;
            case VK_RIGHT:  // Right arrow key - step forward
                renderer->stepForward();
                break;
            }
        }
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

int main() {
    try {
        std::string photoDir = R"(E:\code\shaderDemo\photo)";
        std::cout << "Looking for photos in: " << photoDir << std::endl;

        // Load images from photo directory
        ImageLoadOptions opt;
        opt.maxImages = 10;
        if (imageLoader.loadImagesFromDirectory(photoDir, opt)) {
            std::cout << "Successfully loaded images from " << photoDir << std::endl;
            
            // Get all loaded image names
            auto imageNames = imageLoader.getImageNames();
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

        // Create renderer
        Renderer renderer(hWnd, WINDOW_WIDTH, WINDOW_HEIGHT, imageLoader);
        
        // Store renderer pointer for window procedure
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&renderer));
        
        // Start renderer in a separate thread
        if (!renderer.start()) {
            std::cerr << "Failed to start renderer" << std::endl;
            return -1;
        }
        
        // Message loop
        MSG msg = {};
        bool running = true;
        
        std::cout << "Application running..." << std::endl;
        
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
            
            // Sleep to avoid excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "Stopping renderer..." << std::endl;
        renderer.stop();
        
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
#include "ImageLoader.h"

// Define STB_IMAGE_IMPLEMENTATION before including stb_image.h to create the implementation
#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb/stb_image.h"

#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

ImageLoader::ImageLoader() {
}

ImageLoader::~ImageLoader() {
    // Clear all loaded images
    clearImages();
}

void ImageLoader::clearImages() {
    images.clear();
}

bool ImageLoader::loadImagesFromDirectory(const std::string& directory, const ImageLoadOptions& options) {
    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        std::cerr << "Error: Directory '" << directory << "' does not exist or is not a directory." << std::endl;
        return false;
    }
    
    size_t loadedCount = 0;
    
    // Collect all PNG files first
    std::vector<fs::path> pngFiles;
    
    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        
        const fs::path& path = entry.path();
        std::string extension = path.extension().string();
        
        // Convert extension to lowercase for case-insensitive comparison
        std::transform(extension.begin(), extension.end(), extension.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        
        // Collect PNG files
        if (extension == ".png") {
            pngFiles.push_back(path);
        }
    }
    
    // Sort files by name for consistent loading order
    std::sort(pngFiles.begin(), pngFiles.end());
    
    // Apply max images limit if specified
    if (options.maxImages > 0 && pngFiles.size() > static_cast<size_t>(options.maxImages)) {
        pngFiles.resize(options.maxImages);
    }
    
    // Load the PNG files
    for (const auto& path : pngFiles) {
        std::string filePath = path.string();
        std::string baseName = extractBaseName(path);
        
        // Load the image
        int width, height, channels;
        stbi_set_flip_vertically_on_load(false); // Don't flip the image vertically
        
        // If maxResolution is set, use it to resize the image during loading
        if (options.maxResolution > 0) {
            // First get the original dimensions
            int origWidth, origHeight, origChannels;
            if (!stbi_info(filePath.c_str(), &origWidth, &origHeight, &origChannels)) {
                std::cerr << "Failed to get image info: " << filePath << std::endl;
                continue;
            }
            
            // Calculate the resize factor
            float factor = 1.0f;
            if (origWidth > origHeight) {
                if (origWidth > options.maxResolution) {
                    factor = static_cast<float>(options.maxResolution) / origWidth;
                }
            } else {
                if (origHeight > options.maxResolution) {
                    factor = static_cast<float>(options.maxResolution) / origHeight;
                }
            }
            
            // Load the image with resizing
            unsigned char* data = nullptr;
            if (factor < 1.0f) {
                // Calculate new dimensions
                int newWidth = static_cast<int>(origWidth * factor);
                int newHeight = static_cast<int>(origHeight * factor);
                
                // Load and resize
                data = stbi_load_from_file_with_hq_2x(fopen(filePath.c_str(), "rb"), &width, &height, &channels, 0);
                
                if (!data) {
                    // Fall back to regular loading if high-quality resize fails
                    data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
                }
            } else {
                // Load at original size
                data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
            }
            
            if (data) {
                // Calculate the size of the image data
                size_t dataSize = width * height * channels;
                
                // Store the image data
                images[baseName] = ImageData(width, height, channels, data, dataSize);
                
                // Free the image data loaded by stb_image
                stbi_image_free(data);
                
                loadedCount++;
                if (options.verbose) {
                    std::cout << "Loaded image: " << baseName << " (" << width << "x" << height << ", " 
                              << channels << " channels)" << std::endl;
                }
            } else {
                std::cerr << "Failed to load image: " << filePath << std::endl;
            }
        } else {
            // Load at original size
            unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
            
            if (data) {
                // Calculate the size of the image data
                size_t dataSize = width * height * channels;
                
                // Store the image data
                images[baseName] = ImageData(width, height, channels, data, dataSize);
                
                // Free the image data loaded by stb_image
                stbi_image_free(data);
                
                loadedCount++;
                if (options.verbose) {
                    std::cout << "Loaded image: " << baseName << " (" << width << "x" << height << ", " 
                              << channels << " channels)" << std::endl;
                }
            } else {
                std::cerr << "Failed to load image: " << filePath << std::endl;
            }
        }
    }
    
    std::cout << "Loaded " << loadedCount << " PNG images from " << directory << std::endl;
    return loadedCount > 0;
}

const ImageData* ImageLoader::getImage(const std::string& name) const {
    auto it = images.find(name);
    if (it != images.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<std::string> ImageLoader::getImageNames() const {
    std::vector<std::string> names;
    names.reserve(images.size());
    
    for (const auto& pair : images) {
        names.push_back(pair.first);
    }
    
    return names;
}

std::string ImageLoader::extractBaseName(const fs::path& path) const {
    return path.stem().string();
}

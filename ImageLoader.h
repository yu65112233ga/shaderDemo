#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <iostream>

// Structure to hold image data
struct ImageData {
    int width;
    int height;
    int channels;  // 3 for RGB, 4 for RGBA
    std::vector<unsigned char> data;
    
    ImageData() : width(0), height(0), channels(0) {}
    
    ImageData(int w, int h, int c, const unsigned char* ptr, size_t size) 
        : width(w), height(h), channels(c), data(ptr, ptr + size) {}
    
    bool isValid() const { return !data.empty() && width > 0 && height > 0; }
};

// Options for loading images
struct ImageLoadOptions {
    int maxImages;       // Maximum number of images to load (0 = no limit)
    int maxResolution;   // Maximum resolution (width or height) to load (0 = original resolution)
    bool verbose;        // Print detailed loading information
    
    ImageLoadOptions() : maxImages(0), maxResolution(0), verbose(true) {}
};

class ImageLoader {
public:
    ImageLoader();
    ~ImageLoader();
    
    // Load all PNG images from a directory with options
    bool loadImagesFromDirectory(const std::string& directory, const ImageLoadOptions& options = ImageLoadOptions());
    
    // Get image data by filename (without path and extension)
    const ImageData* getImage(const std::string& name) const;
    
    // Get all loaded image names
    std::vector<std::string> getImageNames() const;
    
    // Get number of loaded images
    size_t getImageCount() const { return images.size(); }
    
    // Clear all loaded images
    void clearImages();
    
private:
    // Map of image name to image data
    std::unordered_map<std::string, ImageData> images;
    
    // Extract filename without extension
    std::string extractBaseName(const std::filesystem::path& path) const;
};

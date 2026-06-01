#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct TextureData {
    std::string name;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t mipmapCount;
    bool hasAlpha;
    std::vector<uint8_t> ddsData;
};

struct TexFile {
    std::map<std::string, TextureData> textures;
    std::string error;
};

TexFile LoadTex(const std::string& filepath);

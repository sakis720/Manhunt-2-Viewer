#include "tex_parser.hpp"
#include "utils.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdio>

static void ParseTCDT(const uint8_t* data, size_t len, TexFile& result) {
    if (len < 48) return;

    // Header: magic(4), unk(28), numTextures(4), firstOffset(4)
    uint32_t numTextures = ReadLE<uint32_t>(data, len, 32); 
    uint32_t firstOffset = ReadLE<uint32_t>(data, len, 36);

    printf("[Tex] TCDT Archive: numTextures=%d firstOffset=0x%X\n", numTextures, firstOffset);

    uint32_t nextOffset = firstOffset;
    for (uint32_t i = 0; i < numTextures; i++) {
        if (nextOffset == 0 || nextOffset + 112 > len) break;

        // Corrected Offsets based on Python struct.unpack('<II32sB31sIIIIHHB3sIIII', ...)
        uint32_t currentNextOffset = ReadLE<uint32_t>(data, len, nextOffset + 0);
        
        char rawName[33];
        std::memcpy(rawName, data + nextOffset + 8, 32);
        rawName[32] = '\0';
        std::string name = rawName;

        uint8_t alphaFlag = data[nextOffset + 40];
        uint32_t width = ReadLE<uint32_t>(data, len, nextOffset + 72);
        uint32_t height = ReadLE<uint32_t>(data, len, nextOffset + 76);
        uint32_t bpp = ReadLE<uint32_t>(data, len, nextOffset + 80);
        // uint32_t pitch = ReadLE<uint32_t>(data, len, nextOffset + 84);
        uint8_t mipmapCount = data[nextOffset + 92]; // Index 11 (B)
        uint32_t dataOffset = ReadLE<uint32_t>(data, len, nextOffset + 96); // Index 13 (I)
        uint32_t ddsSize = ReadLE<uint32_t>(data, len, nextOffset + 104); // Index 15 (I)

        printf("[Tex]  [%d] name=\"%s\" %dx%d size=%d at 0x%X (next: 0x%X)\n", i, name.c_str(), width, height, ddsSize, dataOffset, currentNextOffset);

        if (ddsSize > 0 && dataOffset > 0 && dataOffset + ddsSize <= len) {
            TextureData tex;
            tex.name = name;
            tex.width = width;
            tex.height = height;
            tex.bpp = bpp;
            tex.mipmapCount = mipmapCount;
            tex.hasAlpha = (alphaFlag != 0);
            tex.ddsData.assign(data + dataOffset, data + dataOffset + ddsSize);
            result.textures[name] = std::move(tex);
        }

        nextOffset = currentNextOffset;
        if (nextOffset == 0) break;
    }
}

static void ParseSingleTex(const uint8_t* data, size_t len, TexFile& result) {
    if (len < 112) return;

    char rawName[33];
    std::memcpy(rawName, data + 8, 32);
    rawName[32] = '\0';
    std::string name = rawName;

    uint8_t alphaFlag = data[40];
    uint32_t width = ReadLE<uint32_t>(data, len, 72);
    uint32_t height = ReadLE<uint32_t>(data, len, 76);
    uint32_t bpp = ReadLE<uint32_t>(data, len, 80);
    uint8_t mipmapCount = data[92];
    uint32_t dataOffset = ReadLE<uint32_t>(data, len, 96);
    uint32_t ddsSize = ReadLE<uint32_t>(data, len, 104);

    if (ddsSize > 0 && dataOffset > 0 && dataOffset + ddsSize <= len) {
        TextureData tex;
        tex.name = name;
        tex.width = width;
        tex.height = height;
        tex.bpp = bpp;
        tex.mipmapCount = mipmapCount;
        tex.hasAlpha = (alphaFlag != 0);
        tex.ddsData.assign(data + dataOffset, data + dataOffset + ddsSize);
        result.textures[name] = std::move(tex);
        printf("[Tex] Single Entry: name=\"%s\" %dx%d size=%d at 0x%X\n", name.c_str(), width, height, ddsSize, dataOffset);
    }
}

TexFile LoadTex(const std::string& filepath) {
    TexFile result;
    std::ifstream f(filepath, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        result.error = "Failed to open file: " + filepath;
        return result;
    }

    size_t size = f.tellg();
    std::vector<uint8_t> raw(size);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(raw.data()), size);

    if (raw.size() < 4) {
        result.error = "File too small";
        return result;
    }

    const uint8_t MAGIC_COMPRESSED[4] = { 0x5A, 0x32, 0x48, 0x4D }; // Z2HM
    const uint8_t MAGIC_TCDT[4] = { 0x54, 0x43, 0x44, 0x54 };       // TCDT

    if (std::memcmp(raw.data(), MAGIC_COMPRESSED, 4) == 0) {
        printf("[Tex] Compressed Z2HM found.\n");
        std::vector<uint8_t> decompressed;
        if (DecompressZlib(raw.data() + 8, raw.size() - 8, decompressed)) {
            if (decompressed.size() >= 4 && std::memcmp(decompressed.data(), MAGIC_TCDT, 4) == 0) {
                ParseTCDT(decompressed.data(), decompressed.size(), result);
            } else {
                ParseSingleTex(decompressed.data(), decompressed.size(), result);
            }
        } else {
            result.error = "Decompression failed";
        }
    } else if (std::memcmp(raw.data(), MAGIC_TCDT, 4) == 0) {
        ParseTCDT(raw.data(), raw.size(), result);
    } else {
        ParseSingleTex(raw.data(), raw.size(), result);
    }

    if (result.textures.empty() && result.error.empty()) {
        result.error = "No textures found in file";
    }

    return result;
}

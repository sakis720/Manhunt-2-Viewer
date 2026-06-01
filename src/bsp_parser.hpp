#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct BSPVertex {
    float x, y, z;
    float nx, ny, nz; // Decoded Normals
    uint32_t packedNormal;
    uint8_t color[4]; // BGRA
    float uv1[2];     // UV Set 1
    float uv2[2];     // UV Set 2 (if present)
};

struct BSPSubMesh {
    uint32_t materialIndex;
    std::vector<uint16_t> indices;
    size_t offset; // Offset in the (decompressed) file
};

struct BSPMesh {
    std::vector<BSPVertex> vertices;
    std::vector<BSPSubMesh> subMeshes;
    uint32_t vertexElementType;
    size_t offset; // Offset of the chunk tag in the (decompressed) file
};

struct BSPFile {
    std::vector<BSPMesh> meshes;
    std::vector<std::string> materialNames;
    bool compressed;                   // was the file zlib-compressed?
    std::string error;                 // non-empty on failure
};


struct BSPLoadOptions {
    bool isOlderVersion = false;
};

BSPFile LoadBSP(const std::string& filepath, BSPLoadOptions options = {});

bool ExportToOBJ(const BSPFile& bsp, const std::string& filepath);

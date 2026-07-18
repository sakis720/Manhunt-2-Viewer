#include "bsp_parser.hpp"
#include "utils.hpp"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cmath>

static const uint8_t CHUNK_TAG[4] = { 0x54, 0xD4, 0x45, 0x00 };

static size_t FindChunkTag(const uint8_t* data, size_t len, size_t startPos) {
    if (len < 4) return std::string::npos;
    for (size_t i = startPos; i + 4 <= len; ++i) {
        if (data[i]   == CHUNK_TAG[0] && data[i+1] == CHUNK_TAG[1] &&
            data[i+2] == CHUNK_TAG[2] && data[i+3] == CHUNK_TAG[3]) {
            return i;
        }
    }
    return std::string::npos;
}

struct MaterialID {
    float min[3], max[3];
    uint16_t numFaces;
    uint16_t materialIndex;
    uint16_t startFaceID;
};

BSPFile LoadBSP(const std::string& filepath, BSPLoadOptions options) {
    BSPFile result;
    result.compressed = false;

    std::ofstream log("bsp_debug.log");
    if (!log.is_open()) printf("[Manhunt 2 Viewer] Warning: Could not create bsp_debug.log\n");
    log << "Loading BSP: " << filepath << std::endl;

    std::vector<uint8_t> raw;
    if (!ReadFileBytes(filepath, raw)) {
        result.error = "Failed to open file";
        return result;
    }

    if (raw.size() < 8) {
        result.error = "File too small";
        return result;
    }

    std::vector<uint8_t> decompressed;
    const uint8_t* data;
    size_t dataLen;

    const uint8_t MAGIC_COMPRESSED[4]   = { 0x5A, 0x32, 0x48, 0x4D };
    const uint8_t MAGIC_UNCOMPRESSED[4] = { 0x44, 0x4C, 0x52, 0x57 };

    if (std::memcmp(raw.data(), MAGIC_COMPRESSED, 4) == 0) {
        result.compressed = true;
        if (!DecompressZlib(raw.data() + 8, raw.size() - 8, decompressed)) {
            result.error = "Zlib decompression failed";
            return result;
        }
        data    = decompressed.data();
        dataLen = decompressed.size();
    } else if (std::memcmp(raw.data(), MAGIC_UNCOMPRESSED, 4) == 0) {
        data    = raw.data();
        dataLen = raw.size();
    } else {
        result.error = "Unknown file magic bytes";
        return result;
    }

    // Material Name Resolution from Material Table (0x50 / 0x54)
    if (dataLen >= 0x58) {
        const uint32_t tableOffset = ReadLE<uint32_t>(data, dataLen, 0x50);
        const uint32_t count = ReadLE<uint32_t>(data, dataLen, 0x54);
        if (tableOffset != 0 && count < 5000) {
            const size_t tableOffsetSz = static_cast<size_t>(tableOffset);
            const size_t tableSize = static_cast<size_t>(count) * 12u;
            if (tableOffsetSz + tableSize <= dataLen) {
                result.materialNames.reserve(count);
                bool anyNonEmpty = false;
                for (uint32_t i = 0; i < count; i++) {
                    const uint32_t entryOffset = tableOffset + i * 12u;
                    const uint32_t nameOffset = ReadLE<uint32_t>(data, dataLen, entryOffset);
                    if (nameOffset != 0 && nameOffset < dataLen) {
                        const uint8_t* start = data + nameOffset;
                        const void* nul = std::memchr(start, 0, dataLen - nameOffset);
                        if (nul != nullptr) {
                            const char* cStart = reinterpret_cast<const char*>(start);
                            const char* cEnd = reinterpret_cast<const char*>(nul);
                            result.materialNames.emplace_back(cStart, static_cast<size_t>(cEnd - cStart));
                            anyNonEmpty |= !result.materialNames.back().empty();
                            continue;
                        }
                    }
                    result.materialNames.emplace_back("");
                }
                if (!anyNonEmpty) result.materialNames.clear();
            }
        }
    }

    // Fallback 1: Original pointer indirection method
    if (result.materialNames.empty() && dataLen >= 24) {
        uint32_t offsetTablePos = ReadLE<uint32_t>(data, dataLen, 12);
        if (offsetTablePos != 0 && offsetTablePos < dataLen) {
            uint32_t ptr1 = ReadLE<uint32_t>(data, dataLen, offsetTablePos);
            if (ptr1 != 0 && ptr1 < dataLen) {
                uint32_t ptr2 = ReadLE<uint32_t>(data, dataLen, ptr1);
                if (ptr2 != 0 && ptr2 < dataLen) {
                    uint32_t ptr3 = ReadLE<uint32_t>(data, dataLen, ptr2);
                    if (ptr3 != 0 && ptr3 < dataLen) {
                        size_t sPos = ptr3;
                        while (sPos < dataLen && data[sPos] != 0) {
                            size_t len = 0;
                            while (sPos + len < dataLen && data[sPos + len] != '\0') {
                                len++;
                            }
                            std::string name(reinterpret_cast<const char*>(data + sPos), len);
                            result.materialNames.push_back(name);
                            sPos += len + 1;
                            if (result.materialNames.size() > 5000) break;
                        }
                    }
                }
            }
        }
    }

    // Fallback 2: Original consecutive scanning from first string in 0x50
    if (result.materialNames.empty() && dataLen >= 0x54) {
        uint32_t f1 = ReadLE<uint32_t>(data, dataLen, 0x50);
        if (f1 != 0 && f1 < dataLen) {
            uint32_t f2 = ReadLE<uint32_t>(data, dataLen, f1);
            if (f2 != 0 && f2 < dataLen) {
                size_t sPos = f2;
                while (sPos < dataLen && data[sPos] != 0) {
                    size_t len = 0;
                    while (sPos + len < dataLen && data[sPos + len] != '\0') {
                        len++;
                    }
                    std::string name(reinterpret_cast<const char*>(data + sPos), len);
                    result.materialNames.push_back(name);
                    sPos += len + 1;
                    if (result.materialNames.size() > 5000) break;
                }
            }
        }
    }

    size_t pos = 0;
    while (true) {
        size_t tagPos = FindChunkTag(data, dataLen, pos);
        if (tagPos == std::string::npos) break;
        if (tagPos + 148 > dataLen) break;

        uint32_t numMaterialIDs = ReadLE<uint32_t>(data, dataLen, tagPos + 12);
        uint32_t numFaceIndices = ReadLE<uint32_t>(data, dataLen, tagPos + 16);
        uint32_t numVertices = ReadLE<uint32_t>(data, dataLen, tagPos + 48);
        uint32_t vertexElementType = ReadLE<uint32_t>(data, dataLen, tagPos + 112);

        uint32_t vertexStride = 0;
        int numUV = 0;
        switch (vertexElementType) {
            case 0x52:  vertexStride = 24; numUV = 0; break;
            case 0x152: vertexStride = 32; numUV = 1; break;
            case 0x252: vertexStride = 40; numUV = 2; break;
            default: pos = tagPos + 4; continue;
        }

        std::vector<MaterialID> matIDs;
        size_t matIDStart = tagPos + 148;
        for (uint32_t i = 0; i < numMaterialIDs; i++) {
            size_t off = matIDStart + i * 44;
            if (off + 44 > dataLen) break;
            MaterialID mid;
            mid.numFaces = ReadLE<uint16_t>(data, dataLen, off + 24);
            mid.materialIndex = ReadLE<uint16_t>(data, dataLen, off + 26);
            mid.startFaceID = ReadLE<uint16_t>(data, dataLen, off + 28);
            matIDs.push_back(mid);
        }

        size_t indicesStart = matIDStart + numMaterialIDs * 44;
        if (indicesStart + numFaceIndices * 2 > dataLen) { pos = tagPos + 4; continue; }

        size_t verticesStart = indicesStart + numFaceIndices * 2;
        if (verticesStart + numVertices * vertexStride > dataLen) { pos = tagPos + 4; continue; }

        BSPMesh mesh;
        mesh.offset = tagPos;
        mesh.vertexElementType = vertexElementType;
        mesh.vertices.resize(numVertices);

        for (uint32_t i = 0; i < numVertices; i++) {
            size_t off = verticesStart + i * vertexStride;
            BSPVertex& v = mesh.vertices[i];
            v.x = ReadLE<float>(data, dataLen, off + 0);
            v.y = ReadLE<float>(data, dataLen, off + 4);
            v.z = ReadLE<float>(data, dataLen, off + 8);
            v.packedNormal = ReadLE<uint32_t>(data, dataLen, off + 12);
            std::memcpy(v.color, data + off + 20, 4);
            if (numUV >= 1) { v.uv1[0] = ReadLE<float>(data, dataLen, off + 24); v.uv1[1] = ReadLE<float>(data, dataLen, off + 28); }
            if (numUV >= 2) { v.uv2[0] = ReadLE<float>(data, dataLen, off + 32); v.uv2[1] = ReadLE<float>(data, dataLen, off + 36); }
            v.nx = v.ny = v.nz = 0;
        }

        for (uint32_t i = 0; i < (uint32_t)matIDs.size(); i++) {
            const auto& mid = matIDs[i];
            BSPSubMesh sub;
            sub.materialIndex = mid.materialIndex;
            sub.offset = matIDStart + i * 44;
            sub.indices.resize(mid.numFaces);
            for (uint16_t j = 0; j < mid.numFaces; j++) {
                uint16_t idx = ReadLE<uint16_t>(data, dataLen, indicesStart + (mid.startFaceID + j) * 2);
                sub.indices[j] = (idx < numVertices) ? idx : 0;
            }
            mesh.subMeshes.push_back(std::move(sub));
        }

        // Calculate Normals
        for (auto& sub : mesh.subMeshes) {
            for (size_t t = 0; t < sub.indices.size() / 3; t++) {
                uint16_t i0 = sub.indices[t * 3 + 0], i1 = sub.indices[t * 3 + 1], i2 = sub.indices[t * 3 + 2];
                BSPVertex &v0 = mesh.vertices[i0], &v1 = mesh.vertices[i1], &v2 = mesh.vertices[i2];
                float dx1 = v1.x - v0.x, dy1 = v1.y - v0.y, dz1 = v1.z - v0.z;
                float dx2 = v2.x - v0.x, dy2 = v2.y - v0.y, dz2 = v2.z - v0.z;
                float nx = dy1 * dz2 - dz1 * dy2, ny = dz1 * dx2 - dx1 * dz2, nz = dx1 * dy2 - dy1 * dx2;
                v0.nx += nx; v0.ny += ny; v0.nz += nz; v1.nx += nx; v1.ny += ny; v1.nz += nz; v2.nx += nx; v2.ny += ny; v2.nz += nz;
            }
        }
        for (auto& v : mesh.vertices) {
            float mag = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (mag > 0.000001f) { v.nx /= mag; v.ny /= mag; v.nz /= mag; } else { v.ny = 1.0f; }
        }

        result.meshes.push_back(std::move(mesh));
        pos = verticesStart + numVertices * vertexStride;
    }
    return result;
}

bool ExportToOBJ(const BSPFile& bsp, const std::string& filepath) {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    uint32_t vOff = 1;
    for (size_t m = 0; m < bsp.meshes.size(); m++) {
        const auto& mesh = bsp.meshes[m];
        f << "g Mesh_" << m << "\n";
        for (const auto& v : mesh.vertices) f << "v " << v.x << " " << v.y << " " << v.z << "\nvn " << v.nx << " " << v.ny << " " << v.nz << "\nvt " << v.uv1[0] << " " << v.uv1[1] << "\n";
        for (const auto& sub : mesh.subMeshes) {
            f << "usemtl Material_" << sub.materialIndex << "\n";
            for (size_t t = 0; t < sub.indices.size() / 3; t++) {
                uint32_t i0 = sub.indices[t*3]+vOff, i1 = sub.indices[t*3+1]+vOff, i2 = sub.indices[t*3+2]+vOff;
                f << "f " << i0 << "/" << i0 << "/" << i0 << " " << i1 << "/" << i1 << "/" << i1 << " " << i2 << "/" << i2 << "/" << i2 << "\n";
            }
        }
        vOff += (uint32_t)mesh.vertices.size();
    }
    return true;
}

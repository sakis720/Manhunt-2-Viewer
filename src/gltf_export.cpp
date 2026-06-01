#include "gltf_export.hpp"
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <map>
#include <algorithm>

// Binary GLTF (.glb) Exporter - Logical Naming Edition

struct BufferView {
    uint32_t offset;
    uint32_t length;
    uint32_t target; 
};

struct Accessor {
    uint32_t bufferView;
    uint32_t componentType; 
    uint32_t count;
    std::string type; 
    float min[3], max[3];
    bool hasMinMax = false;
};

static void WritePadding(std::vector<uint8_t>& buffer) {
    while (buffer.size() % 4 != 0) buffer.push_back(0);
}

static std::string SanitizeFilename(const std::string& name) {
    std::string s = name;
    const char* illegal = "\\/:*?\"<>|";
    for (char& c : s) {
        if (strchr(illegal, c)) c = '_';
        else if ((unsigned char)c < 32 || (unsigned char)c > 126) c = '_';
    }
    return s;
}

static std::string JsonEscape(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if ((unsigned char)c < 32) res += ' ';
        else res += c;
    }
    return res;
}

bool ExportToGLTF(const BSPFile& bsp, const std::string& filepath, 
                  const std::map<std::string, Texture2D>& textures,
                  const std::map<std::string, std::vector<uint8_t>>& rawDDS) {

    std::vector<uint8_t> binBuffer;
    binBuffer.reserve(32 * 1024 * 1024);
    
    std::vector<BufferView> bufferViews;
    std::vector<Accessor> accessors;

    struct PrimitiveRef {
        uint32_t posAcc, normAcc, uvAcc, colAcc, idxAcc;
        uint32_t materialIdx;
        int chunkIdx, subIdx; // Tracking indices for naming
    };
    std::vector<PrimitiveRef> subMeshRefs;

    // --- 1) Process Geometry ---
    for (size_t mIdx = 0; mIdx < bsp.meshes.size(); mIdx++) {
        const auto& bspMesh = bsp.meshes[mIdx];
        for (size_t sIdx = 0; sIdx < bspMesh.subMeshes.size(); sIdx++) {
            const auto& sub = bspMesh.subMeshes[sIdx];
            if (sub.indices.empty()) continue;

            std::vector<int> usedVertices;
            std::map<int, int> oldToNew;
            for (auto oldIdx : sub.indices) {
                if (oldIdx >= 0 && oldIdx < (int)bspMesh.vertices.size()) {
                    if (oldToNew.find(oldIdx) == oldToNew.end()) {
                        oldToNew[oldIdx] = (int)usedVertices.size();
                        usedVertices.push_back(oldIdx);
                    }
                }
            }
            if (usedVertices.empty()) continue;
            uint32_t vCount = (uint32_t)usedVertices.size();

            // Positions
            WritePadding(binBuffer);
            uint32_t posStart = (uint32_t)binBuffer.size();
            float minP[3] = {1e18f, 1e18f, 1e18f}, maxP[3] = {-1e18f, -1e18f, -1e18f};
            for (int oldIdx : usedVertices) {
                const auto& v = bspMesh.vertices[oldIdx];
                float p[3] = { v.x, v.y, v.z };
                for(int i=0; i<3; i++) { minP[i] = std::min(minP[i], p[i]); maxP[i] = std::max(maxP[i], p[i]); }
                binBuffer.insert(binBuffer.end(), (uint8_t*)p, (uint8_t*)p + 12);
            }
            bufferViews.push_back({ posStart, vCount * 12, 34962 });
            accessors.push_back({ (uint32_t)bufferViews.size()-1, 5126, vCount, "VEC3" });
            std::memcpy(accessors.back().min, minP, 12); std::memcpy(accessors.back().max, maxP, 12); accessors.back().hasMinMax = true;

            // Normals
            WritePadding(binBuffer);
            uint32_t normStart = (uint32_t)binBuffer.size();
            for (int oldIdx : usedVertices) {
                const auto& v = bspMesh.vertices[oldIdx];
                float n[3] = { v.nx, v.ny, v.nz };
                binBuffer.insert(binBuffer.end(), (uint8_t*)n, (uint8_t*)n + 12);
            }
            bufferViews.push_back({ normStart, vCount * 12, 34962 });
            accessors.push_back({ (uint32_t)bufferViews.size()-1, 5126, vCount, "VEC3" });

            // UVs
            WritePadding(binBuffer);
            uint32_t uvStart = (uint32_t)binBuffer.size();
            for (int oldIdx : usedVertices) {
                const auto& v = bspMesh.vertices[oldIdx];
                float uv[2] = { v.uv1[0], v.uv1[1] };
                binBuffer.insert(binBuffer.end(), (uint8_t*)uv, (uint8_t*)uv + 8);
            }
            bufferViews.push_back({ uvStart, vCount * 8, 34962 });
            accessors.push_back({ (uint32_t)bufferViews.size()-1, 5126, vCount, "VEC2" });

            // Colors
            WritePadding(binBuffer);
            uint32_t colStart = (uint32_t)binBuffer.size();
            for (int oldIdx : usedVertices) {
                const auto& v = bspMesh.vertices[oldIdx];
                float c[4] = { v.color[2]/255.0f, v.color[1]/255.0f, v.color[0]/255.0f, v.color[3]/255.0f };
                binBuffer.insert(binBuffer.end(), (uint8_t*)c, (uint8_t*)c + 16);
            }
            bufferViews.push_back({ colStart, vCount * 16, 34962 });
            accessors.push_back({ (uint32_t)bufferViews.size()-1, 5126, vCount, "VEC4" });

            // Indices
            WritePadding(binBuffer);
            uint32_t idxStart = (uint32_t)binBuffer.size();
            for (auto oldIdx : sub.indices) {
                uint16_t val = (uint16_t)oldToNew[oldIdx];
                binBuffer.insert(binBuffer.end(), (uint8_t*)&val, (uint8_t*)&val + 2);
            }
            bufferViews.push_back({ idxStart, (uint32_t)sub.indices.size() * 2, 34963 });
            accessors.push_back({ (uint32_t)bufferViews.size()-1, 5123, (uint32_t)sub.indices.size(), "SCALAR" });

            subMeshRefs.push_back({ (uint32_t)accessors.size()-5, (uint32_t)accessors.size()-4, (uint32_t)accessors.size()-3, (uint32_t)accessors.size()-2, (uint32_t)accessors.size()-1, sub.materialIndex, (int)mIdx, (int)sIdx });
        }
    }

    // --- 2) Process Textures (GPU) ---
    std::string baseDir = "";
    size_t lastSlash = filepath.find_last_of("\\/");
    if (lastSlash != std::string::npos) baseDir = filepath.substr(0, lastSlash + 1);

    std::map<std::string, int> texToTextureIdx;
    std::vector<uint32_t> imageBufferViews;
    int tCount = 0;

    for (auto const& [name, tex] : textures) {
        if (rawDDS.count(name)) {
            std::string safeName = SanitizeFilename(name);
            std::string ddsPath = baseDir + safeName + ".dds";
            std::ofstream df(ddsPath, std::ios::binary);
            if (df.is_open()) { df.write((char*)rawDDS.at(name).data(), rawDDS.at(name).size()); df.close(); }
        }

        RenderTexture2D rt = LoadRenderTexture(tex.width, tex.height);
        if (rt.id != 0) {
            BeginTextureMode(rt);
            ClearBackground(BLANK);
            DrawTexturePro(tex, {0,0,(float)tex.width,(float)-tex.height}, {0,0,(float)tex.width,(float)tex.height}, {0,0}, 0, WHITE);
            EndTextureMode();
            
            Image img = LoadImageFromTexture(rt.texture);
            if (img.data) {
                int pngSize = 0;
                unsigned char* pngData = ExportImageToMemory(img, ".png", &pngSize);
                if (pngData) {
                    WritePadding(binBuffer);
                    uint32_t imgStart = (uint32_t)binBuffer.size();
                    binBuffer.insert(binBuffer.end(), pngData, pngData + pngSize);
                    imageBufferViews.push_back((uint32_t)bufferViews.size());
                    bufferViews.push_back({ imgStart, (uint32_t)pngSize, 0 });
                    texToTextureIdx[name] = tCount++;
                    std::string safeName = SanitizeFilename(name);
                    std::string pngPath = baseDir + safeName + ".png";
                    std::ofstream tf(pngPath, std::ios::binary);
                    if (tf.is_open()) { tf.write((char*)pngData, pngSize); tf.close(); }
                    MemFree(pngData);
                }
                UnloadImage(img);
            }
            UnloadRenderTexture(rt);
        }
    }

    // --- 3) Build JSON ---
    std::stringstream ss;
    ss.imbue(std::locale::classic());
    ss << std::fixed << std::setprecision(6);
    ss << "{\"asset\":{\"version\":\"2.0\"},";
    
    // Nodes
    ss << "\"scenes\":[{\"nodes\":[";
    for(size_t i=0; i<subMeshRefs.size(); i++) ss << i << (i == subMeshRefs.size()-1 ? "" : ",");
    ss << "]}],\"scene\":0,";

    ss << "\"nodes\":[";
    for(size_t i=0; i<subMeshRefs.size(); i++) {
        const auto& ref = subMeshRefs[i];
        std::string nodeName = "Chunk" + std::to_string(ref.chunkIdx) + ".Sub" + std::to_string(ref.subIdx);
        ss << "{\"name\":\"" << nodeName << "\",\"mesh\":" << i << "}" << (i == subMeshRefs.size()-1 ? "" : ",");
    }
    ss << "],";

    // Meshes
    ss << "\"meshes\":[";
    for(size_t i=0; i<subMeshRefs.size(); i++) {
        const auto& ref = subMeshRefs[i];
        std::string meshName = "Chunk" + std::to_string(ref.chunkIdx) + ".Sub" + std::to_string(ref.subIdx);
        uint32_t mIdx = (ref.materialIdx < bsp.materialNames.size()) ? ref.materialIdx : 0;

        ss << "{\"name\":\"" << meshName << "\",\"primitives\":[{\"attributes\":{\"POSITION\":" << ref.posAcc 
           << ",\"NORMAL\":" << ref.normAcc << ",\"TEXCOORD_0\":" << ref.uvAcc 
           << ",\"COLOR_0\":" << ref.colAcc << "},";
        ss << "\"indices\":" << ref.idxAcc << ",\"material\":" << mIdx << "}]}";
        if(i < subMeshRefs.size()-1) ss << ",";
    }
    ss << "],";

    // Materials
    ss << "\"materials\":[";
    for(size_t i=0; i<bsp.materialNames.size(); i++) {
        std::string matName = bsp.materialNames[i];
        size_t slash = matName.find_last_of("\\/");
        std::string nameNoPath = (slash != std::string::npos) ? matName.substr(slash + 1) : matName;
        int texIdx = -1;
        if (texToTextureIdx.count(matName)) texIdx = texToTextureIdx[matName];
        else if (texToTextureIdx.count(nameNoPath)) texIdx = texToTextureIdx[nameNoPath];
        else {
            std::string lowerMat = matName; std::transform(lowerMat.begin(), lowerMat.end(), lowerMat.begin(), ::tolower);
            std::string lowerNoPath = nameNoPath; std::transform(lowerNoPath.begin(), lowerNoPath.end(), lowerNoPath.begin(), ::tolower);
            for (auto const& [tName, idx] : texToTextureIdx) {
                std::string lowerT = tName; std::transform(lowerT.begin(), lowerT.end(), lowerT.begin(), ::tolower);
                if (lowerT == lowerMat || lowerT == lowerNoPath) { texIdx = idx; break; }
            }
        }
        ss << "{\"name\":\"" << JsonEscape(matName) << "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[1,1,1,1],\"metallicFactor\":0,\"roughnessFactor\":1";
        if (texIdx != -1) ss << ",\"baseColorTexture\":{\"index\":" << texIdx << "}";
        ss << "},\"doubleSided\":true}";
        if(i < bsp.materialNames.size()-1) ss << ",";
    }
    ss << "],";

    if (tCount > 0) {
        ss << "\"textures\":[";
        for(int i=0; i<tCount; i++) ss << "{\"sampler\":0,\"source\":" << i << "}" << (i == tCount-1 ? "" : ",");
        ss << "],\"images\":[";
        for(size_t i=0; i<imageBufferViews.size(); i++) ss << "{\"bufferView\":" << imageBufferViews[i] << ",\"mimeType\":\"image/png\"}" << (i == imageBufferViews.size()-1 ? "" : ",");
        ss << "],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],";
    }

    ss << "\"accessors\":[";
    for(size_t i=0; i<accessors.size(); i++) {
        const auto& a = accessors[i];
        ss << "{\"bufferView\":" << a.bufferView << ",\"componentType\":" << a.componentType << ",\"count\":" << a.count << ",\"type\":\"" << a.type << "\"";
        if(a.hasMinMax) ss << ",\"min\":[" << a.min[0] << "," << a.min[1] << "," << a.min[2] << "],\"max\":[" << a.max[0] << "," << a.max[1] << "," << a.max[2] << "]";
        ss << "}" << (i == accessors.size()-1 ? "" : ",");
    }
    ss << "],\"bufferViews\":[";
    for(size_t i=0; i<bufferViews.size(); i++) {
        const auto& bv = bufferViews[i];
        ss << "{\"buffer\":0,\"byteOffset\":" << bv.offset << ",\"byteLength\":" << bv.length;
        if(bv.target != 0) ss << ",\"target\":" << bv.target;
        ss << "}" << (i == bufferViews.size()-1 ? "" : ",");
    }
    ss << "],\"buffers\":[{\"byteLength\":" << (uint32_t)binBuffer.size() << "}]}";
    
    std::string json = ss.str();
    while (json.size() % 4 != 0) json += " ";
    WritePadding(binBuffer);

    std::ofstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;
    uint32_t magic = 0x46546C67, version = 2, totalLength = 12 + 8 + (uint32_t)json.size() + 8 + (uint32_t)binBuffer.size();
    f.write((char*)&magic, 4); f.write((char*)&version, 4); f.write((char*)&totalLength, 4);
    uint32_t chunk0Len = (uint32_t)json.size(), chunk0Type = 0x4E4F534A;
    f.write((char*)&chunk0Len, 4); f.write((char*)&chunk0Type, 4); f.write(json.data(), json.size());
    uint32_t chunk1Len = (uint32_t)binBuffer.size(), chunk1Type = 0x004E4942;
    f.write((char*)&chunk1Len, 4); f.write((char*)&chunk1Type, 4); f.write((char*)binBuffer.data(), binBuffer.size());
    f.close();
    return true;
}

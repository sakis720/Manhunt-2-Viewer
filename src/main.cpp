#include "bsp_parser.hpp"
#include "tex_parser.hpp"
#include "file_dialog.hpp"
#include "gltf_export.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "rlImGui.h"
#include "imgui.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <fstream>

#include "win32_utils.hpp"

// ─── Constants & Palette ───────────────────────────────────────────────────

static const Color MESH_COLORS[] = {
    { 70, 130, 180, 255 },   // steel blue
    { 178, 102, 102, 255 },  // rosy
    { 102, 178, 102, 255 },  // sage green
    { 178, 178, 102, 255 },  // olive
    { 140, 102, 178, 255 },  // lavender
    { 102, 178, 178, 255 },  // teal
    { 200, 140, 100, 255 },  // sand
    { 160, 120, 160, 255 },  // mauve
    { 100, 160, 200, 255 },  // sky
    { 180, 130, 100, 255 },  // copper
};
static const int NUM_COLORS = sizeof(MESH_COLORS) / sizeof(MESH_COLORS[0]);

// ─── Structs ───────────────────────────────────────────────────────────────

struct ViewerMesh {
    Model  rModel;
    Color  baseColor;
    bool   visible = true;
};

class App {
public:
    App() {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
        InitWindow(1280, 720, "Manhunt 2 Viewer");
        SetTargetFPS(60);
        rlImGuiSetup(true);
        ImGui::GetIO().IniFilename = nullptr;

        // Set window icon from embedded resource (Win32 specific)
        SetWin32WindowIcon(GetWindowHandle());

        // Load Logo Texture for UI
        const char* logoPaths[] = { 
            "src/resources/icon/logo.ico", 
            "resources/icon/logo.ico", 
            "logo.ico" 
        };
        for (const char* path : logoPaths) {
            if (FileExists(path)) {
                Image logoImg = LoadImage(path);
                if (logoImg.data) {
                    logoTexture = LoadTextureFromImage(logoImg);
                    UnloadImage(logoImg);
                    break;
                }
            }
        }

        camera.position   = { 50.0f, 50.0f, 50.0f };
        camera.target     = { 0.0f, 0.0f, 0.0f };
        camera.up         = { 0.0f, 1.0f, 0.0f };
        camera.fovy       = 60.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        orbitTarget = camera.target;
        UpdateCameraTransform();
    }

    ~App() {
        if (logoTexture.id != 0) UnloadTexture(logoTexture);
        UnloadAllAssets();
        rlImGuiShutdown();
        CloseWindow();
    }

    void Run(int argc, char* argv[]) {
        ApplyModernTheme();
        if (argc > 1) {
            LoadFile(argv[1]);
        }

        while (!WindowShouldClose()) {
            Update();
            Draw();
        }
    }

private:
    // State
    Camera3D camera = { 0 };
    std::vector<ViewerMesh> viewerMeshes;
    std::map<std::string, Texture2D> loadedTextures;
    std::map<std::string, std::vector<uint8_t>> loadedDDS; // Stores raw bytes for export
    BSPFile bspFile;

    Texture2D logoTexture = { 0 };
    
    bool fileLoaded = false;
    bool wireframeMode = false;
    bool showGrid = true;
    bool showNormals = false;
    bool useVertexColors = true;
    bool showTextures = true;
    bool flyMode = false;

    std::string currentFileName = "";
    std::string currentFilePath = "";

    // Windows State
    bool showUVWindow = false;
    bool showTextureWindow = false;
    bool showAboutWindow = false;
    
    int selectedMeshIdx = 0;
    int selectedSubMeshIdx = 0;
    std::string selectedTextureName = "";

    // Stats
    int totalVerts = 0;
    int totalTris = 0;

    // Camera State
    float orbitAngleH = 0.7f;
    float orbitAngleV = 0.4f;
    float orbitDistance = 100.0f;
    Vector3 orbitTarget = { 0, 0, 0 };
    float flySpeed = 20.0f;

    // Notification
    struct Notification {
        std::string message;
        Color color;
        float timer;
    } notification = { "", WHITE, 0.0f };

    void ApplyModernTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        style.WindowRounding = 6.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;

        colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]            = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.12f, 0.94f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.16f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.26f, 0.26f, 0.35f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.24f, 0.24f, 0.32f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.15f, 0.15f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.38f, 0.58f, 0.94f, 1.00f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.38f, 0.58f, 0.94f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.48f, 0.68f, 1.00f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.24f, 0.50f, 0.82f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.26f, 0.26f, 0.35f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.15f, 0.86f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.41f, 0.68f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }

    void Notify(const std::string& msg, Color color = RAYWHITE, float duration = 3.0f) {
        notification = { msg, color, duration };
    }

    void UnloadAllAssets() {
        for (auto& vm : viewerMeshes) {
            UnloadModel(vm.rModel);
        }
        viewerMeshes.clear();
        for (auto& pair : loadedTextures) {
            UnloadTexture(pair.second);
        }
        loadedTextures.clear();
        loadedDDS.clear(); // Simply clear the vectors
        totalVerts = 0;
        totalTris = 0;
        selectedTextureName = "";
    }

    void LoadTexturesFromFile(const std::string& path) {
        TexFile texFile = LoadTex(path);
        if (texFile.error.empty()) {
            int loadedCount = 0;
            for (auto& pair : texFile.textures) {
                if (pair.second.ddsData.empty()) continue;
                
                // Store raw DDS bytes for export (Memory efficient)
                loadedDDS[pair.first] = pair.second.ddsData;

                // Load to GPU for rendering
                Image img = LoadImageFromMemory(".dds", pair.second.ddsData.data(), (int)pair.second.ddsData.size());
                if (img.data) {
                    if (loadedTextures.count(pair.first)) UnloadTexture(loadedTextures[pair.first]);
                    loadedTextures[pair.first] = LoadTextureFromImage(img);
                    UnloadImage(img);
                    
                    if (selectedTextureName.empty()) selectedTextureName = pair.first;
                    loadedCount++;
                }
            }
            Notify("Loaded " + std::to_string(loadedCount) + " textures from " + GetFileName(path.c_str()), GREEN);
        }
    }

    void LoadFile(const std::string& path) {
        UnloadAllAssets();

        BSPLoadOptions options;
        bspFile = LoadBSP(path, options);
        if (!bspFile.error.empty()) {
            Notify("Error: " + bspFile.error, RED, 5.0f);
            fileLoaded = false;
            return;
        }

        currentFilePath = path;
        currentFileName = GetFileName(path.c_str());

        // Automatic texture lookup
        std::string base = path;
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        std::string texPath = base + ".tex";
        std::string txdPath = base + ".txd";

        if (FileExists(texPath.c_str())) LoadTexturesFromFile(texPath);
        else if (FileExists(txdPath.c_str())) LoadTexturesFromFile(txdPath);

        // Log Detailed BSP Info to bsp_debug.log
        std::ofstream log("bsp_debug.log", std::ios::trunc);
        if (log.is_open()) {
            log << "BSP Debug Log\n";
            log << "File: " << path << "\n";
            log << "Compressed: " << (bspFile.compressed ? "Yes" : "No") << "\n";
            log << "Total Meshes (Chunks): " << bspFile.meshes.size() << "\n";
            log << "Total Materials Defined: " << bspFile.materialNames.size() << "\n\n";

            log << "--- Mesh & SubMesh Details ---\n";
            for (size_t i = 0; i < bspFile.meshes.size(); i++) {
                const auto& mesh = bspFile.meshes[i];
                std::string typeDesc = "Unknown";
                if (mesh.vertexElementType == 0x52) typeDesc = "Pos+Norm";
                else if (mesh.vertexElementType == 0x152) typeDesc = "Pos+Norm+UV1";
                else if (mesh.vertexElementType == 0x252) typeDesc = "Pos+Norm+UV1+UV2";

                log << "Mesh [" << i << "] at Offset: 0x" << std::hex << std::uppercase << mesh.offset << std::dec << "\n";
                log << "  Vertices: " << mesh.vertices.size() << " (Type: 0x" << std::hex << mesh.vertexElementType << std::dec << " - " << typeDesc << ")\n";
                log << "  SubMeshes: " << mesh.subMeshes.size() << "\n";
                
                for (size_t j = 0; j < mesh.subMeshes.size(); j++) {
                    const auto& sub = mesh.subMeshes[j];
                    std::string matName = (sub.materialIndex < bspFile.materialNames.size()) ? bspFile.materialNames[sub.materialIndex] : "INVALID";
                    
                    size_t slash = matName.find_last_of("\\/");
                    std::string nameNoPath = (slash != std::string::npos) ? matName.substr(slash + 1) : matName;
                    bool texFound = (loadedTextures.count(matName) > 0 || loadedTextures.count(nameNoPath) > 0);
                    if (!texFound) {
                        std::string lowerMat = matName; std::transform(lowerMat.begin(), lowerMat.end(), lowerMat.begin(), ::tolower);
                        std::string lowerNoPath = nameNoPath; std::transform(lowerNoPath.begin(), lowerNoPath.end(), lowerNoPath.begin(), ::tolower);
                        for (auto const& [name, tex] : loadedTextures) {
                            std::string lowerLoaded = name; std::transform(lowerLoaded.begin(), lowerLoaded.end(), lowerLoaded.begin(), ::tolower);
                            if (lowerLoaded == lowerMat || lowerLoaded == lowerNoPath) { texFound = true; break; }
                        }
                    }

                    log << "    SubMesh [" << j << "] Offset: 0x" << std::hex << sub.offset << std::dec 
                        << ", Material Index: " << sub.materialIndex 
                        << " (\"" << matName << "\"), Triangles: " << (sub.indices.size() / 3)
                        << " -> Texture: " << (texFound ? "FOUND" : "MISSING") << "\n";
                }
                log << "\n";
            }

            log << "--- Material Summary ---\n";
            for (size_t i = 0; i < bspFile.materialNames.size(); i++) {
                std::string matName = bspFile.materialNames[i];
                size_t slash = matName.find_last_of("\\/");
                std::string nameNoPath = (slash != std::string::npos) ? matName.substr(slash + 1) : matName;
                bool found = (loadedTextures.count(matName) > 0 || loadedTextures.count(nameNoPath) > 0);
                if (!found) {
                    std::string lowerMat = matName; std::transform(lowerMat.begin(), lowerMat.end(), lowerMat.begin(), ::tolower);
                    std::string lowerNoPath = nameNoPath; std::transform(lowerNoPath.begin(), lowerNoPath.end(), lowerNoPath.begin(), ::tolower);
                    for (auto const& [name, tex] : loadedTextures) {
                        std::string lowerLoaded = name; std::transform(lowerLoaded.begin(), lowerLoaded.end(), lowerLoaded.begin(), ::tolower);
                        if (lowerLoaded == lowerMat || lowerLoaded == lowerNoPath) { found = true; break; }
                    }
                }
                log << "  Material [" << i << "]: \"" << matName << "\" [" << (found ? "FOUND" : "MISSING") << "]\n";
            }
            log << "--- End of Log ---\n" << std::endl;
            log.close();
        }

        for (int i = 0; i < (int)bspFile.meshes.size(); i++) {
            viewerMeshes.push_back(CreateViewerMesh(bspFile.meshes[i], i));
            for (int m = 0; m < viewerMeshes.back().rModel.meshCount; m++) {
                totalVerts += viewerMeshes.back().rModel.meshes[m].vertexCount;
                totalTris += viewerMeshes.back().rModel.meshes[m].triangleCount;
            }
        }

        BoundingBox bb = ComputeSceneBounds();
        orbitTarget = { (bb.min.x + bb.max.x) * 0.5f, (bb.min.y + bb.max.y) * 0.5f, (bb.min.z + bb.max.z) * 0.5f };
        orbitDistance = Vector3Distance(bb.min, bb.max) * 0.7f;
        if (orbitDistance < 1.0f) orbitDistance = 10.0f;
        if (flyMode) { camera.position = Vector3Add(orbitTarget, {0, 0, orbitDistance}); camera.target = orbitTarget; }

        fileLoaded = true;
        Notify("Loaded " + currentFileName, GREEN);
    }

    ViewerMesh CreateViewerMesh(const BSPMesh& bspMesh, int colorIdx) {
        ViewerMesh vm{};
        vm.baseColor = MESH_COLORS[colorIdx % NUM_COLORS];
        if (bspMesh.vertices.empty() || bspMesh.subMeshes.empty()) return vm;

        Model model = { 0 };
        model.meshCount = (int)bspMesh.subMeshes.size();
        model.materialCount = (int)bspMesh.subMeshes.size();
        model.meshes = (Mesh*)MemAlloc(model.meshCount * sizeof(Mesh));
        model.materials = (Material*)MemAlloc(model.materialCount * sizeof(Material));
        model.meshMaterial = (int*)MemAlloc(model.meshCount * sizeof(int));
        std::memset(model.meshes, 0, model.meshCount * sizeof(Mesh));
        std::memset(model.meshMaterial, 0, model.meshCount * sizeof(int));
        model.transform = MatrixIdentity();

        for (int i = 0; i < model.meshCount; i++) {
            const auto& sub = bspMesh.subMeshes[i];
            Mesh& mesh = model.meshes[i];
            
            std::vector<int> usedVertices;
            std::map<int, int> oldToNew;
            for (auto oldIdx : sub.indices) {
                if (oldToNew.find(oldIdx) == oldToNew.end()) {
                    oldToNew[oldIdx] = (int)usedVertices.size();
                    usedVertices.push_back(oldIdx);
                }
            }

            mesh.vertexCount = (int)usedVertices.size();
            mesh.triangleCount = (int)(sub.indices.size() / 3);
            mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
            mesh.normals = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
            mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
            mesh.colors = (unsigned char*)MemAlloc(mesh.vertexCount * 4 * sizeof(unsigned char));
            mesh.indices = (unsigned short*)MemAlloc(sub.indices.size() * sizeof(unsigned short));

            for (int v = 0; v < mesh.vertexCount; v++) {
                int oldIdx = usedVertices[v];
                const auto& bv = bspMesh.vertices[oldIdx];
                mesh.vertices[v * 3 + 0] = bv.x; mesh.vertices[v * 3 + 1] = bv.y; mesh.vertices[v * 3 + 2] = bv.z;
                mesh.normals[v * 3 + 0] = bv.nx; mesh.normals[v * 3 + 1] = bv.ny; mesh.normals[v * 3 + 2] = bv.nz;
                mesh.texcoords[v * 2 + 0] = bv.uv1[0]; mesh.texcoords[v * 2 + 1] = bv.uv1[1];
                mesh.colors[v * 4 + 0] = bv.color[2]; mesh.colors[v * 4 + 1] = bv.color[1]; 
                mesh.colors[v * 4 + 2] = bv.color[0]; mesh.colors[v * 4 + 3] = bv.color[3];
            }
            for (size_t idx = 0; idx < sub.indices.size(); idx++) mesh.indices[idx] = (unsigned short)oldToNew[sub.indices[idx]];
            UploadMesh(&mesh, false);

            model.materials[i] = LoadMaterialDefault();
            model.meshMaterial[i] = i;

            if (sub.materialIndex < bspFile.materialNames.size()) {
                std::string matName = bspFile.materialNames[sub.materialIndex];
                size_t slash = matName.find_last_of("\\/");
                std::string nameNoPath = (slash != std::string::npos) ? matName.substr(slash + 1) : matName;
                
                if (loadedTextures.count(matName)) model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = loadedTextures[matName];
                else if (loadedTextures.count(nameNoPath)) model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = loadedTextures[nameNoPath];
                else {
                    std::string lowerMat = matName; std::transform(lowerMat.begin(), lowerMat.end(), lowerMat.begin(), ::tolower);
                    std::string lowerNoPath = nameNoPath; std::transform(lowerNoPath.begin(), lowerNoPath.end(), lowerNoPath.begin(), ::tolower);
                    for (auto const& [name, tex] : loadedTextures) {
                        std::string lowerLoaded = name; std::transform(lowerLoaded.begin(), lowerLoaded.end(), lowerLoaded.begin(), ::tolower);
                        if (lowerLoaded == lowerMat || lowerLoaded == lowerNoPath) { model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture = tex; break; }
                    }
                }
            }
        }
        vm.rModel = model;
        return vm;
    }

    BoundingBox ComputeSceneBounds() {
        BoundingBox bb = { { 1e18f, 1e18f, 1e18f }, { -1e18f, -1e18f, -1e18f } };
        for (auto& m : bspFile.meshes) {
            for (auto& v : m.vertices) {
                bb.min = { std::min(bb.min.x, v.x), std::min(bb.min.y, v.y), std::min(bb.min.z, v.z) };
                bb.max = { std::max(bb.max.x, v.x), std::max(bb.max.y, v.y), std::max(bb.max.z, v.z) };
            }
        }
        if (bspFile.meshes.empty()) bb = { { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } };
        return bb;
    }

    void Update() {
        if (IsFileDropped()) {
            FilePathList files = LoadDroppedFiles();
            if (files.count > 0) LoadFile(files.paths[0]);
            UnloadDroppedFiles(files);
        }
        if (IsKeyPressed(KEY_F)) {
             BoundingBox bb = ComputeSceneBounds();
             orbitTarget = { (bb.min.x + bb.max.x) * 0.5f, (bb.min.y + bb.max.y) * 0.5f, (bb.min.z + bb.max.z) * 0.5f };
             orbitDistance = Vector3Distance(bb.min, bb.max) * 0.7f;
        }
        if (IsKeyPressed(KEY_W) && !flyMode) wireframeMode = !wireframeMode;
        if (IsKeyPressed(KEY_G)) showGrid = !showGrid;
        if (IsKeyPressed(KEY_N)) showNormals = !showNormals;
        if (IsKeyPressed(KEY_U)) showUVWindow = !showUVWindow;
        if (IsKeyPressed(KEY_T)) showTextureWindow = !showTextureWindow;

        if (!ImGui::GetIO().WantCaptureMouse) { if (flyMode) UpdateFlyCamera(); else UpdateOrbitCamera(); }
        if (!flyMode) UpdateCameraTransform();
        if (notification.timer > 0) notification.timer -= GetFrameTime();
    }

    void UpdateOrbitCamera() {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            orbitAngleH -= delta.x * 0.005f; orbitAngleV += delta.y * 0.005f;
            orbitAngleV = std::clamp(orbitAngleV, -1.5f, 1.5f);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_SHIFT))) {
            Vector2 delta = GetMouseDelta();
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
            Vector3 up = Vector3CrossProduct(right, forward);
            float panSpeed = orbitDistance * 0.001f;
            orbitTarget = Vector3Add(orbitTarget, Vector3Add(Vector3Scale(right, -delta.x * panSpeed), Vector3Scale(up, delta.y * panSpeed)));
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) { orbitDistance -= wheel * orbitDistance * 0.1f; orbitDistance = std::max(orbitDistance, 0.1f); }
    }

    void UpdateFlyCamera() {
        float dt = GetFrameTime(); float speed = flySpeed; if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 3.0f;
        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
        if (IsKeyDown(KEY_W)) camera.position = Vector3Add(camera.position, Vector3Scale(forward, speed * dt));
        if (IsKeyDown(KEY_S)) camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, speed * dt));
        if (IsKeyDown(KEY_A)) camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed * dt));
        if (IsKeyDown(KEY_D)) camera.position = Vector3Add(camera.position, Vector3Scale(right, speed * dt));
        if (IsKeyDown(KEY_E)) camera.position.y += speed * dt;
        if (IsKeyDown(KEY_Q)) camera.position.y -= speed * dt;
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta(); orbitAngleH -= delta.x * 0.003f; orbitAngleV -= delta.y * 0.003f;
            orbitAngleV = std::clamp(orbitAngleV, -1.5f, 1.5f);
        }
        camera.target = { camera.position.x + sinf(orbitAngleH) * cosf(orbitAngleV), camera.position.y + sinf(orbitAngleV), camera.position.z + cosf(orbitAngleH) * cosf(orbitAngleV) };
    }

    void UpdateCameraTransform() {
        camera.target = orbitTarget;
        camera.position = { orbitTarget.x + orbitDistance * cosf(orbitAngleV) * sinf(orbitAngleH), orbitTarget.y + orbitDistance * sinf(orbitAngleV), orbitTarget.z + orbitDistance * cosf(orbitAngleV) * cosf(orbitAngleH) };
    }

    void DrawUVWindow() {
        if (!showUVWindow || !fileLoaded) return;
        ImGui::SetNextWindowSize({ 400, 450 }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("UV Viewer", &showUVWindow)) {
            if (ImGui::BeginCombo("Select Chunk", (std::string("Chunk ") + std::to_string(selectedMeshIdx)).c_str())) {
                for (int i = 0; i < (int)viewerMeshes.size(); i++) {
                    if (ImGui::Selectable((std::string("Chunk ") + std::to_string(i)).c_str(), selectedMeshIdx == i)) {
                        selectedMeshIdx = i; selectedSubMeshIdx = 0;
                    }
                }
                ImGui::EndCombo();
            }
            if (selectedMeshIdx < (int)viewerMeshes.size()) {
                const auto& vm = viewerMeshes[selectedMeshIdx];
                if (ImGui::BeginCombo("Select Sub-Mesh", (std::string("Sub-Mesh ") + std::to_string(selectedSubMeshIdx)).c_str())) {
                    for (int i = 0; i < vm.rModel.meshCount; i++) {
                        if (ImGui::Selectable((std::string("Sub-Mesh ") + std::to_string(i)).c_str(), selectedSubMeshIdx == i)) selectedSubMeshIdx = i;
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::Separator();
            ImVec2 region = ImGui::GetContentRegionAvail();
            float size = std::min(region.x, region.y);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(p, { p.x + size, p.y + size }, IM_COL32(30, 30, 30, 255));
            draw->AddRect(p, { p.x + size, p.y + size }, IM_COL32(200, 200, 200, 255));

            if (selectedMeshIdx < (int)viewerMeshes.size() && selectedMeshIdx < (int)bspFile.meshes.size()) {
                const auto& rMesh = viewerMeshes[selectedMeshIdx].rModel.meshes[selectedSubMeshIdx];
                for (int i = 0; i < rMesh.triangleCount; i++) {
                    int i0 = rMesh.indices[i * 3 + 0], i1 = rMesh.indices[i * 3 + 1], i2 = rMesh.indices[i * 3 + 2];
                    ImVec2 v0 = { p.x + rMesh.texcoords[i0 * 2 + 0] * size, p.y + rMesh.texcoords[i0 * 2 + 1] * size };
                    ImVec2 v1 = { p.x + rMesh.texcoords[i1 * 2 + 0] * size, p.y + rMesh.texcoords[i1 * 2 + 1] * size };
                    ImVec2 v2 = { p.x + rMesh.texcoords[i2 * 2 + 0] * size, p.y + rMesh.texcoords[i2 * 2 + 1] * size };
                    draw->AddTriangle(v0, v1, v2, IM_COL32(0, 255, 0, 150));
                }
            }
        }
        ImGui::End();
    }

    void DrawTextureWindow() {
        if (!showTextureWindow) return;
        ImGui::SetNextWindowSize({ 500, 500 }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Texture Viewer", &showTextureWindow)) {
            ImGui::BeginChild("TexList", { 150, 0 }, true);
            for (auto const& [name, tex] : loadedTextures) {
                if (ImGui::Selectable(name.c_str(), selectedTextureName == name)) selectedTextureName = name;
            }
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("TexView");
            if (loadedTextures.count(selectedTextureName)) {
                Texture2D tex = loadedTextures[selectedTextureName];
                ImGui::Text("%s (%dx%d)", selectedTextureName.c_str(), tex.width, tex.height);
                float regionW = ImGui::GetContentRegionAvail().x;
                float aspect = (float)tex.height / (float)tex.width;
                float drawW = regionW;
                float drawH = drawW * aspect;
                rlImGuiImageSize(&tex, (int)drawW, (int)drawH);
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void DrawAboutWindow() {
        if (!showAboutWindow) return;
        
        ImGui::SetNextWindowPos({ ImGui::GetIO().DisplaySize.x - 330, 40 }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 320, 320 }, ImGuiCond_Always);
        if (ImGui::Begin("About", &showAboutWindow, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
            if (logoTexture.id != 0) {
                float size = 80.0f;
                ImGui::SetCursorPosX((ImGui::GetWindowSize().x - size) * 0.5f);
                rlImGuiImageSize(&logoTexture, (int)size, (int)size);
                ImGui::Spacing();
            }
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("Manhunt 2 Viewer").x) * 0.5f);
            ImGui::TextColored({0.4f, 0.7f, 1.0f, 1.0f}, "Manhunt 2 Viewer");
            ImGui::Separator();
            ImGui::Text("Version: 0.7.0");
            ImGui::Text("Features:");
            ImGui::BulletText("BSP Geometry Support");
            ImGui::BulletText("TEX Texture Support");
            ImGui::BulletText("GLTF 2.0/OBJ Export");
            ImGui::Spacing();
            ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, "by sakis720");
            if (ImGui::Button("Close", {-1, 0})) showAboutWindow = false;
        }
        ImGui::End();
    }

    void Draw() {
        BeginDrawing();
        ClearBackground({ 18, 18, 25, 255 });
        BeginMode3D(camera);
        if (showGrid) DrawGrid(20, 10.0f);
        for (auto& vm : viewerMeshes) {
            if (!vm.visible) continue;
            for (int i = 0; i < vm.rModel.materialCount; i++) vm.rModel.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = useVertexColors ? WHITE : vm.baseColor;
            DrawModel(vm.rModel, { 0, 0, 0 }, 1.0f, WHITE);
            if (wireframeMode) DrawModelWires(vm.rModel, { 0, 0, 0 }, 1.0f, { 255, 255, 255, 100 });
            if (showNormals) {
                for (int m = 0; m < vm.rModel.meshCount; m++) {
                    Mesh& mesh = vm.rModel.meshes[m];
                    for (int i = 0; i < mesh.vertexCount; i++) {
                        Vector3 pos = { mesh.vertices[i*3], mesh.vertices[i*3+1], mesh.vertices[i*3+2] };
                        Vector3 normal = { mesh.normals[i*3], mesh.normals[i*3+1], mesh.normals[i*3+2] };
                        DrawLine3D(pos, Vector3Add(pos, Vector3Scale(normal, 0.5f)), LIME);
                    }
                }
            }
        }
        EndMode3D();
        DrawUI();
        EndDrawing();
    }

    void DrawUI() {
        rlImGuiBegin();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open BSP...", "Ctrl+O")) { std::string path = OpenBSPFileDialog(GetWindowHandle()); if (!path.empty()) LoadFile(path); }
                if (ImGui::MenuItem("Open Tex...", "Ctrl+T")) {
                    std::string path = OpenTexFileDialog(GetWindowHandle());
                    if (!path.empty()) {
                        LoadTexturesFromFile(path);
                        if (fileLoaded) { for (auto& vm : viewerMeshes) UnloadModel(vm.rModel); viewerMeshes.clear(); for (int i = 0; i < (int)bspFile.meshes.size(); i++) viewerMeshes.push_back(CreateViewerMesh(bspFile.meshes[i], i)); }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("UV Viewer (U)", nullptr, &showUVWindow);
                ImGui::MenuItem("Texture Viewer (T)", nullptr, &showTextureWindow);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("About")) {
                showAboutWindow = !showAboutWindow;
            }
            ImGui::EndMainMenuBar();
        }

        ImGui::SetNextWindowPos({ 10, 40 }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({ 320, 600 }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
            if (ImGui::BeginTabBar("ControlTabs")) {
                if (ImGui::BeginTabItem("Scene")) {
                    if (fileLoaded) {
                        ImGui::TextColored({0.4f, 0.7f, 1.0f, 1.0f}, "File: %s", currentFileName.c_str());
                        ImGui::Separator();
                        ImGui::Text("Chunks:    %d", (int)viewerMeshes.size());
                        ImGui::Text("Materials: %d", (int)bspFile.materialNames.size());
                        ImGui::Text("Vertices:  %d", totalVerts);
                        ImGui::Text("Triangles: %d", totalTris);
                        ImGui::Separator();

                        if (ImGui::Button("Show All", {140, 25})) for (auto& vm : viewerMeshes) vm.visible = true;
                        ImGui::SameLine();
                        if (ImGui::Button("Hide All", {140, 25})) for (auto& vm : viewerMeshes) vm.visible = false;

                        if (ImGui::BeginChild("ChunkList", { 0, 0 }, true)) {
                            for (int i = 0; i < (int)viewerMeshes.size(); i++) {
                                char label[64]; snprintf(label, sizeof(label), "Chunk %d (%d sub-meshes)", i, viewerMeshes[i].rModel.meshCount);
                                ImGui::Checkbox(label, &viewerMeshes[i].visible);
                            }
                        }
                        ImGui::EndChild();
                    } else {
                        if (logoTexture.id != 0) {
                            float size = 128.0f;
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
                            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - size) * 0.5f);
                            rlImGuiImageSize(&logoTexture, (int)size, (int)size);
                            ImGui::Spacing();
                        }
                        ImGui::TextWrapped("No scene loaded. Use File -> Open or Drag & Drop a .bsp file.");
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Rendering")) {
                    ImGui::TextColored({0.4f, 0.7f, 1.0f, 1.0f}, "Camera Settings");
                    ImGui::Checkbox("Fly Mode", &flyMode);
                    if (flyMode) ImGui::SliderFloat("Speed", &flySpeed, 1.0f, 200.0f);
                    if (ImGui::Button("Focus Scene (F)", {-1, 25})) { 
                        BoundingBox bb = ComputeSceneBounds(); orbitTarget = { (bb.min.x + bb.max.x) * 0.5f, (bb.min.y + bb.max.y) * 0.5f, (bb.min.z + bb.max.z) * 0.5f }; orbitDistance = Vector3Distance(bb.min, bb.max) * 0.7f; 
                    }
                    
                    ImGui::Separator();
                    ImGui::TextColored({0.4f, 0.7f, 1.0f, 1.0f}, "Visuals");
                    ImGui::Checkbox("Wireframe (W)", &wireframeMode);
                    ImGui::Checkbox("Grid (G)", &showGrid);
                    ImGui::Checkbox("Normals (N)", &showNormals);
                    ImGui::Checkbox("Vertex Colors", &useVertexColors);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Export")) {
                    if (fileLoaded) {
                        ImGui::TextColored({0.4f, 0.7f, 1.0f, 1.0f}, "Export Options");
                        ImGui::Spacing();
                        if (ImGui::Button("Export to Wavefront (.obj)", { -1, 35 })) {
                            std::string exportPath = currentFilePath; size_t dot = exportPath.find_last_of('.'); if (dot != std::string::npos) exportPath = exportPath.substr(0, dot); exportPath += ".obj"; if (ExportToOBJ(bspFile, exportPath)) Notify("Exported to " + exportPath, GREEN); else Notify("Export FAILED", RED);
                        }
                        ImGui::Spacing();
                        if (ImGui::Button("Export to GLTF Binary (.glb)", { -1, 35 })) {
                            std::string exportPath = currentFilePath; size_t dot = exportPath.find_last_of('.'); if (dot != std::string::npos) exportPath = exportPath.substr(0, dot); exportPath += ".glb";
                            if (ExportToGLTF(bspFile, exportPath, loadedTextures, loadedDDS)) Notify("Exported to " + exportPath, GREEN);
                            else Notify("Export FAILED", RED);
                        }
                        ImGui::Separator();
                        ImGui::TextWrapped("GLB exports include embedded textures and separate PNGs.");
                    } else {
                        ImGui::TextDisabled("Load a file to enable export.");
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        DrawUVWindow();
        DrawTextureWindow();
        DrawAboutWindow();

        if (notification.timer > 0) {
            float alpha = std::min(1.0f, notification.timer);
            ImGui::SetNextWindowPos({ ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y - 60 }, ImGuiCond_Always, { 0.5f, 0.5f });
            ImGui::Begin("##Notification", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
            ImGui::TextColored({ notification.color.r / 255.0f, notification.color.g / 255.0f, notification.color.b / 255.0f, alpha }, "%s", notification.message.c_str());
            ImGui::End();
        }
        rlImGuiEnd();
    }
};

int main(int argc, char* argv[]) { App app; app.Run(argc, argv); return 0; }

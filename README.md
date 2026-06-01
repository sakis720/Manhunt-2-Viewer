# Manhunt 2 Viewer

3D map viewer for **Manhunt 2**. This tool allows you to explore the game's level geometry (BSP), view textures, inspect UV layouts, and export the map geometry.

## Features

- **Advanced BSP Parsing:** Support for Manhunt 2 level geometry including multiple UV sets and vertex colors.
- **Texture Support:** Automatic loading and previewing of textures from `.tex` archives.
- **Support Export:**
    - **GLTF 2.0 (.glb):** Fully self-contained exports with embedded textures, vertex colors.
    - **OBJ Export:** Standard wavefront OBJ export for basic geometry needs.

## Building from Source

### Dependencies
The project uses the following libraries (included in `deps/`):
- [raylib](https://github.com/raysan5/raylib) - Core rendering and windowing.
- [Dear ImGui](https://github.com/ocornut/imgui) - User interface.
- [rlImGui](https://github.com/raylib-extras/rlImGui) - ImGui integration for raylib.
- [zlib](https://github.com/madler/zlib) - Decompression support.

### Build Instructions (Windows)
1. Clone the repository with submodules:
   ```bash
   git clone --recursive https://github.com/yourusername/Manhunt2Viewer.git
   ```
2. Run the provided build script:
   ```bash
   .\build.bat
   ```
   The executable will be located at `build\Release\Manhunt2Viewer.exe`.
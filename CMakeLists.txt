cmake_minimum_required(VERSION 3.14)
project(Manhunt2Viewer CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies in deps/
set(RAYLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/raylib)
set(ZLIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/zlib)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui)
set(RLIMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/rlImGui)

# --- ZLIB (Source Integration) ---
set(ZLIB_SOURCES
    ${ZLIB_DIR}/adler32.c
    ${ZLIB_DIR}/compress.c
    ${ZLIB_DIR}/crc32.c
    ${ZLIB_DIR}/deflate.c
    ${ZLIB_DIR}/gzclose.c
    ${ZLIB_DIR}/gzlib.c
    ${ZLIB_DIR}/gzread.c
    ${ZLIB_DIR}/gzwrite.c
    ${ZLIB_DIR}/inflate.c
    ${ZLIB_DIR}/infback.c
    ${ZLIB_DIR}/inftrees.c
    ${ZLIB_DIR}/inffast.c
    ${ZLIB_DIR}/trees.c
    ${ZLIB_DIR}/uncompr.c
    ${ZLIB_DIR}/zutil.c
)
if(NOT EXISTS "${ZLIB_DIR}/zconf.h")
    configure_file("${ZLIB_DIR}/zconf.h.in" "${ZLIB_DIR}/zconf.h" COPYONLY)
endif()

# --- Raylib ---
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${RAYLIB_DIR} EXCLUDE_FROM_ALL)

# --- ImGui sources ---
file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

# Application source files (excluding debug utilities)
set(SOURCES 
    "src/main.cpp"
    "src/bsp_parser.cpp"
    "src/tex_parser.cpp"
    "src/file_dialog.cpp"
    "src/gltf_export.cpp"
    "src/utils.cpp"
    "src/win32_utils.cpp"
)

if(WIN32)
    list(APPEND SOURCES "src/resources/resource.rc")
endif()

# Headers for IDE visibility
file(GLOB_RECURSE HEADERS
    "src/*.hpp"
    "src/*.h"
)

# Build the main executable as a WIN32 app (no console)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${IMGUI_SOURCES} ${ZLIB_SOURCES} ${RLIMGUI_DIR}/rlImGui.cpp)

# Links
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 comdlg32)
endif()

# Search paths for headers
target_include_directories(${PROJECT_NAME} PRIVATE 
    src
    ${IMGUI_DIR}
    ${RLIMGUI_DIR}
    ${ZLIB_DIR}
    ${RAYLIB_DIR}/src
)

if(MSVC)
    # Allows us to keep using 'int main()' while being a WIN32 windowed app
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
endif()

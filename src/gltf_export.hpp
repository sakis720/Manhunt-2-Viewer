#pragma once
#include "bsp_parser.hpp"
#include "raylib.h"
#include <string>
#include <map>
#include <vector>

bool ExportToGLTF(const BSPFile& bsp, const std::string& filepath, 
                  const std::map<std::string, Texture2D>& textures,
                  const std::map<std::string, std::vector<uint8_t>>& rawDDS);

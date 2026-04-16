#pragma once

#include <string>
#include <vector>

// Implemented in generated/wasm_character_manifest.cpp (Emscripten builds only).
std::vector<std::string> wasm_get_bmps_for_slug(const std::string &slug);
void wasm_scan_character_slugs(std::vector<std::string> &out);

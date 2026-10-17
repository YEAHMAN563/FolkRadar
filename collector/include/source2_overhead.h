#pragma once
#include <string>

// Extracts and decodes the Source 2 overhead texture for mapName directly from
// CS2's VPK files and writes web/maps/<mapName>.png. No external converter.
bool EnsureSource2OverheadMap(const std::string& mapName, const std::string& webRoot);

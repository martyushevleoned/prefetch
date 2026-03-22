#pragma once

#include "pf_data.hpp"
#include <optional>
#include <string>
#include <vector>

std::optional<PfData> parsePfFile(const std::string& filePath);

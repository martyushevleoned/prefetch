#pragma once

#include "data.hpp"
#include <optional>
#include <string>

namespace prefetch::parser::ntdll
{

std::optional<Record> parseFile(const std::string& filePath);

} // namespace prefetch::parser::ntdll

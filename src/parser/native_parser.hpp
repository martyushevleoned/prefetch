#pragma once

#include "data.hpp"
#include <optional>
#include <string>

namespace prefetch::parser::native
{

std::optional<Record> parseFile(const std::string& filePath);

} // namespace prefetch::parser::native

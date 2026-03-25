#pragma once

#include "data.hpp"
#include <optional>
#include <string>

namespace prefetch
{
namespace parser
{

std::optional<Record> parseFile(const std::string& filePath);

}
} // namespace prefetch

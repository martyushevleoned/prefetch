#pragma once

#include "data.hpp"
#include <optional>
#include <string>

namespace prefetch
{
namespace parser
{
namespace libscca
{

std::optional<Record> parseFile(const std::string& filePath);

}
} // namespace parser
} // namespace prefetch

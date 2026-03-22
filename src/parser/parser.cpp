#include "parser.hpp"
#ifdef NTDLL_PARSER
#include "ntdll_parser.hpp"
#endif
#ifdef LIBSCCA_PARSER
#include "libscca_parser.hpp"
#endif

#include <iostream>

namespace prefetch::parser
{

std::optional<Record> parseFile(const std::string& filename)
{
#ifdef NTDLL_PARSER
    if (std::optional<Record> record = ntdll::parseFile(filename))
    {
        return record;
    }
#endif
#ifdef LIBSCCA_PARSER
    if (std::optional<Record> record = libscca::parseFile(filename))
    {
        return record;
    }
#endif
    return std::nullopt;
}

} // namespace prefetch::parser

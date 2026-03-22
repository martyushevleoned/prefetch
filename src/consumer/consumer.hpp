#pragma once

#include "data.hpp"
#include <memory>
#include <string>

namespace prefetch::consumer
{

class PfConsumer
{
public:
    virtual ~PfConsumer() = default;
    virtual void consume(const Record&) = 0;
};

std::unique_ptr<PfConsumer> make_consumer(const std::string& output);

} // namespace prefetch::consumer

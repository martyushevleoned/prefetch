#pragma once

#include "consumer.hpp"

namespace prefetch::consumer
{

class ConsolePfConsumer : public PfConsumer
{
public:
    void consume(const Record&) override;
};

} // namespace prefetch::consumer

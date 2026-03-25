#include "data.hpp"
#include <fstream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace prefetch
{
namespace consumer
{

class PfConsumer
{
public:
    virtual ~PfConsumer() = default;
    virtual void consume(const Record&) = 0;
};

class PfConsoleConsumer : public PfConsumer
{
public:
    void consume(const Record&) override;
};

std::unique_ptr<PfConsumer> make_consumer(const std::string& fileName);

} // namespace consumer
} // namespace prefetch

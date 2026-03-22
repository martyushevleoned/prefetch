#include "consumer.hpp"
#include "console_consumer.hpp"
#ifdef SQLITE3_CONSUMER
#include "sqlite3_consumer.hpp"
#endif

namespace prefetch::consumer
{

std::unique_ptr<PfConsumer> make_consumer(const std::string& output)
{
    if (output.empty() || output == "console")
    {
        return std::make_unique<ConsolePfConsumer>();
    }
#ifdef SQLITE3_CONSUMER
    if (output.size() > 3 && output.substr(output.size() - 3) == ".db")
    {
        return std::make_unique<Sqlite3PfConsumer>(output);
    }
#endif
    return std::unique_ptr<PfConsumer>();
}

} // namespace prefetch::consumer

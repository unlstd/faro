#include <string>

namespace faro::log {
    namespace color {
        constexpr const char* BLUE    = "\033[94m";
        constexpr const char* CYAN    = "\033[96m";
        constexpr const char* GREEN   = "\033[92m";
        constexpr const char* YELLOW  = "\033[93m";
        constexpr const char* RED     = "\033[91m";
        constexpr const char* MAGENTA = "\033[95m";
        constexpr const char* GRAY    = "\033[90m";
        constexpr const char* RESET   = "\033[0m";
    }
        
    void info(const std::string& message);
    void success(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void status(const std::string& message);
    void step(const std::string& message);
    void debug(const std::string& message);

}

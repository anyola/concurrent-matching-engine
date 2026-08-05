#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <stdexcept>
#include <string>

namespace matching_engine {

    class order_error : public std::runtime_error {
    public:
        explicit order_error(const std::string& message) : std::runtime_error(message) {}
    };

    class invalid_price_error : public order_error {
    public:
        explicit invalid_price_error(const std::string& message) : order_error(message) {}
    };

    class invalid_quantity_error : public order_error {
    public:
        explicit invalid_quantity_error(const std::string& message) : order_error(message) {}
    };

    class invalid_name : public order_error {
    public:
        explicit invalid_name(const std::string& message) : order_error(message) {}
    };

}

#endif
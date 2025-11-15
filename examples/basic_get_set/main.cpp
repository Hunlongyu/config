#include <iostream>
#include <config/config.h>

int main() {
    using namespace config;
    set<std::string>("user", "alice");
    std::cout << get<std::string>("user") << "\n";
    std::cout << get_or<std::string>("missing", std::string("default")) << "\n";
    remove("user");
    std::cout << dump(2) << "\n";
    return 0;
}
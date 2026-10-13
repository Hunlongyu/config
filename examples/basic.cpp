// snippet:begin quick-start
#include <config/config.hpp>
#include <iostream>

int main()
{
    config::Store store("settings.json", {.save = config::SaveMode::manual});
    store.set("/server/port", 8080);
    store.update([](nlohmann::json &values) {
        values["server"]["host"] = "localhost";
        values["theme"]          = "dark";
    });
    store.save();
    std::cout << store.get<int>("/server/port") << '\n';
    return store.get_or<int>("/absent", 80) == 80 ? 0 : 1;
}
// snippet:end quick-start

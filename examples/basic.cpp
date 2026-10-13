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
    const auto fallback  = store.get<int>("/absent", 80);
    const auto generated = store.get<int>("/absent", [] { return 80; });
    const auto optional  = store.find<int>("/absent");
    return fallback == 80 && generated == 80 && !optional ? 0 : 1;
}
// snippet:end quick-start

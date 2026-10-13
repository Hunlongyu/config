#include <config/watcher.hpp>
#include <iostream>

int main()
{
    config::Store store("observed.json", {.validate = [](const nlohmann::json &values) {
                            if (values.value("port", 8080) <= 0)
                                throw std::runtime_error("port must be positive");
                        }});
    auto subscription = store.subscribe(
        [](const config::Change &change) { std::cout << change.revision << ": " << change.after.dump() << '\n'; });
    config::Watcher watcher(store, std::chrono::milliseconds(100),
                            [](const config::Error &error) { std::cerr << error.what() << '\n'; });
    store.set("/port", 8080);
    watcher.stop();
    return store.get<int>("/port") == 8080 ? 0 : 1;
}

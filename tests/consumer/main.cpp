#include <algorithm>
#include <config/config.hpp>
int peer();
int main()
{
    const auto file = std::filesystem::current_path() / "consumer.json";
    {
        config::Store store(file);
        store.set("/value", 42);
        store.reload();
        if (store.get<int>("/value") != 42 || std::max(1, 2) != peer())
            return 1;
    }
    std::filesystem::remove(file);
    return 0;
}

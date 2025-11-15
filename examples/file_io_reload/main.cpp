#include <iostream>
#include <fstream>
#include <config/config.h>

int main() {
    using namespace config;
    config_store store("file_io_reload", SavePolicy::ManualSave, Path::CurrentDir);
    store.set<std::string>("k", "v1");
    store.save();
    std::cout << store.dump(2) << "\n";

    auto path = store.get_file_path();
    {
        std::ofstream ofs(path);
        ofs << "{\n  \"k\": \"v2\"\n}";
        ofs.flush();
    }
    store.reload();
    std::cout << store.dump(2) << "\n";
    return 0;
}
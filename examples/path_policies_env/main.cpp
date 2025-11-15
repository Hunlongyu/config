#include <iostream>
#include <config/config.h>

int main() {
    using namespace config;
    config_store current("path_policies_env_current", SavePolicy::ManualSave, Path::CurrentDir);
    current.set<std::string>("k", "v");
    current.save();
    std::cout << current.get_file_path().string() << "\n";

    config_store appdata("path_policies_env_appdata", SavePolicy::ManualSave, Path::AppData);
    appdata.set<std::string>("k", "v");
    appdata.save();
    std::cout << appdata.get_file_path().string() << "\n";

    config_store autodetect("path_policies_env_auto", SavePolicy::ManualSave, Path::AutoDetect);
    autodetect.set<std::string>("k", "v");
    autodetect.save();
    std::cout << autodetect.get_file_path().string() << "\n";
    return 0;
}
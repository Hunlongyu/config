#include <iostream>
#include <filesystem>
#include <config/config.h>

int main() {
    using namespace config;
    config_store formatted("save_formats_formatted", SavePolicy::ManualSave, Path::CurrentDir);
    formatted.set_save_format(SaveFormat::Formatted);
    formatted.set<std::string>("name", "example");
    formatted.set<int>("count", 3);
    formatted.save();

    config_store compressed("save_formats_compressed", SavePolicy::ManualSave, Path::CurrentDir);
    compressed.set_save_format(SaveFormat::Compressed);
    compressed.set<std::string>("name", "example");
    compressed.set<int>("count", 3);
    compressed.save();

    auto f1 = formatted.get_file_path();
    auto f2 = compressed.get_file_path();
    auto s1 = std::filesystem::file_size(f1);
    auto s2 = std::filesystem::file_size(f2);
    std::cout << f1.string() << " " << s1 << "\n";
    std::cout << f2.string() << " " << s2 << "\n";
    return 0;
}
#include "config/config.h"
#include <iostream>

int main() {
    // 示例 1：最简单使用方式，全默认配置
    config::set("username", "john_doe");
    std::cout << "Username: " << config::get<std::string>("username") << std::endl;


    // 示例 2：使用 AppData 目录存储配置文件
    // 创建一个使用 AppData 路径的配置存储，自动保存策略
    auto appdata_store = config::get_store("appdata_config", config::save_policy::auto_save, config::Path::appdata);
    // 设置键值对：用户名和分数
    appdata_store->set("username", "john_doe");
    appdata_store->set("score", 100);
    // 打印配置文件路径和存储的值
    std::cout << "AppData: " << appdata_store->get_file_path().string() << std::endl;
    std::cout << "username: " << appdata_store->get<std::string>("username") 
              << ", score: " << appdata_store->get<int>("score") << std::endl;

    // 示例 3：使用当前目录存储配置文件
    // 创建一个使用当前目录的配置存储，自动保存策略
    auto current_store = config::get_store("current_config", config::save_policy::auto_save, config::Path::current_dir);
    // 设置主题配置
    current_store->set("theme", "dark");
    // 打印配置文件路径和主题值
    std::cout << "current_config: " << current_store->get_file_path().string() << std::endl;
    std::cout << "theme: " << current_store->get<std::string>("theme") << std::endl;

    // 示例 4：使用自动检测路径策略
    // 创建一个自动检测路径的配置存储（优先 AppData，无权限则回退到当前目录）
    auto auto_store = config::get_store("auto_config", config::save_policy::auto_save, config::Path::auto_detect);
    // 设置语言配置
    auto_store->set("language", "en");
    // 打印配置文件路径和语言值
    std::cout << "auto_config: " << auto_store->get_file_path().string() << std::endl;
    std::cout << "language: " << auto_store->get<std::string>("language") << std::endl;

    return 0;
}
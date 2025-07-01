#include "config/config.h"
#include <iostream>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    using namespace config;

    // 创建一个配置存储
    auto store = get_store("listener_config");

    // 添加监听器，监控 "score" 键的变更
    auto listener_id = store->connect("score", [](const json &old_value, const json &new_value) {
        std::cout << "分数变更 - 旧值: " << (old_value.is_null() ? "无" : old_value.dump())
                  << ", 新值: " << new_value.dump() << std::endl;
    });

    // 添加监听器，监控 "/user/name" 键的变更
    auto listener_id2 = store->connect("/user/name", [](const json &old_value, const json &new_value) {
        std::cout << "用户名 - 旧值: " << (old_value.is_null() ? "无" : old_value.dump())
                  << ", 新值: " << new_value.dump() << std::endl;
    });

    // 设置初始分数，触发监听器
    store->set("score", 100);

    // 更新分数，触发监听器
    store->set("score", 150);

    // 设置其他键（不会触发监听器）
    store->set("username", "Bob");

    store->set("/user/name", "john");

    // 移除监听器
    store->disconnect(listener_id);
    store->disconnect(listener_id2);

    // 再次更新分数（不会触发监听器）
    store->set("score", 200);

    // 打印最终配置
    std::cout << "最终配置:\n" << store->dump(2) << std::endl;

    return 0;
}
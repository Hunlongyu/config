#include "config/config.h"
#include <iostream>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 创建一个配置存储
    auto store = config::get_store("json_pointer_config");

    // 使用 JSON 指针设置嵌套配置
    store->set("/user/profile/name", "Alice");
    store->set("/user/profile/age", 30);
    store->set("/user/settings/theme", "light");

    // 使用 JSON 指针读取嵌套值
    std::cout << "用户名: " << store->get<std::string>("/user/profile/name") << std::endl;
    std::cout << "年龄: " << store->get<int>("/user/profile/age") << std::endl;
    std::cout << "主题: " << store->get<std::string>("/user/settings/theme") << std::endl;

    // 检查嵌套路径是否存在
    std::cout << "是否存在 /user/profile/name: " << (store->contains("/user/profile/name") ? "是" : "否") << std::endl;
    std::cout << "是否存在 /user/invalid: " << (store->contains("/user/invalid") ? "是" : "否") << std::endl;

    // 删除嵌套字段
    store->remove("/user/settings/theme");
    std::cout << "删除主题后，是否存在 /user/settings/theme: "
              << (store->contains("/user/settings/theme") ? "是" : "否") << std::endl;

    // 打印整个配置
    std::cout << "配置内容:\n" << store->dump(2) << std::endl;

    return 0;
}
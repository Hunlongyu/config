#include "config/config.h"
#include <iostream>

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 创建一个配置存储，使用定时保存策略（每5秒保存一次）
    auto store = config::get_store("timed_config", config::save_policy::timed_save);

    // 设置初始值
    store->set("counter", 0);

    // 模拟多次更新
    for (int i = 5; i <= 8; ++i) {
        store->set("counter", i);
        std::cout << "更新计数器: " << i << ", 是否需要保存: " << (store->is_dirty() ? "是" : "否") << std::endl;
        // 等待2秒，模拟批量更新
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // 等待定时保存线程完成（默认5秒间隔）
    std::cout << "等待定时保存..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));

    // 打印最终配置
    std::cout << "最终配置:\n" << store->dump(2) << std::endl;
}
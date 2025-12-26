#include <chrono>
#include <config/config.hpp>
#include <filesystem>
#include <iostream>
#include <vector>

class Timer
{
  public:
    Timer(const std::string &name) : name_(name), start_(std::chrono::high_resolution_clock::now())
    {
    }
    ~Timer()
    {
        const auto end      = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        std::cout << "[" << name_ << "] " << duration / 1000.0 << " ms" << std::endl;
    }

  private:
    std::string name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

void run_benchmark(const int iterations, const bool use_auto_save)
{
    const std::string filename =
        "bench_" + std::to_string(iterations) + "_" + (use_auto_save ? "auto" : "manual") + ".json";

    // Cleanup previous run
    if (std::filesystem::exists(filename))
    {
        std::filesystem::remove(filename);
    }

    const auto save_strategy = use_auto_save ? config::SaveStrategy::Auto : config::SaveStrategy::Manual;
    auto &store              = config::get_store(filename, config::Path::Relative, save_strategy);
    store.clear(); // Ensure clean state

    std::cout << "\n=== Running Benchmark (N=" << iterations << ", Strategy=" << (use_auto_save ? "Auto" : "Manual")
              << ") ===" << std::endl;

    // 1. Write Performance
    {
        Timer t("Write (Set)");
        for (int i = 0; i < iterations; ++i)
        {
            store.set("key_" + std::to_string(i), i);
        }
        if (!use_auto_save)
        {
            store.save(); // Include save time for manual strategy to be fair?
                          // Or measure pure memory set vs disk IO?
                          // Usually set() in Auto mode includes IO.
                          // So for Manual mode, we should measure set() separately from save().
        }
    }

    if (!use_auto_save)
    {
        Timer t("Manual Save (Disk IO)");
        store.save();
    }

    // 2. Read Performance (Memory)
    {
        Timer t("Read (Get)");
        volatile int sum = 0; // Prevent optimization
        for (int i = 0; i < iterations; ++i)
        {
            sum += store.get<int>("key_" + std::to_string(i));
        }
    }

    // 3. Mixed Read/Write
    {
        Timer t("Mixed Read/Write");
        for (int i = 0; i < iterations; ++i)
        {
            store.set("mixed_" + std::to_string(i), i);
            const auto val = store.get<int>("mixed_" + std::to_string(i));
            (void)val;
        }
        if (!use_auto_save)
            store.save();
    }

    // Cleanup
    std::filesystem::remove(filename);
}

int main()
{
    const std::vector<int> counts = {1000, 10000};

    for (const int count : counts)
    {
        run_benchmark(count, false); // Manual Save (Fast, Memory dominant)
        run_benchmark(count, true);  // Auto Save (Slow, IO dominant)
    }

    return 0;
}

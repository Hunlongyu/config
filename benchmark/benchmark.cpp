#include <chrono>
#include <config/config.hpp>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <vector>

class BenchmarkLogger
{
  public:
    BenchmarkLogger(const std::string &filename)
    {
        // Ensure directory exists
        std::filesystem::path p(filename);
        if (p.has_parent_path())
        {
            std::filesystem::create_directories(p.parent_path());
        }
        file_.open(filename, std::ios::app);
        if (!file_.is_open())
        {
            std::cerr << "Failed to open benchmark log file: " << filename << std::endl;
        }
    }

    void log(const std::string &message)
    {
        std::cout << message;
        if (file_.is_open())
        {
            file_ << message;
            file_.flush();
        }
    }

  private:
    std::ofstream file_;
};

class Timer
{
  public:
    Timer(const std::string &name, BenchmarkLogger &logger)
        : name_(name), logger_(logger), start_(std::chrono::high_resolution_clock::now())
    {
    }
    ~Timer()
    {
        const auto end      = std::chrono::high_resolution_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();
        logger_.log(std::format("[{}] {:.3f} ms\n", name_, duration / 1000.0));
    }

  private:
    std::string name_;
    BenchmarkLogger &logger_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

void run_benchmark(const int iterations, const bool use_auto_save, BenchmarkLogger &logger)
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

    logger.log(std::format("\n=== Running Benchmark (N={}, Strategy={}) ===\n", iterations,
                           (use_auto_save ? "Auto" : "Manual")));

    // 1. Write Performance
    {
        Timer t("Write (Set)", logger);
        for (int i = 0; i < iterations; ++i)
        {
            store.set("key_" + std::to_string(i), i);
        }
        if (!use_auto_save)
        {
            // For Manual mode, set() is memory only.
        }
    }

    if (!use_auto_save)
    {
        Timer t("Manual Save (Disk IO)", logger);
        store.save();
    }

    // 2. Read Performance (Memory)
    {
        Timer t("Read (Get)", logger);
        volatile int sum = 0; // Prevent optimization
        for (int i = 0; i < iterations; ++i)
        {
            sum += store.get<int>("key_" + std::to_string(i));
        }
    }

    // 3. Mixed Read/Write
    {
        Timer t("Mixed Read/Write", logger);
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
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Generate filename with timestamp
    const auto now                 = std::chrono::system_clock::now();
    const auto timestamp           = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const std::string log_filename = std::format("benchmark_results/benchmark_{}.txt", timestamp);

    BenchmarkLogger logger(log_filename);
    logger.log(std::format("Benchmark started at timestamp: {}\n", timestamp));

    const std::vector<int> counts = {1000, 10000};

    for (const int count : counts)
    {
        run_benchmark(count, false, logger); // Manual Save (Fast, Memory dominant)
        run_benchmark(count, true, logger);  // Auto Save (Slow, IO dominant)
    }

    logger.log("\nBenchmark finished.\n");
    std::cout << std::format("Results saved to: {}\n", std::filesystem::absolute(log_filename).string());

    return 0;
}

#include <benchmark/benchmark.h>
#include <config/config.hpp>
#include <filesystem>
#include <string>

// BM_Get: read a pre-set int key (Manual save = pure memory)
static void BM_Get(benchmark::State &state)
{
    config::ConfigStore store("bm_get.json", config::Path::Relative, config::SaveStrategy::Manual);
    store.set("key", 42);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(store.get<int>("key"));
    }
    std::filesystem::remove("bm_get.json");
}
BENCHMARK(BM_Get);

// BM_Set: write an int key (Manual save = pure memory)
static void BM_Set(benchmark::State &state)
{
    config::ConfigStore store("bm_set.json", config::Path::Relative, config::SaveStrategy::Manual);
    int i = 0;
    for (auto _ : state)
    {
        store.set("key", i++);
    }
    std::filesystem::remove("bm_set.json");
}
BENCHMARK(BM_Set);

// BM_SetAutoSave: write triggers disk flush (Auto save)
static void BM_SetAutoSave(benchmark::State &state)
{
    config::ConfigStore store("bm_autosave.json", config::Path::Relative, config::SaveStrategy::Auto);
    int i = 0;
    for (auto _ : state)
    {
        store.set("key", i++);
    }
    std::filesystem::remove("bm_autosave.json");
}
BENCHMARK(BM_SetAutoSave);

// BM_Contains: check key existence
static void BM_Contains(benchmark::State &state)
{
    config::ConfigStore store("bm_contains.json", config::Path::Relative, config::SaveStrategy::Manual);
    store.set("key", 42);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(store.contains("key"));
    }
    std::filesystem::remove("bm_contains.json");
}
BENCHMARK(BM_Contains);

// BM_MixedReadWrite: 90% reads, 10% writes (Manual save)
static void BM_MixedReadWrite(benchmark::State &state)
{
    config::ConfigStore store("bm_mixed.json", config::Path::Relative, config::SaveStrategy::Manual);
    store.set("key", 0);
    int i = 0;
    for (auto _ : state)
    {
        if (i % 10 == 0)
            store.set("key", i);
        else
            benchmark::DoNotOptimize(store.get<int>("key"));
        ++i;
    }
    std::filesystem::remove("bm_mixed.json");
}
BENCHMARK(BM_MixedReadWrite);

BENCHMARK_MAIN();

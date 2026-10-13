#include <benchmark/benchmark.h>
#include <config/config.hpp>

static void BM_Get(benchmark::State &state)
{
    config::Store store("benchmark-get.json", {.save = config::SaveMode::manual});
    store.set("/key", 42);
    for (auto _ : state)
        benchmark::DoNotOptimize(store.get<int>("/key"));
}
BENCHMARK(BM_Get);

static void BM_Set(benchmark::State &state)
{
    config::Store store("benchmark-set.json", {.save = config::SaveMode::manual});
    int value = 0;
    for (auto _ : state)
        store.set("/key", value++);
}
BENCHMARK(BM_Set);
BENCHMARK_MAIN();

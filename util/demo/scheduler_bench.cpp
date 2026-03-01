#include <algorithm>
#include <cmath>
#include <mutex>
#include <numeric>
#include <vector>

#include <fmt/format.h>

#include <sihd/util/Clocks.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Scheduler.hpp>
#include <sihd/util/Stopwatch.hpp>
#include <sihd/util/Task.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;

// --- Statistics helpers ---

struct Stats
{
        double min_us;
        double max_us;
        double mean_us;
        double median_us;
        double stddev_us;
        double p95_us;
        double p99_us;
};

Stats compute_stats(std::vector<int64_t> & samples_ns)
{
    Stats s {};
    if (samples_ns.empty())
        return s;

    std::sort(samples_ns.begin(), samples_ns.end());
    size_t n = samples_ns.size();

    auto to_us = [](int64_t ns) {
        return (double)ns / 1000.0;
    };
    auto percentile = [&](double p) -> double {
        double idx = p / 100.0 * (double)(n - 1);
        size_t lo = (size_t)idx;
        size_t hi = lo + 1;
        if (hi >= n)
            return to_us(samples_ns[n - 1]);
        double frac = idx - (double)lo;
        return to_us(samples_ns[lo]) * (1.0 - frac) + to_us(samples_ns[hi]) * frac;
    };

    double sum = 0;
    for (auto v : samples_ns)
        sum += (double)v;
    double mean = sum / (double)n;

    double var = 0;
    for (auto v : samples_ns)
    {
        double d = (double)v - mean;
        var += d * d;
    }
    var /= (double)n;

    s.min_us = to_us(samples_ns.front());
    s.max_us = to_us(samples_ns.back());
    s.mean_us = mean / 1000.0;
    s.median_us = percentile(50);
    s.stddev_us = std::sqrt(var) / 1000.0;
    s.p95_us = percentile(95);
    s.p99_us = percentile(99);
    return s;
}

std::string format_us(double us)
{
    if (std::abs(us) >= 1000.0)
        return fmt::format("{:.2f} ms", us / 1000.0);
    return fmt::format("{:.2f} us", us);
}

std::string format_interval(time_t ns)
{
    if (ns >= time::milli(1))
        return fmt::format("{} ms", time::to_milli(ns));
    if (ns >= time::micro(1))
        return fmt::format("{} us", time::to_micro(ns));
    return fmt::format("{} ns", ns);
}

// --- Phase 1: Latency & Jitter ---

struct LatencyResult
{
        size_t tasks;
        time_t interval_ns;
        size_t expected_ticks;
        size_t actual_ticks;
        size_t overruns;
        double accuracy_pct;
        Stats latency;
        Stats jitter;
};

LatencyResult bench_latency(size_t task_count, time_t interval_ns, time_t run_duration_ns)
{
    Scheduler scheduler("bench_latency");
    SteadyClock clock;
    scheduler.set_clock(&clock);
    scheduler.overrun_at = interval_ns / 2;
    scheduler.set_no_delay(false);
    scheduler.set_start_synchronised(true);

    size_t reserve_per_task = (size_t)(run_duration_ns / interval_ns) + 64;

    // per-task sample collection to avoid lock contention
    struct TaskData
    {
            std::vector<int64_t> latencies_ns;
            time_t last_expected = 0;
    };
    std::vector<TaskData> task_data(task_count);
    for (auto & td : task_data)
        td.latencies_ns.reserve(reserve_per_task);

    std::atomic<size_t> total_ticks {0};

    for (size_t i = 0; i < task_count; i++)
    {
        Task *task = new Task();
        TaskData *td = &task_data[i];
        task->set_method([td, &scheduler, &total_ticks, interval_ns] {
            time_t now = scheduler.now();
            time_t expected = td->last_expected;
            if (expected > 0)
            {
                int64_t latency = (int64_t)(now - expected);
                td->latencies_ns.push_back(latency);
            }
            td->last_expected = now + interval_ns;
            total_ticks.fetch_add(1, std::memory_order_relaxed);
            return true;
        });
        task->run_in = interval_ns;
        task->reschedule_time = interval_ns;
        scheduler.add_task(task);
    }

    scheduler.start();
    std::this_thread::sleep_for(std::chrono::nanoseconds(run_duration_ns));
    scheduler.stop();

    // merge all latency samples
    std::vector<int64_t> all_latencies;
    all_latencies.reserve(reserve_per_task * task_count);
    for (auto & td : task_data)
        all_latencies.insert(all_latencies.end(), td.latencies_ns.begin(), td.latencies_ns.end());

    // compute jitter (absolute difference between consecutive latencies per task)
    std::vector<int64_t> all_jitter;
    all_jitter.reserve(all_latencies.size());
    for (auto & td : task_data)
    {
        for (size_t j = 1; j < td.latencies_ns.size(); j++)
        {
            int64_t diff = std::abs(td.latencies_ns[j] - td.latencies_ns[j - 1]);
            all_jitter.push_back(diff);
        }
    }

    size_t expected_per_task = run_duration_ns / interval_ns;
    size_t expected_total = expected_per_task * task_count;
    size_t actual = total_ticks.load();
    double accuracy = expected_total > 0 ? (double)actual / (double)expected_total * 100.0 : 0.0;

    return {
        .tasks = task_count,
        .interval_ns = interval_ns,
        .expected_ticks = expected_total,
        .actual_ticks = actual,
        .overruns = scheduler.overruns,
        .accuracy_pct = accuracy,
        .latency = compute_stats(all_latencies),
        .jitter = compute_stats(all_jitter),
    };
}

void print_latency_header()
{
    fmt::print("{:<22s} {:>8s} {:>8s} {:>9s} {:>8s}  {:>10s} {:>10s} {:>10s} {:>10s} {:>10s}\n",
               "Config",
               "Expect",
               "Actual",
               "Accuracy",
               "Overrun",
               "Lat.mean",
               "Lat.p50",
               "Lat.p99",
               "Jit.mean",
               "Jit.p99");
    fmt::print("{:-<22s}-{:-<8s}-{:-<8s}-{:-<9s}-{:-<8s}--{:-<10s}-{:-<10s}-{:-<10s}-{:-<10s}-{:-<10s}\n",
               "",
               "",
               "",
               "",
               "",
               "",
               "",
               "",
               "",
               "");
}

void print_latency_row(const char *label, const LatencyResult & r)
{
    fmt::print("{:<22s} {:>8d} {:>8d} {:>8.1f}% {:>8d}  {:>10s} {:>10s} {:>10s} {:>10s} {:>10s}\n",
               label,
               r.expected_ticks,
               r.actual_ticks,
               r.accuracy_pct,
               r.overruns,
               format_us(r.latency.mean_us),
               format_us(r.latency.median_us),
               format_us(r.latency.p99_us),
               format_us(r.jitter.mean_us),
               format_us(r.jitter.p99_us));
}

// --- Phase 2: Throughput ---

struct ThroughputResult
{
        size_t tasks;
        time_t interval_ns;
        time_t total_time_ns;
        size_t total_ticks;
        double throughput_khz;
        double accuracy_pct;
        size_t overruns;
};

ThroughputResult bench_throughput(size_t task_count, time_t interval_ns, time_t run_duration_ns)
{
    Scheduler scheduler("bench_throughput");
    SteadyClock clock;
    scheduler.set_clock(&clock);
    scheduler.overrun_at = interval_ns / 2;
    scheduler.set_no_delay(false);
    scheduler.set_start_synchronised(true);

    std::atomic<size_t> total_ticks {0};

    for (size_t i = 0; i < task_count; i++)
    {
        Task *task = new Task();
        task->set_method([&total_ticks] {
            total_ticks.fetch_add(1, std::memory_order_relaxed);
            return true;
        });
        task->run_in = interval_ns;
        task->reschedule_time = interval_ns;
        scheduler.add_task(task);
    }

    Stopwatch sw;
    scheduler.start();
    std::this_thread::sleep_for(std::chrono::nanoseconds(run_duration_ns));
    scheduler.stop();
    time_t elapsed = sw.time();

    size_t actual = total_ticks.load();
    size_t expected_per_task = run_duration_ns / interval_ns;
    size_t expected_total = expected_per_task * task_count;
    double accuracy = expected_total > 0 ? (double)actual / (double)expected_total * 100.0 : 0.0;
    double throughput = elapsed > 0 ? (double)actual / ((double)elapsed / 1e9) / 1000.0 : 0.0;

    return {
        .tasks = task_count,
        .interval_ns = interval_ns,
        .total_time_ns = elapsed,
        .total_ticks = actual,
        .throughput_khz = throughput,
        .accuracy_pct = accuracy,
        .overruns = scheduler.overruns,
    };
}

// --- Phase 3: Resolution finder ---

struct ResolutionResult
{
        time_t min_stable_interval_ns;
        size_t max_stable_tasks;
        double max_throughput_khz;
};

ResolutionResult find_resolution(time_t run_duration_ns)
{
    // binary search for the minimum interval at which 1 task stays above 95% accuracy
    time_t lo = time::micro(1);
    time_t hi = time::milli(10);
    time_t best_interval = hi;

    fmt::print("  Binary search: 1 task, finding minimum stable interval...\n");

    while (hi - lo > time::micro(1))
    {
        time_t mid = (lo + hi) / 2;
        auto r = bench_throughput(1, mid, run_duration_ns);
        bool stable = r.accuracy_pct >= 95.0;
        fmt::print("    {} -> {:.1f}% accuracy, {:.1f} kHz throughput {}\n",
                   format_interval(mid),
                   r.accuracy_pct,
                   r.throughput_khz,
                   stable ? "OK" : "FAIL");
        if (stable)
        {
            best_interval = mid;
            hi = mid;
        }
        else
        {
            lo = mid;
        }
    }

    // find max tasks at 2x the found resolution
    time_t test_interval = best_interval * 2;
    if (test_interval < time::micro(100))
        test_interval = time::micro(100);

    fmt::print("\n  Scaling tasks at {} interval...\n", format_interval(test_interval));

    size_t best_tasks = 1;
    double best_throughput = 0;
    for (size_t tasks : {1, 2, 5, 10, 20, 50, 100, 200, 500})
    {
        auto r = bench_throughput(tasks, test_interval, run_duration_ns);
        bool stable = r.accuracy_pct >= 95.0;
        fmt::print("    {:>4d} tasks -> {:.1f}% accuracy, {:.1f} kHz throughput {}\n",
                   tasks,
                   r.accuracy_pct,
                   r.throughput_khz,
                   stable ? "OK" : "FAIL");
        if (stable)
        {
            best_tasks = tasks;
            best_throughput = r.throughput_khz;
        }
        else
        {
            break;
        }
    }

    return {
        .min_stable_interval_ns = best_interval,
        .max_stable_tasks = best_tasks,
        .max_throughput_khz = best_throughput,
    };
}

// --- Main ---

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    if (term::is_interactive() && !sihd::util::platform::is_emscripten)
        LoggerManager::console();
    else
        LoggerManager::stream(sihd::util::platform::is_emscripten ? stdout : stderr);

    constexpr time_t phase_duration = time::milli(500);

    // ===================================================================
    fmt::print(
        "╔══════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    fmt::print(
        "║                                   Scheduler Benchmark                                              ║\n");
    fmt::print(
        "╚══════════════════════════════════════════════════════════════════════════════════════════════════════╝\n\n");

    // --- Phase 1: Latency & Jitter profiling ---
    fmt::print(
        "── Phase 1: Latency & Jitter ─────────────────────────────────────────────────────────────────────────\n");
    fmt::print("  Measuring scheduling precision across different frequencies (duration: {} ms per test)\n\n",
               time::to_milli(phase_duration));

    struct TestCase
    {
            size_t tasks;
            time_t interval_ns;
            const char *label;
    };

    std::vector<TestCase> latency_cases = {
        {1, time::milli(10), "1T @ 100 Hz"},
        {10, time::milli(10), "10T @ 100 Hz"},
        {1, time::milli(1), "1T @ 1 kHz"},
        {10, time::milli(1), "10T @ 1 kHz"},
        {50, time::milli(1), "50T @ 1 kHz"},
        {1, time::micro(500), "1T @ 2 kHz"},
        {10, time::micro(500), "10T @ 2 kHz"},
        {1, time::micro(100), "1T @ 10 kHz"},
        {10, time::micro(100), "10T @ 10 kHz"},
        {1, time::micro(50), "1T @ 20 kHz"},
        {1, time::micro(10), "1T @ 100 kHz"},
    };

    print_latency_header();

    std::vector<LatencyResult> latency_results;
    for (const auto & tc : latency_cases)
    {
        auto r = bench_latency(tc.tasks, tc.interval_ns, phase_duration);
        print_latency_row(tc.label, r);
        latency_results.push_back(r);
    }

    // --- Phase 2: Throughput scaling ---
    fmt::print(
        "\n── Phase 2: Throughput scaling ────────────────────────────────────────────────────────────────────────\n");
    fmt::print("  Fixed interval, increasing task count to find scheduler throughput ceiling\n\n");

    constexpr time_t tp_interval = time::micro(500);
    fmt::print("{:<22s} {:>8s} {:>8s} {:>9s} {:>8s}  {:>12s}\n",
               "Config",
               "Expect",
               "Actual",
               "Accuracy",
               "Overrun",
               "Throughput");
    fmt::print("{:-<22s}-{:-<8s}-{:-<8s}-{:-<9s}-{:-<8s}--{:-<12s}\n", "", "", "", "", "", "");

    for (size_t tasks : {1, 5, 10, 25, 50, 100, 200})
    {
        auto r = bench_throughput(tasks, tp_interval, phase_duration);
        fmt::print("{:<22s} {:>8d} {:>8d} {:>8.1f}% {:>8d}  {:>9.1f} kHz\n",
                   fmt::format("{}T @ 2 kHz", tasks),
                   (size_t)(phase_duration / tp_interval) * tasks,
                   r.total_ticks,
                   r.accuracy_pct,
                   r.overruns,
                   r.throughput_khz);
    }

    // --- Phase 3: Automatic resolution detection ---
    fmt::print(
        "\n── Phase 3: Resolution detection ─────────────────────────────────────────────────────────────────────\n");
    fmt::print("  Binary search for minimum stable scheduling interval (>= 95% accuracy)\n\n");

    auto resolution = find_resolution(phase_duration);

    // --- Summary ---
    fmt::print(
        "\n══════════════════════════════════════════════════════════════════════════════════════════════════════\n");
    fmt::print("  Summary\n");
    fmt::print(
        "══════════════════════════════════════════════════════════════════════════════════════════════════════\n\n");

    // find best latency result that's still accurate
    const LatencyResult *best_lat = nullptr;
    for (const auto & r : latency_results)
    {
        if (r.accuracy_pct >= 95.0)
        {
            if (!best_lat || r.interval_ns < best_lat->interval_ns
                || (r.interval_ns == best_lat->interval_ns && r.tasks > best_lat->tasks))
            {
                best_lat = &r;
            }
        }
    }

    fmt::print("  Scheduling resolution : {}\n", format_interval(resolution.min_stable_interval_ns));
    fmt::print("  Max frequency (1 task): {:.0f} Hz\n", 1e9 / (double)resolution.min_stable_interval_ns);
    fmt::print("  Max parallel tasks    : {} (at {} interval)\n",
               resolution.max_stable_tasks,
               format_interval(resolution.min_stable_interval_ns * 2));
    fmt::print("  Peak throughput       : {:.1f} kHz\n", resolution.max_throughput_khz);

    if (best_lat)
    {
        fmt::print("\n  Best stable latency profile ({} tasks @ {}):\n",
                   best_lat->tasks,
                   format_interval(best_lat->interval_ns));
        fmt::print("    Mean latency : {}\n", format_us(best_lat->latency.mean_us));
        fmt::print("    p50  latency : {}\n", format_us(best_lat->latency.median_us));
        fmt::print("    p99  latency : {}\n", format_us(best_lat->latency.p99_us));
        fmt::print("    Max  latency : {}\n", format_us(best_lat->latency.max_us));
        fmt::print("    Mean jitter  : {}\n", format_us(best_lat->jitter.mean_us));
        fmt::print("    p99  jitter  : {}\n", format_us(best_lat->jitter.p99_us));
    }

    double score = resolution.max_throughput_khz;
    fmt::print("\n  Score: {:.1f} kHz effective throughput\n\n", score);

    return 0;
}

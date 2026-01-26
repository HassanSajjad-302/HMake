#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

struct BenchmarkResult {
    std::vector<double> timings;
    double average;
    double min;
    double max;
    double median;
};

void run_command(const std::string& cmd) {
    system(cmd.c_str());
}

double measure_command(const std::string& cmd) {
    auto start = std::chrono::high_resolution_clock::now();
    system(cmd.c_str());
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    return elapsed.count();
}

BenchmarkResult calculate_statistics(const std::vector<double>& timings) {
    BenchmarkResult result;
    result.timings = timings;

    result.average = std::accumulate(timings.begin(), timings.end(), 0.0) / timings.size();
    result.min = *std::min_element(timings.begin(), timings.end());
    result.max = *std::max_element(timings.begin(), timings.end());

    std::vector<double> sorted = timings;
    std::sort(sorted.begin(), sorted.end());
    size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0) {
        result.median = (sorted[mid - 1] + sorted[mid]) / 2.0;
    } else {
        result.median = sorted[mid];
    }

    return result;
}

BenchmarkResult build_ninja_performance(int n) {
    std::vector<double> timings;
    fs::path original_path = fs::current_path();

    fs::current_path("llvm/cmake-build-release");

    for (int i = 0; i < n; i++) {
        std::cout << "Ninja iteration " << (i + 1) << "/" << n << "..." << std::endl;

        run_command("ninja clean");
        double time = measure_command("ninja llvm-min-tblgen");

        timings.push_back(time);
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << time << " ms" << std::endl;
    }

    fs::current_path(original_path);

    return calculate_statistics(timings);
}

BenchmarkResult build_hmake_performance(int n) {
    std::vector<double> timings;
    fs::path original_path = fs::current_path();

    fs::current_path("LLVMBuild");

    for (int i = 0; i < n; i++) {
        std::cout << "HMake iteration " << (i + 1) << "/" << n << "..." << std::endl;

        run_command("rm -r *");
        run_command("hhelper");
        run_command("hhelper");

        fs::current_path("Release/LLVMMinTableGen");

        double time = measure_command("hbuild");
        timings.push_back(time);
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << time << " ms" << std::endl;

        fs::current_path("../..");
    }

    fs::current_path(original_path);

    return calculate_statistics(timings);
}

void print_results(const std::string& name, const BenchmarkResult& result) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << name << " Results:" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Average: " << result.average << " ms" << std::endl;
    std::cout << "Median:  " << result.median << " ms" << std::endl;
    std::cout << "Min:     " << result.min << " ms" << std::endl;
    std::cout << "Max:     " << result.max << " ms" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

int main(int argc, char* argv[]) {
    int n = 5;

    if (argc > 1) {
        n = std::atoi(argv[1]);
    }

    std::cout << "Build System Benchmark (n=" << n << " iterations)" << std::endl;
    std::cout << std::string(60, '=') << std::endl << std::endl;

    std::cout << "Running Ninja benchmarks..." << std::endl;
    BenchmarkResult ninja_result = build_ninja_performance(n);

    std::cout << "\nRunning HMake benchmarks..." << std::endl;
    BenchmarkResult hmake_result = build_hmake_performance(n);

    print_results("Ninja (llvm-min-tblgen)", ninja_result);
    print_results("HMake (hbuild)", hmake_result);

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Comparison:" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    double speedup = hmake_result.average / ninja_result.average;
    if (speedup > 1.0) {
        std::cout << "Ninja is " << speedup << "x faster than HMake" << std::endl;
    } else {
        std::cout << "HMake is " << (1.0/speedup) << "x faster than Ninja" << std::endl;
    }
    std::cout << std::string(60, '=') << std::endl;

    return 0;
}


/*
[3157/3157] Creating executable symlink bin/clang
Command being timed: "ninja clang"
User time (seconds): 14423.44
System time (seconds): 469.44
Percent of CPU this job got: 3130%
Elapsed (wall clock) time (h:mm:ss or m:ss): 7:55.66
Average shared text size (kbytes): 0
Average unshared data size (kbytes): 0
Average stack size (kbytes): 0
Average total size (kbytes): 0
Maximum resident set size (kbytes): 1237472
Average resident set size (kbytes): 0
Major (requiring I/O) page faults: 534
Minor (reclaiming a frame) page faults: 162260363
Voluntary context switches: 298188
Involuntary context switches: 2480340
Swaps: 0
File system inputs: 74976
File system outputs: 1799248
Socket messages sent: 0
Socket messages received: 0
Signals delivered: 0
Page size (bytes): 4096
Exit status: 0

*/
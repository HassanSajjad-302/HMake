#include <filesystem>
#include <format>
#include <iostream>

using std::string, std::filesystem::path, std::chrono::high_resolution_clock, std::filesystem::current_path;

double calculateAverageTime(std::chrono::duration<double, std::milli> totalTime, int iterations)
{
    return totalTime.count() / iterations;
}

auto huBuild(const bool cleanBuild)
{
    const auto start = high_resolution_clock::now();
    if (cleanBuild)
    {
        system("cl /std:c++latest /EHsc /exportHeader ../hu/stl.hpp /Fo");
    }
    system("cl /std:c++latest /EHsc /headerUnit:angle stl.hpp=stl.hpp.ifc ../hu/json.cpp -c");
    const auto end = high_resolution_clock::now();
    return end - start;
}

auto modulesBuild(const bool cleanBuild)
{
    const auto start = high_resolution_clock::now();
    if (cleanBuild)
    {
        system("cl /std:c++latest /EHsc /interface ../modules/std.ixx -c");
        system("cl /std:c++latest /EHsc /interface /reference std=std.ifc ../modules/std.compat.ixx -c");
    }
    system("cl /std:c++latest /EHsc /interface /reference std=std.ifc /reference std.compat=std.compat.ifc "
           "../modules/json.cpp -c");
    const auto end = high_resolution_clock::now();
    return end - start;
}

auto conventionalBuild()
{
    const auto start = high_resolution_clock::now();
    system("cl /std:c++latest /EHsc ../conventional/json.cpp -c");
    const auto end = high_resolution_clock::now();
    return end - start;
}

int main()
{
    constexpr int iterations = 7;

    std::chrono::nanoseconds conventionalCleanBuild(0), moduleCleanBuild(0), huCleanBuild(0), modulesRebuild(0),
        huRebuild(0);

    for (int i = 0; i < iterations; i++)
    {
        conventionalCleanBuild += conventionalBuild();
        moduleCleanBuild += modulesBuild(true);
        huCleanBuild += huBuild(true);
        modulesRebuild += modulesBuild(false);
        huRebuild += huBuild(false);
    }

    // Calculate the average time taken for each function
    const double avgTimeConventionalClean = calculateAverageTime(conventionalCleanBuild, iterations);
    const double avgTimeModuleClean = calculateAverageTime(moduleCleanBuild, iterations);
    const double avgTimeHuClean = calculateAverageTime(huCleanBuild, iterations);
    const double avgTimeModuleRebuild = calculateAverageTime(modulesRebuild, iterations);
    const double avgTimeHuRebuild = calculateAverageTime(huRebuild, iterations);

    // Print the average times
    std::cout << "Average time for conventional: " << avgTimeConventionalClean << " milliseconds" << std::endl;
    std::cout << "Average time for modules clean build: " << avgTimeHuClean << " milliseconds" << std::endl;
    std::cout << "Average time for hu clean build: " << avgTimeModuleClean << " milliseconds" << std::endl;
    std::cout << "Average time for modules rebuild: " << avgTimeHuRebuild << " milliseconds" << std::endl;
    std::cout << "Average time for hu rebuild: " << avgTimeModuleRebuild << " milliseconds" << std::endl;

    return 0;
}
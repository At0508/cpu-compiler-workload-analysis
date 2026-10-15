#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
using Kernel = double (*)(const std::vector<float>&, std::size_t);

double row_major(const std::vector<float>& values, std::size_t n) {
  double sum = 0.0;
  for (std::size_t row = 0; row < n; ++row)
    for (std::size_t col = 0; col < n; ++col)
      sum += values[row * n + col];
  return sum;
}

double column_major(const std::vector<float>& values, std::size_t n) {
  double sum = 0.0;
  for (std::size_t col = 0; col < n; ++col)
    for (std::size_t row = 0; row < n; ++row)
      sum += values[row * n + col];
  return sum;
}

double blocked(const std::vector<float>& values, std::size_t n) {
  constexpr std::size_t tile = 32;
  double sum = 0.0;
  for (std::size_t row0 = 0; row0 < n; row0 += tile)
    for (std::size_t col0 = 0; col0 < n; col0 += tile)
      for (std::size_t row = row0; row < std::min(n, row0 + tile); ++row)
        for (std::size_t col = col0; col < std::min(n, col0 + tile); ++col)
          sum += values[row * n + col];
  return sum;
}

#ifdef _OPENMP
double parallel_rows(const std::vector<float>& values, std::size_t n) {
  double sum = 0.0;
#pragma omp parallel for reduction(+ : sum) schedule(static)
  for (std::ptrdiff_t row = 0; row < static_cast<std::ptrdiff_t>(n); ++row)
    for (std::size_t col = 0; col < n; ++col)
      sum += values[static_cast<std::size_t>(row) * n + col];
  return sum;
}
#endif

struct Result {
  std::string name;
  Kernel kernel;
};

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  const auto middle = values.size() / 2;
  return values.size() % 2 ? values[middle]
                           : (values[middle - 1] + values[middle]) / 2.0;
}

bool run(std::size_t n, int repetitions) {
  std::mt19937 generator(2026);
  std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
  std::vector<float> values(n * n);
  for (auto& value : values) value = distribution(generator);
  const double reference = std::accumulate(values.begin(), values.end(), 0.0);
  const double tolerance = std::max(1.0, std::abs(reference)) * 1e-8;

  std::vector<Result> kernels = {
      {"row_major", row_major},
      {"column_major", column_major},
      {"blocked_32", blocked},
  };
#ifdef _OPENMP
  kernels.push_back({"openmp_rows", parallel_rows});
#endif

  std::cout << "kernel,n,repetitions,median_ms,mad_ms,checksum,check\n";
  for (const auto& item : kernels) {
    const double warmup = item.kernel(values, n);
    if (std::abs(warmup - reference) > tolerance) {
      std::cerr << "correctness check failed for " << item.name << '\n';
      return false;
    }
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repetitions));
    double checksum = warmup;
    for (int i = 0; i < repetitions; ++i) {
      const auto start = Clock::now();
      checksum = item.kernel(values, n);
      const auto stop = Clock::now();
      samples.push_back(std::chrono::duration<double, std::milli>(stop - start).count());
    }
    if (std::abs(checksum - reference) > tolerance) {
      std::cerr << "correctness check failed for " << item.name << '\n';
      return false;
    }
    const double center = median(samples);
    std::vector<double> deviations;
    deviations.reserve(samples.size());
    for (double sample : samples) deviations.push_back(std::abs(sample - center));
    std::cout << item.name << ',' << n << ',' << repetitions << ','
              << std::fixed << std::setprecision(4) << center << ','
              << median(deviations) << ',' << std::setprecision(10)
              << checksum << ",PASS\n";
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  std::size_t n = 512;
  int repetitions = 9;
  if (argc > 1) n = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
  if (argc > 2) repetitions = std::atoi(argv[2]);
  if (n == 0 || repetitions <= 0 || n > 10000) {
    std::cerr << "usage: loop_benchmark [matrix_dimension:1..10000] [repetitions:positive]\n";
    return 2;
  }
#ifdef _OPENMP
  std::cerr << "OpenMP threads=" << omp_get_max_threads() << '\n';
#else
  std::cerr << "OpenMP variant omitted (compiled without OpenMP support)\n";
#endif
  return run(n, repetitions) ? 0 : 1;
}

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

double mean(const std::vector<double> &v)
{
    if (v.empty())
        return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

double median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    const auto n = v.size();
    if (n == 0)
        return 0.0;
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

int main()
{
    int n = 0;
    std::cin >> n;
    std::vector<double> values;
    values.reserve(n);
    for (int i = 0; i < n; ++i) {
        double x;
        std::cin >> x;
        values.push_back(x);
    }
    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "mean = " << mean(values) << ", median = " << median(values) << std::endl;
    if (!values.empty())
        std::cout << "min = " << *minIt << ", max = " << *maxIt << "\n";
    int positive = std::count_if(values.begin(), values.end(), [](double x) { return x > 0; });
    std::cout << "positive: " << positive << std::endl;
    for (const auto &x : values)
        std::cout << std::setw(8) << x;
    std::cout << std::endl;
    return 0;
}

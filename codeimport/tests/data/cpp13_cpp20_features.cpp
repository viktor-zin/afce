#include <compare>
#include <concepts>
#include <iostream>
#include <string>
#include <vector>

template <typename T>
concept Number = std::integral<T> || std::floating_point<T>;

template <Number T>
T sum(const std::vector<T> &values)
{
    T total{};
    for (const T &v : values)
        total += v;
    return total;
}

template <typename T>
    requires std::integral<T>
T gcd(T a, T b)
{
    while (b != 0) {
        T t = a % b;
        a = b;
        b = t;
    }
    return a;
}

struct Version
{
    int major = 0;
    int minor = 0;
    auto operator<=>(const Version &) const = default;
};

consteval int square(int x)
{
    return x * x;
}

auto average(Number auto a, Number auto b)
{
    return (a + b) / 2.0;
}

int main()
{
    std::vector<int> v{1, 2, 3, 4};
    std::cout << sum(v) << ' ' << gcd(84, 36) << ' ' << square(7) << '\n';
    Version a{.major = 1, .minor = 2}, b{.major = 1, .minor = 3};
    if (a < b)
        std::cout << "older" << std::endl;
    for (int i = 0; auto x : v)
        std::cout << i++ << ':' << x << ' ';
    std::cout << '\n' << average(3, 4.5) << std::endl;
    return 0;
}

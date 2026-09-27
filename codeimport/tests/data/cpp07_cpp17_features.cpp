#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

inline constexpr int kLimit = 10;

std::optional<int> parseNumber(std::string_view s)
{
    int value = 0;
    if (s.empty())
        return std::nullopt;
    for (char c : s) {
        if (c < '0' || c > '9')
            return {};
        value = value * 10 + (c - '0');
    }
    return value;
}

template <typename T>
std::string describe(const T &value)
{
    if constexpr (std::is_integral_v<T>)
        return "integer " + std::to_string(value);
    else if constexpr (std::is_floating_point_v<T>)
        return "floating " + std::to_string(value);
    else
        return "something else";
}

std::tuple<int, int, int> divmod(int a, int b)
{
    return {a / b, a % b, b};
}

int main()
{
    std::map<std::string, int> stock{{"apple", 3}, {"pear", 0}};
    if (auto it = stock.find("apple"); it != stock.end() && it->second > 0)
        std::cout << "apples: " << it->second << '\n';
    switch (auto n = parseNumber("42"); n.value_or(-1)) {
    case 42:
        std::cout << "the answer" << std::endl;
        break;
    case -1:
        std::cout << "not a number" << std::endl;
        break;
    default:
        std::cout << "a number" << std::endl;
    }
    auto [q, r, d] = divmod(17, 5);
    std::cout << q << ' ' << r << ' ' << d << '\n';
    for (auto &[name, count] : stock)
        count += kLimit;
    std::cout << describe(5) << ", " << describe(2.5) << ", " << describe("x") << std::endl;
    return 0;
}

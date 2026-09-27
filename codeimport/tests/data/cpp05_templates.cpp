#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

template <typename T>
T maxOf(const T &a, const T &b)
{
    return a < b ? b : a;
}

template <>
std::string maxOf<std::string>(const std::string &a, const std::string &b)
{
    return a.size() < b.size() ? b : a;
}

template <typename T>
class Stack
{
public:
    void push(const T &value) { items_.push_back(value); }
    T pop();
    bool empty() const { return items_.empty(); }
    std::size_t size() const { return items_.size(); }

private:
    std::vector<T> items_;
};

template <typename T>
T Stack<T>::pop()
{
    if (items_.empty())
        throw std::out_of_range("pop from an empty stack");
    T top = items_.back();
    items_.pop_back();
    return top;
}

template <typename T, std::size_t N>
constexpr std::size_t length(const T (&)[N]) noexcept
{
    return N;
}

int main()
{
    std::cout << maxOf(3, 7) << ' ' << maxOf<double>(2.5, 1.5) << ' '
              << maxOf(std::string("pear"), std::string("fig")) << '\n';
    Stack<int> s;
    int data[] = {1, 2, 3, 4};
    for (std::size_t i = 0; i < length(data); ++i)
        s.push(data[i]);
    while (!s.empty())
        std::cout << s.pop() << ' ';
    std::cout << std::endl;
    return 0;
}

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

std::vector<int> sieve(int n)
{
    std::vector<bool> composite(n + 1, false);
    std::vector<int> primes;
    for (int i = 2; i <= n; ++i) {
        if (composite[i])
            continue;
        primes.push_back(i);
        if (static_cast<long long>(i) * i > n)
            continue;
        for (int j = i * i; j <= n; j += i)
            composite[j] = true;
    }
    return primes;
}

bool isPerfect(int n)
{
    if (n < 2)
        return false;
    int sum = 1;
    for (int d = 2; d * d <= n; ++d) {
        if (n % d != 0)
            continue;
        sum += d;
        if (d != n / d)
            sum += n / d;
        if (sum > n)
            return false;
    }
    return sum == n;
}

int main()
{
    int n;
    std::cout << "n = ";
    std::cin >> n;
    const auto primes = sieve(n);
    int column = 0;
    for (int p : primes) {
        std::cout << std::setw(6) << p;
        if (++column == 10) {
            std::cout << '\n';
            column = 0;
        }
    }
    std::cout << "\nFound " << primes.size() << " primes" << std::endl;
    for (int i = 1; i <= 10000; i++) {
        if (!isPerfect(i))
            continue;
        std::cout << i << " is perfect" << std::endl;
    }
    return 0;
}

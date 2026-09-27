#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;

void hanoi(int n, char from, char to, char via)
{
    if (n == 0)
        return;
    hanoi(n - 1, from, via, to);
    cout << "Move disk " << n << " from " << from << " to " << to << endl;
    hanoi(n - 1, via, to, from);
}

long long power(long long base, unsigned exp)
{
    if (exp == 0)
        return 1;
    long long half = power(base, exp / 2);
    if (exp % 2 == 0)
        return half * half;
    else
        return half * half * base;
}

void permutations(std::string &s, std::size_t k, std::vector<std::string> &out)
{
    if (k == s.size()) {
        out.push_back(s);
        return;
    }
    for (std::size_t i = k; i < s.size(); ++i) {
        std::swap(s[k], s[i]);
        permutations(s, k + 1, out);
        std::swap(s[k], s[i]);
    }
}

int binarySearch(const std::vector<int> &v, int lo, int hi, int key)
{
    if (lo > hi)
        return -1;
    int mid = (lo + hi) / 2;
    if (v[mid] == key)
        return mid;
    return v[mid] < key ? binarySearch(v, mid + 1, hi, key) : binarySearch(v, lo, mid - 1, key);
}

int ackermann(int m, int n)
{
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

int main()
{
    hanoi(3, 'A', 'C', 'B');
    cout << "2^10 = " << power(2, 10) << endl;
    std::string s = "abc";
    std::vector<std::string> perms;
    permutations(s, 0, perms);
    for (const auto &p : perms)
        cout << p << ' ';
    cout << endl;
    std::vector<int> v{1, 4, 9, 16, 25};
    cout << binarySearch(v, 0, static_cast<int>(v.size()) - 1, 16) << endl;
    cout << ackermann(2, 3) << endl;
    return 0;
}

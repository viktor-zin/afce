#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

std::string normalize(const std::string &word)
{
    std::string result;
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c)))
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::map<std::string, int> countWords(std::istream &in)
{
    std::map<std::string, int> counts;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string w;
        while (words >> w) {
            w = normalize(w);
            if (!w.empty())
                ++counts[w];
        }
    }
    return counts;
}

int main()
{
    const auto counts = countWords(std::cin);
    int total = 0;
    for (const auto &[word, n] : counts) {
        std::cout << word << ": " << n << '\n';
        total += n;
    }
    if (auto it = counts.find("the"); it != counts.end())
        std::cout << "'the' occurs " << it->second << " times" << std::endl;
    std::cout << "Total: " << total << std::endl;
}

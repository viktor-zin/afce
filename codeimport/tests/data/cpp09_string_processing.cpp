#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

using namespace std;

bool isPalindrome(const string &s)
{
    string clean;
    for (char c : s)
        if (isalnum(static_cast<unsigned char>(c)))
            clean += tolower(static_cast<unsigned char>(c));
    return equal(clean.begin(), clean.begin() + clean.size() / 2, clean.rbegin());
}

int countVowels(const string &s)
{
    int count = 0;
    const string vowels = "aeiouAEIOU";
    for (string::size_type i = 0; i < s.size(); ++i) {
        if (vowels.find(s[i]) != string::npos)
            count++;
    }
    return count;
}

string capitalizeWords(string s)
{
    bool newWord = true;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (isspace(static_cast<unsigned char>(*it))) {
            newWord = true;
        } else if (newWord) {
            *it = static_cast<char>(toupper(static_cast<unsigned char>(*it)));
            newWord = false;
        }
    }
    return s;
}

string caesar(const string &text, int shift)
{
    string out = text;
    for (char &c : out) {
        if (c >= 'a' && c <= 'z')
            c = 'a' + (c - 'a' + shift) % 26;
        else if (c >= 'A' && c <= 'Z')
            c = 'A' + (c - 'A' + shift) % 26;
    }
    return out;
}

int main()
{
    string line;
    while (getline(cin, line)) {
        if (line.empty())
            continue;
        cout << "\"" << line << "\": vowels=" << countVowels(line)
             << (isPalindrome(line) ? ", palindrome" : "") << endl;
        cout << capitalizeWords(line) << endl;
        cout << caesar(line, 3) << endl;
        string upper = line;
        transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        cout << upper << endl;
    }
    return 0;
}

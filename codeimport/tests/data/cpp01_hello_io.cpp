// Простейший ввод-вывод на C++
#include <iostream>
#include <string>

using namespace std;

int main()
{
    string name;
    int age;
    cout << "What is your name? ";
    getline(cin, name);
    cout << "How old are you? ";
    cin >> age;
    if (age < 0) {
        cerr << "Age cannot be negative" << endl;
        return 1;
    }
    cout << "Hello, " << name << "!" << endl;
    cout << "In ten years you will be " << age + 10 << '\n';
    return 0;
}

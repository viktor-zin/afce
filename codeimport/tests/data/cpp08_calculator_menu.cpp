#include <cmath>
#include <iostream>
#include <limits>

using namespace std;

double readNumber(const char *prompt)
{
    double x;
    while (true) {
        cout << prompt;
        if (cin >> x)
            return x;
        cout << "Invalid number, try again" << endl;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

int main()
{
    char op;
    do {
        cout << "Operation (+ - * / ^ q): ";
        cin >> op;
        if (op == 'q')
            break;
        double a = readNumber("a = ");
        double b = readNumber("b = ");
        double result = 0;
        bool ok = true;
        switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*':
        case 'x':
            result = a * b;
            break;
        case '/':
            if (b == 0) {
                cout << "Division by zero!" << endl;
                ok = false;
            } else {
                result = a / b;
            }
            break;
        case '^':
            result = pow(a, b);
            break;
        default:
            cout << "Unknown operation '" << op << "'" << endl;
            ok = false;
        }
        if (ok)
            cout << a << ' ' << op << ' ' << b << " = " << result << endl;
    } while (op != 'q');
    cout << "Bye" << endl;
    return 0;
}

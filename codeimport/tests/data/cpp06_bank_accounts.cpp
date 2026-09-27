#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace bank {

class Account
{
public:
    explicit Account(std::string owner, double balance = 0)
        : owner_(std::move(owner)), balance_(balance)
    {
    }
    virtual ~Account() = default;

    virtual void withdraw(double amount)
    {
        if (amount <= 0)
            throw std::invalid_argument("amount must be positive");
        if (amount > balance_)
            throw std::runtime_error("insufficient funds");
        balance_ -= amount;
    }
    void deposit(double amount) { balance_ += amount; }
    double balance() const { return balance_; }
    const std::string &owner() const { return owner_; }
    virtual std::string kind() const { return "basic"; }

protected:
    std::string owner_;
    double balance_;
};

class CreditAccount : public Account
{
public:
    CreditAccount(std::string owner, double limit) : Account(std::move(owner)), limit_(limit) {}

    void withdraw(double amount) override
    {
        if (balance_ - amount < -limit_)
            throw std::runtime_error("credit limit exceeded");
        balance_ -= amount;
    }
    std::string kind() const override { return "credit"; }

private:
    double limit_;
};

} // namespace bank

int main()
{
    std::vector<std::unique_ptr<bank::Account>> accounts;
    accounts.push_back(std::make_unique<bank::Account>("Ann", 100));
    accounts.push_back(std::make_unique<bank::CreditAccount>("Bob", 50));
    for (auto &acc : accounts) {
        try {
            acc->withdraw(120);
            std::cout << acc->owner() << ": ok" << std::endl;
        } catch (const std::exception &e) {
            std::cout << acc->owner() << ": " << e.what() << std::endl;
        }
        std::cout << acc->kind() << " balance " << acc->balance() << std::endl;
    }
    return 0;
}

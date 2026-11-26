#include "Account.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace std;

string formatCents(Cents amount) {
    bool negative = amount < 0;
    Cents absolute = negative ? -amount : amount;

    Cents dollars = absolute / 100;
    Cents cents = absolute % 100;

    // Build the dollar digits and insert a comma every three digits from the right.
    string digits = to_string(dollars);
    string withCommas;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        withCommas.insert(withCommas.begin(), digits[i]);
        ++count;
        if (count % 3 == 0 && i > 0) {
            withCommas.insert(withCommas.begin(), ',');
        }
    }

    ostringstream out;
    out << (negative ? "-$" : "$") << withCommas << '.'
        << setw(2) << setfill('0') << cents;
    return out.str();
}

Cents toCents(double amount) {
    // llround rounds half away from zero, which matches everyday money rules.
    return static_cast<Cents>(llround(amount * 100.0));
}

Account::Account(int id, const string& owner, AccountType type, Cents openingBalance)
    : id_(id), owner_(owner), type_(type), balance_(openingBalance), frozen_(false) {

    if (openingBalance < 0) {
        throw invalid_argument("Opening balance cannot be negative");
    }
    if (owner.empty()) {
        throw invalid_argument("Account owner name cannot be empty");
    }
}

int Account::id() const {
    return id_;
}

string Account::owner() const {
    return owner_;
}

AccountType Account::type() const {
    return type_;
}

string Account::typeName() const {
    switch (type_) {
        case AccountType::CHECKING: return "Checking";
        case AccountType::SAVINGS:  return "Savings";
    }
    return "Unknown";
}

Cents Account::balance() const {
    lock_guard<mutex> guard(mtx_);
    return balance_;
}

bool Account::isFrozen() const {
    lock_guard<mutex> guard(mtx_);
    return frozen_;
}

bool Account::deposit(Cents amount) {
    lock_guard<mutex> guard(mtx_);

    if (amount <= 0 || frozen_) {
        return false;
    }

    balance_ += amount;
    return true;
}

bool Account::withdraw(Cents amount) {
    lock_guard<mutex> guard(mtx_);

    // Check every rule before touching the balance so a failure leaves no trace.
    if (amount <= 0 || frozen_ || amount > balance_) {
        return false;
    }

    balance_ -= amount;
    return true;
}

void Account::freeze() {
    lock_guard<mutex> guard(mtx_);
    frozen_ = true;
}

void Account::unfreeze() {
    lock_guard<mutex> guard(mtx_);
    frozen_ = false;
}

Cents Account::applyMonthlyInterest(double annualRatePercent) {
    lock_guard<mutex> guard(mtx_);

    // Only healthy, unfrozen savings accounts with a positive balance earn interest.
    if (type_ != AccountType::SAVINGS || frozen_ || balance_ <= 0) {
        return 0;
    }

    // Annual percentage divided by 100 gives the rate, then by 12 for one month.
    double monthlyRate = annualRatePercent / 100.0 / 12.0;
    Cents interest = llround(static_cast<double>(balance_) * monthlyRate);

    balance_ += interest;
    return interest;
}

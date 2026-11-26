#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <mutex>
#include <string>

using namespace std;

// Account is one of the core entities in the banking system. Other modules
// (ATM, FraudEngine, TransactionDatabase) refer to accounts by their integer
// ID and call the public methods below, so the locking rules stay in one place.
// Interest, deposits and withdrawals may all happen concurrently from the
// thread pool without corrupting the balance.

// All money is stored as a whole number of cents. Floating point numbers
// cannot represent most decimal fractions exactly, so 0.1 + 0.2 would drift.
// Using integers keeps every balance exact to the last cent.
using Cents = long long;

// The kinds of account the bank offers. Only savings accounts earn interest.
enum class AccountType {
    CHECKING,
    SAVINGS
};

// Converts an amount in cents into a readable string such as "$1,234.50".
// Negative amounts are shown with a leading minus sign, e.g. "-$4.20".
string formatCents(Cents amount);

// Converts a decimal amount such as 12.34 into cents (1234).
// Rounds to the nearest cent so that 19.99 becomes exactly 1999.
Cents toCents(double amount);

// Account holds one customer's money. It is safe to use from several threads:
// every public method locks an internal mutex for the duration of the call.
//
// Because a mutex cannot be copied, Account objects are not copyable. Store
// them with std::unique_ptr or inside containers of pointers.
class Account {
public:
    // Creates an account. Throws std::invalid_argument for a negative opening balance.
    Account(int id, const string& owner, AccountType type, Cents openingBalance = 0);

    // ---- Read-only accessors -------------------------------------------

    // Unique identifier assigned by the bank.
    int id() const;

    // Name of the person or business that owns the account.
    string owner() const;

    // The account type, e.g. AccountType::SAVINGS.
    AccountType type() const;

    // Human-readable account type, e.g. "Savings".
    string typeName() const;

    // Current balance in cents.
    Cents balance() const;

    // True when the account has been frozen by an administrator or by fraud review.
    bool isFrozen() const;

    // ---- Money movement -------------------------------------------------

    // Adds money. Returns false when the amount is not positive or the account is frozen.
    bool deposit(Cents amount);

    // Removes money. Returns false when the amount is not positive, the account is
    // frozen, or the balance is too small. A failed withdrawal changes nothing.
    bool withdraw(Cents amount);

    // ---- Account state --------------------------------------------------

    // Blocks all deposits and withdrawals until unfrozen.
    void freeze();

    // Re-enables normal activity on the account.
    void unfreeze();

    // Adds one month of interest for savings accounts at the given annual rate.
    // Returns the interest that was credited. Checking accounts earn nothing.
    Cents applyMonthlyInterest(double annualRatePercent);

private:
    int id_;
    string owner_;
    AccountType type_;
    Cents balance_;
    bool frozen_;

    // Mutable so that const getters can still lock the mutex.
    mutable mutex mtx_;
};

// ---------------------------------------------------------------------------
// Usage example:
//   auto acc = make_unique<Account>(1001, "Abebe Kebede", AccountType::SAVINGS,
//                                   toCents(500.00));
//   acc->deposit(toCents(25.50));          // balance is now $525.50
//   bool ok = acc->withdraw(toCents(900)); // false: insufficient funds
//   cout << formatCents(acc->balance());   // prints "$525.50"
// ---------------------------------------------------------------------------
#endif // ACCOUNT_H

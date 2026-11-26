#ifndef ATM_H
#define ATM_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "Account.h"
#include "FraudEngine.h"
#include "TransactionDatabase.h"

using namespace std;

// ATM is one cash machine at one physical location.
//
// It keeps track of the cash inside the machine and of how much each account
// has withdrawn today. It also writes every successful movement to the shared
// transaction log and hands the record to the fraud engine.
class ATM {
public:
    // Outcome of a withdrawal or deposit. Callers should check this value.
    enum class Result {
        OK,
        INSUFFICIENT_FUNDS,
        INSUFFICIENT_CASH,
        ACCOUNT_FROZEN,
        DAILY_LIMIT_EXCEEDED,
        INVALID_AMOUNT
    };

    // Cash is dispensed in $20 notes, so withdrawals must be multiples of this value.
    static constexpr Cents kNoteValue = 2000;

    // location     - printed on receipts and stored with each transaction.
    // cashCapacity - the most cash the machine can hold.
    // startingCash - cash loaded when the machine is installed.
    // db           - shared transaction log.
    // fraud        - shared fraud engine, must outlive the ATM.
    ATM(const string& location, Cents cashCapacity, Cents startingCash,
        shared_ptr<TransactionDatabase> db, FraudEngine& fraud);

    // Dispenses cash from the given account. Thread-safe.
    Result withdraw(Account& account, Cents amount);

    // Accepts a cash deposit into the given account.
    Result deposit(Account& account, Cents amount);

    // Adds cash to the machine. Returns false if it would exceed capacity.
    bool refill(Cents amount);

    // Cash currently inside the machine.
    Cents cashLeft() const;

    // Maximum cash the machine can hold.
    Cents cashCapacity() const;

    // Maximum withdrawal per account per day.
    Cents dailyLimit() const;

    // Changes the daily withdrawal limit for every account.
    void setDailyLimit(Cents limit);

    // Total withdrawn by one account since the last reset.
    Cents withdrawnToday(int accountId) const;

    // Clears the daily totals. Called by the nightly batch job.
    void resetDailyTotals();

    // Location name as given at construction.
    string location() const;

    // Short text for a result code, e.g. "Insufficient funds".
    static string resultName(Result result);

private:
    string location_;
    Cents capacity_;
    Cents cashLeft_;
    Cents dailyLimit_;

    // Maps account ID to the amount that account has withdrawn today.
    map<int, Cents> withdrawnToday_;

    shared_ptr<TransactionDatabase> db_;
    FraudEngine& fraud_;

    // Protects cash, daily totals and the withdrawal path.
    mutable mutex mtx_;
};

// Deployment notes:
//   * One ATM object is created per physical machine at start-up.
//   * All ATMs share the same TransactionDatabase and FraudEngine instances.
//   * The nightly batch calls resetDailyTotals() on every ATM once the day closes.
//   * Cash counts are reconciled against the machine log at each refill.
// Security note: the ATM does not store card or PIN data. Authentication is
// assumed to have happened before withdraw() is called.
// Withdrawal limits are per account, so a stolen card cannot be used at many ATMs.
#endif // ATM_H

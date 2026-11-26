#include "ATM.h"

#include <stdexcept>

#include "Logger.h"

using namespace std;

ATM::ATM(const string& location, Cents cashCapacity, Cents startingCash,
         shared_ptr<TransactionDatabase> db, FraudEngine& fraud)
    : location_(location),
      capacity_(cashCapacity),
      cashLeft_(startingCash),
      dailyLimit_(toCents(1000.00)),
      db_(move(db)),
      fraud_(fraud) {

    if (location.empty()) {
        throw invalid_argument("ATM location cannot be empty");
    }
    if (cashCapacity <= 0) {
        throw invalid_argument("ATM cash capacity must be positive");
    }
    if (startingCash < 0 || startingCash > cashCapacity) {
        throw invalid_argument("Starting cash must be between zero and capacity");
    }
    if (!db_) {
        throw invalid_argument("ATM needs a transaction database");
    }

    Logger::instance().info("ATM", "Installed at " + location_
        + " with " + formatCents(startingCash) + " cash");
}

ATM::Result ATM::withdraw(Account& account, Cents amount) {
    // Validate the request before taking any lock.
    if (amount <= 0 || amount % kNoteValue != 0) {
        return Result::INVALID_AMOUNT;
    }

    lock_guard<mutex> guard(mtx_);

    if (account.isFrozen()) {
        return Result::ACCOUNT_FROZEN;
    }

    if (withdrawnToday_[account.id()] + amount > dailyLimit_) {
        return Result::DAILY_LIMIT_EXCEEDED;
    }

    if (amount > cashLeft_) {
        return Result::INSUFFICIENT_CASH;
    }

    // Account::withdraw does its own checks and returns false if funds are short.
    if (!account.withdraw(amount)) {
        return Result::INSUFFICIENT_FUNDS;
    }

    // The money has left the account, so the machine's books must follow.
    cashLeft_ -= amount;
    withdrawnToday_[account.id()] += amount;

    TransactionRecord rec = db_->record(account.id(), TxType::WITHDRAWAL, amount,
                                        account.balance(), "ATM:" + location_);

    // Fire-and-forget: the result is not needed by the customer, and alerts are
    // collected inside the engine. The lock is released before the task runs.
    fraud_.analyzeAsync(rec);

    return Result::OK;
}

ATM::Result ATM::deposit(Account& account, Cents amount) {
    if (amount <= 0) {
        return Result::INVALID_AMOUNT;
    }

    // Deposits do not touch the machine's cash, so only the account lock is needed.
    if (!account.deposit(amount)) {
        return Result::ACCOUNT_FROZEN;
    }

    TransactionRecord rec = db_->record(account.id(), TxType::DEPOSIT, amount,
                                        account.balance(), "ATM:" + location_);
    fraud_.analyzeAsync(rec);
    return Result::OK;
}

bool ATM::refill(Cents amount) {
    lock_guard<mutex> guard(mtx_);

    if (amount <= 0 || cashLeft_ + amount > capacity_) {
        return false;
    }

    cashLeft_ += amount;
    Logger::instance().info("ATM", location_ + " refilled, now holds "
        + formatCents(cashLeft_));
    return true;
}

Cents ATM::cashLeft() const {
    lock_guard<mutex> guard(mtx_);
    return cashLeft_;
}

Cents ATM::cashCapacity() const {
    return capacity_;
}

Cents ATM::dailyLimit() const {
    lock_guard<mutex> guard(mtx_);
    return dailyLimit_;
}

void ATM::setDailyLimit(Cents limit) {
    if (limit <= 0) {
        throw invalid_argument("Daily limit must be positive");
    }
    lock_guard<mutex> guard(mtx_);
    dailyLimit_ = limit;
}

Cents ATM::withdrawnToday(int accountId) const {
    lock_guard<mutex> guard(mtx_);
    auto it = withdrawnToday_.find(accountId);
    return it == withdrawnToday_.end() ? 0 : it->second;
}

void ATM::resetDailyTotals() {
    lock_guard<mutex> guard(mtx_);
    withdrawnToday_.clear();
    Logger::instance().info("ATM", location_ + " daily totals reset");
}

string ATM::location() const {
    return location_;
}

string ATM::resultName(Result result) {
    switch (result) {
        case Result::OK:                   return "Success";
        case Result::INSUFFICIENT_FUNDS:   return "Insufficient funds";
        case Result::INSUFFICIENT_CASH:    return "Machine is out of cash";
        case Result::ACCOUNT_FROZEN:       return "Account is frozen";
        case Result::DAILY_LIMIT_EXCEEDED: return "Daily limit exceeded";
        case Result::INVALID_AMOUNT:       return "Amount must be a positive multiple of $20";
    }
    return "Unknown";
}

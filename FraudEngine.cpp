#include "FraudEngine.h"

#include <stdexcept>

#include "Logger.h"

using namespace std;

FraudEngine::FraudEngine(ThreadPool& pool, Cents largeAmountThreshold, size_t maxTransactionsPerMinute)
    : pool_(pool),
      largeThreshold_(largeAmountThreshold),
      maxPerMinute_(maxTransactionsPerMinute) {

    if (largeAmountThreshold <= 0) {
        throw invalid_argument("Large-amount threshold must be positive");
    }
    if (maxTransactionsPerMinute == 0) {
        throw invalid_argument("Velocity limit must be at least one transaction");
    }
}

future<bool> FraudEngine::analyzeAsync(const TransactionRecord& record) {
    // The lambda copies the record, so the caller may discard its own copy
    // immediately. The engine is captured by pointer and must outlive the task.
    return pool_.enqueue([this, record]() {
        return analyze(record);
    });
}

bool FraudEngine::analyze(const TransactionRecord& record) {
    lock_guard<mutex> guard(mtx_);

    bool clean = true;

    // Rule 1: a single large movement.
    if (record.amount >= largeThreshold_) {
        raise(record.accountId,
              "Large " + txTypeName(record.type) + " at or above threshold",
              record.amount);
        clean = false;
    }

    // Rule 3: amounts just under the threshold may indicate structuring.
    // Multiplying by ten avoids floating point while comparing 90 percent.
    if (record.amount < largeThreshold_ &&
        record.amount * 10 >= largeThreshold_ * 9) {
        raise(record.accountId,
              "Amount just below large-transaction threshold (possible structuring)",
              record.amount);
        clean = false;
    }

    // Rule 2: velocity. Keep only timestamps from the last 60 seconds.
    auto now = chrono::steady_clock::now();
    auto& window = recent_[record.accountId];
    window.push_back(now);

    while (!window.empty() && now - window.front() > chrono::minutes(1)) {
        window.pop_front();
    }

    if (window.size() > maxPerMinute_) {
        raise(record.accountId,
              "High velocity: " + to_string(window.size())
                  + " transactions in the last minute",
              record.amount);
        clean = false;
    }

    return clean;
}

void FraudEngine::raise(int accountId, const string& reason, Cents amount) {
    FraudAlert alert{accountId, reason, amount, time(nullptr)};
    alerts_.push_back(alert);

    Logger::instance().warning(
        "FraudEngine",
        "Account " + to_string(accountId) + ": " + reason
            + " (" + formatCents(amount) + ")");
}

vector<FraudAlert> FraudEngine::alerts() const {
    lock_guard<mutex> guard(mtx_);
    return alerts_;
}

size_t FraudEngine::alertCount() const {
    lock_guard<mutex> guard(mtx_);
    return alerts_.size();
}

void FraudEngine::setLargeAmountThreshold(Cents threshold) {
    if (threshold <= 0) {
        throw invalid_argument("Large-amount threshold must be positive");
    }
    lock_guard<mutex> guard(mtx_);
    largeThreshold_ = threshold;
}

Cents FraudEngine::largeAmountThreshold() const {
    lock_guard<mutex> guard(mtx_);
    return largeThreshold_;
}

size_t FraudEngine::maxTransactionsPerMinute() const {
    lock_guard<mutex> guard(mtx_);
    return maxPerMinute_;
}

void FraudEngine::clearAlerts() {
    lock_guard<mutex> guard(mtx_);
    alerts_.clear();
}

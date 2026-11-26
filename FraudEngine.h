#ifndef FRAUDENGINE_H
#define FRAUDENGINE_H

#include <chrono>
#include <cstddef>
#include <ctime>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "Account.h"
#include "ThreadPool.h"
#include "TransactionDatabase.h"

using namespace std;

// One suspicious event recorded by the engine.
struct FraudAlert {
    int accountId;      // Account that triggered the alert.
    string reason;      // Human-readable explanation of the rule that fired.
    Cents amount;       // Amount of the transaction that caused the alert.
    time_t when;        // When the alert was raised.
};

// FraudEngine inspects every recorded transaction for suspicious patterns.
//
// Rules currently implemented:
//   1. Large amount    - a single movement at or above the threshold.
//   2. Velocity        - more than maxTransactionsPerMinute movements in 60 seconds.
//   3. Structuring     - an amount in the 90 to 99.99 percent band of the threshold,
//                        a common way to avoid large-transaction reporting.
//
// Analysis can run synchronously with analyze() or on the thread pool with
// analyzeAsync(). Both paths use the same internal lock, so the rules see a
// consistent view of recent activity.
class FraudEngine {
public:
    // The pool must outlive the engine. Throws std::invalid_argument for bad settings.
    FraudEngine(ThreadPool& pool, Cents largeAmountThreshold, size_t maxTransactionsPerMinute);

    // Queues the check on the thread pool. The future yields true when clean.
    future<bool> analyzeAsync(const TransactionRecord& record);

    // Runs the check on the calling thread. Returns true when the record looks clean.
    bool analyze(const TransactionRecord& record);

    // Snapshot of every alert raised so far, oldest first.
    vector<FraudAlert> alerts() const;

    // Number of alerts raised so far.
    size_t alertCount() const;

    // Changes the large-amount threshold. Takes effect on the next analysis.
    void setLargeAmountThreshold(Cents threshold);

    // Current large-amount threshold.
    Cents largeAmountThreshold() const;

    // Current velocity limit.
    size_t maxTransactionsPerMinute() const;

    // Clears the alert list. Velocity history is kept.
    void clearAlerts();

private:
    // Stores an alert and writes a warning to the log. Caller must hold mtx_.
    void raise(int accountId, const string& reason, Cents amount);

    ThreadPool& pool_;
    Cents largeThreshold_;
    size_t maxPerMinute_;

    // For every account, the timestamps of its recent transactions.
    map<int, deque<chrono::steady_clock::time_point>> recent_;
    vector<FraudAlert> alerts_;

    mutable mutex mtx_;
};

//
// Why alerts do not block transactions:
//   The engine only observes. Blocking a payment in real time would make the
//   ATM and teller paths depend on fraud latency. Instead, alerts are queued for
//   a review team, and accounts can be frozen with Account::freeze() once a
//   reviewer confirms the pattern. Tuning the thresholds is done with the
//   setter methods above, so policy can change without a code rebuild.
//
// Thread-safety:
//   Every public method takes mtx_, and the thread pool may run analyze() on
//   several workers at once. The velocity window is therefore never read and
//   written at the same time by two threads.
// Tuning guidance: a large-amount threshold near the bank reporting limit catches
// the most serious movements. Velocity limits should reflect normal customer
// behaviour; five transactions a minute is generous for a single human user.
// Alerts are reviewed by a human before any account action is taken.
// The engine never freezes accounts on its own.
#endif // FRAUDENGINE_H

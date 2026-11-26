#ifndef TRANSACTIONDATABASE_H
#define TRANSACTIONDATABASE_H

#include <cstddef>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include "Account.h"

using namespace std;

// The kinds of movement the bank records against an account.
enum class TxType {
    DEPOSIT,
    WITHDRAWAL,
    TRANSFER_IN,
    TRANSFER_OUT,
    LOAN_PAYMENT,
    INTEREST
};

// Readable name for a transaction type, e.g. "Withdrawal".
string txTypeName(TxType type);

// One immutable row in the transaction log. Records are never edited after
// they are written; corrections are made by adding a new compensating record.
struct TransactionRecord {
    long long id;          // Unique, increasing sequence number.
    int accountId;         // Account the movement applies to.
    TxType type;           // What kind of movement it was.
    Cents amount;          // Always positive; the type says whether money came in or out.
    Cents balanceAfter;    // Account balance immediately after the movement.
    time_t timestamp;      // Seconds since the Unix epoch when it was recorded.
    string channel;        // Where it originated, e.g. "ATM:Bole", "BRANCH", "ONLINE".
};

// TransactionDatabase is an in-memory, thread-safe append-only log.
//
// It is intentionally simple: a vector guarded by one mutex. The public API is
// designed so that the storage could later be replaced by a real database
// without changing the modules that call it.
class TransactionDatabase {
public:
    TransactionDatabase();

    // Appends a new record and returns the completed row, including its new ID.
    TransactionRecord record(int accountId, TxType type, Cents amount,
                             Cents balanceAfter, const string& channel);

    // All records for one account, oldest first.
    vector<TransactionRecord> historyFor(int accountId) const;

    // All records whose timestamp is in the closed interval [from, to].
    vector<TransactionRecord> between(time_t from, time_t to) const;

    // Sum of amounts of one transaction type for one account.
    Cents totalOf(int accountId, TxType type) const;

    // Total number of stored records.
    size_t size() const;

    // Writes the whole log to a CSV file. Returns false if the file cannot be written.
    bool exportCsv(const string& path) const;

private:
    vector<TransactionRecord> rows_;
    long long nextId_;
    mutable mutex mtx_;
};


// ---------------------------------------------------------------------------
// Design notes for TransactionDatabase
//
// 1. Append-only: a record is never updated or deleted. Auditors can trust that
//    the history is the exact sequence of events the bank processed.
//
// 2. Amounts are always positive. The TxType determines the direction of money
//    flow, so a withdrawal of $50 is stored as type WITHDRAWAL, amount 5000.
//
// 3. balanceAfter captures the account balance at the moment of the movement.
//    With concurrent activity, the value reflects the balance that the account
//    held right after this specific operation finished.
//
// 4. Timestamps use time_t, which is seconds since 1970-01-01 UTC. Presentation
//    code converts them to local time when printing.
//
// 5. Channel strings identify where a transaction came from. Keeping them as
//    free text makes it easy to add new terminals without changing this file.
//
// 6. Reporting helpers (historyFor, between, totalOf) return copies. This is a
//    deliberate choice: the caller never holds a reference into the live
//    vector, so no dangling references can appear when the vector grows.
//
// 7. CSV export writes amounts in raw cents so that a spreadsheet can perform
//    exact arithmetic. Presentation formatting is left to the reader.
// ---------------------------------------------------------------------------
#endif // TRANSACTIONDATABASE_H

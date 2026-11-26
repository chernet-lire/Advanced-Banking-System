#include "TransactionDatabase.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <numeric>

using namespace std;

string txTypeName(TxType type) {
    switch (type) {
        case TxType::DEPOSIT:      return "Deposit";
        case TxType::WITHDRAWAL:   return "Withdrawal";
        case TxType::TRANSFER_IN:  return "Transfer in";
        case TxType::TRANSFER_OUT: return "Transfer out";
        case TxType::LOAN_PAYMENT: return "Loan payment";
        case TxType::INTEREST:     return "Interest";
    }
    return "Unknown";
}

TransactionDatabase::TransactionDatabase()
    : nextId_(1) {
}

TransactionRecord TransactionDatabase::record(int accountId, TxType type, Cents amount,
                                              Cents balanceAfter, const string& channel) {
    lock_guard<mutex> guard(mtx_);

    TransactionRecord row;
    row.id = nextId_++;
    row.accountId = accountId;
    row.type = type;
    row.amount = amount;
    row.balanceAfter = balanceAfter;
    row.timestamp = time(nullptr);
    row.channel = channel;

    rows_.push_back(row);
    return row;
}

vector<TransactionRecord> TransactionDatabase::historyFor(int accountId) const {
    lock_guard<mutex> guard(mtx_);

    // Copy only the matching rows. Returning a copy means callers can keep
    // reading the result after the lock has been released.
    vector<TransactionRecord> result;
    copy_if(rows_.begin(), rows_.end(), back_inserter(result),
            [accountId](const TransactionRecord& r) {
                return r.accountId == accountId;
            });
    return result;
}

vector<TransactionRecord> TransactionDatabase::between(time_t from, time_t to) const {
    lock_guard<mutex> guard(mtx_);

    vector<TransactionRecord> result;
    copy_if(rows_.begin(), rows_.end(), back_inserter(result),
            [from, to](const TransactionRecord& r) {
                return r.timestamp >= from && r.timestamp <= to;
            });
    return result;
}

Cents TransactionDatabase::totalOf(int accountId, TxType type) const {
    lock_guard<mutex> guard(mtx_);

    // accumulate walks the matching rows and adds up their amounts.
    return accumulate(rows_.begin(), rows_.end(), static_cast<Cents>(0),
                      [accountId, type](Cents sum, const TransactionRecord& r) {
                          if (r.accountId == accountId && r.type == type) {
                              return sum + r.amount;
                          }
                          return sum;
                      });
}

size_t TransactionDatabase::size() const {
    lock_guard<mutex> guard(mtx_);
    return rows_.size();
}

bool TransactionDatabase::exportCsv(const string& path) const {
    lock_guard<mutex> guard(mtx_);

    ofstream file(path);
    if (!file) {
        return false;
    }

    // Header row so spreadsheet programs label the columns correctly.
    file << "id,account_id,type,amount_cents,balance_after_cents,timestamp,channel\n";

    for (const auto& row : rows_) {
        file << row.id << ','
             << row.accountId << ','
             << txTypeName(row.type) << ','
             << row.amount << ','
             << row.balanceAfter << ','
             << row.timestamp << ','
             << row.channel << '\n';
    }

    return file.good();
}

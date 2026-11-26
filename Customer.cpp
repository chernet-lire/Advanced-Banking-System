#include "Customer.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

using namespace std;

Customer::Customer(int id, const string& fullName, const string& email, int creditScore)
    : id_(id), fullName_(fullName), email_(email), creditScore_(creditScore) {

    if (fullName.empty()) {
        throw invalid_argument("Customer name cannot be empty");
    }

    // A very small sanity check. Real systems use a stricter validator.
    if (email.find('@') == string::npos) {
        throw invalid_argument("E-mail address must contain '@'");
    }

    validateScore(creditScore);
}

void Customer::validateScore(int score) {
    if (score < kMinCreditScore || score > kMaxCreditScore) {
        throw invalid_argument("Credit score must be between "
                               + to_string(kMinCreditScore) + " and "
                               + to_string(kMaxCreditScore));
    }
}

int Customer::id() const {
    return id_;
}

string Customer::fullName() const {
    return fullName_;
}

string Customer::email() const {
    return email_;
}

int Customer::creditScore() const {
    return creditScore_;
}

void Customer::setCreditScore(int score) {
    validateScore(score);
    creditScore_ = score;
}

bool Customer::isEligibleForLoan(int requiredScore) const {
    return creditScore_ >= requiredScore;
}

void Customer::linkAccount(int accountId) {
    // std::find returns end() when the value is not present yet.
    if (find(accountIds_.begin(), accountIds_.end(), accountId) == accountIds_.end()) {
        accountIds_.push_back(accountId);
    }
}

void Customer::linkLoan(int loanId) {
    if (find(loanIds_.begin(), loanIds_.end(), loanId) == loanIds_.end()) {
        loanIds_.push_back(loanId);
    }
}

bool Customer::unlinkAccount(int accountId) {
    auto it = find(accountIds_.begin(), accountIds_.end(), accountId);
    if (it == accountIds_.end()) {
        return false;
    }
    accountIds_.erase(it);
    return true;
}

bool Customer::ownsAccount(int accountId) const {
    return find(accountIds_.begin(), accountIds_.end(), accountId) != accountIds_.end();
}

const vector<int>& Customer::accountIds() const {
    return accountIds_;
}

const vector<int>& Customer::loanIds() const {
    return loanIds_;
}

size_t Customer::accountCount() const {
    return accountIds_.size();
}

string Customer::summary() const {
    ostringstream out;
    out << "Customer #" << id_ << "\n"
        << "  Name:         " << fullName_ << "\n"
        << "  E-mail:       " << email_ << "\n"
        << "  Credit score: " << creditScore_ << "\n"
        << "  Accounts:     " << accountIds_.size() << "\n"
        << "  Loans:        " << loanIds_.size() << "\n";
    return out.str();
}

#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <string>
#include <vector>

using namespace std;

// Thread-safety note for Customer:
//   Customer does NOT lock internally. The bank registry is responsible for
//   serialising access, typically by calling these methods from a single
//   thread or while holding the registry's own mutex. Account and Loan do their
//   own locking because they are touched by many threads at once, whereas a
//   customer's profile is usually edited only at account-opening time.
//
// Credit-score bands used by the bank:
//   300-579  Poor       (no unsecured loans)
//   580-669  Fair      (small loans only)
//   670-739  Good      (standard loans)
//   740-850  Excellent (preferred rates)

// Customer describes a bank client. A customer can own any number of accounts
// and loans. The customer stores only the IDs of those objects; the objects
// themselves are owned by the bank's registries. This keeps the relationship
// loose and avoids keeping two copies of the same data in sync.
class Customer {
public:
    // Credit scores outside this range are rejected by the constructor.
    static constexpr int kMinCreditScore = 300;
    static constexpr int kMaxCreditScore = 850;

    // Creates a customer. Throws std::invalid_argument when the name is empty,
    // the e-mail address does not contain '@', or the credit score is out of range.
    Customer(int id, const string& fullName, const string& email, int creditScore);

    // ---- Identity -------------------------------------------------------

    // Unique customer number assigned by the bank.
    int id() const;

    // Full legal name as entered at account opening.
    string fullName() const;

    // Contact e-mail address used for statements and alerts.
    string email() const;

    // ---- Credit ---------------------------------------------------------

    // Current credit score between kMinCreditScore and kMaxCreditScore.
    int creditScore() const;

    // Updates the score after a credit review. Throws if out of range.
    void setCreditScore(int score);

    // True when the score meets or exceeds the requested minimum.
    bool isEligibleForLoan(int requiredScore) const;

    // ---- Relationships --------------------------------------------------

    // Records that this customer owns the account with the given ID.
    // Linking the same account twice has no additional effect.
    void linkAccount(int accountId);

    // Records a loan taken out by this customer. Duplicates are ignored.
    void linkLoan(int loanId);

    // Removes an account from the customer's list. Returns false if it was not linked.
    bool unlinkAccount(int accountId);

    // True if the account belongs to this customer.
    bool ownsAccount(int accountId) const;

    // IDs of all accounts owned by this customer, in the order they were linked.
    const vector<int>& accountIds() const;

    // IDs of all loans held by this customer.
    const vector<int>& loanIds() const;

    // Number of accounts currently linked.
    size_t accountCount() const;

    // Multi-line text summary for printing reports.
    string summary() const;

private:
    // Shared check used by the constructor and setCreditScore().
    static void validateScore(int score);

    int id_;
    string fullName_;
    string email_;
    int creditScore_;
    vector<int> accountIds_;
    vector<int> loanIds_;
};

// Customers are created by the bank teller system or the online
// registration service. Each new customer is then given an ID that is never reused.
// Deleting a customer is not supported; closing their accounts is used instead.
#endif // CUSTOMER_H

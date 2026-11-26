#ifndef LOAN_H
#define LOAN_H

#include <mutex>
#include <string>

#include "Account.h"

using namespace std;

// Loan models a fixed-rate, fully amortising loan.
//
// The monthly payment is calculated with the standard annuity formula:
//     payment = P * r * (1 + r)^n / ((1 + r)^n - 1)
// where P is the principal, r is the monthly interest rate and n is the
// number of months in the term. Each payment first covers that month's
// interest and the rest reduces the principal.
class Loan {
public:
    // Life-cycle of a loan.
    enum class Status {
        ACTIVE,
        PAID_OFF,
        DEFAULTED
    };

    // Creates a loan. Throws std::invalid_argument for a non-positive principal,
    // a negative rate, or a term of less than one month.
    Loan(int id, int customerId, Cents principal, double annualRatePercent, int termMonths);

    // ---- Accessors ------------------------------------------------------

    // Unique loan number.
    int id() const;

    // The customer who owns the loan.
    int customerId() const;

    // Amount originally borrowed, in cents.
    Cents principal() const;

    // Amount still owed, including any interest that has accrued but not been paid.
    Cents remaining() const;

    // Annual interest rate as a percentage, e.g. 12.5 means 12.5 percent per year.
    double annualRate() const;

    // Total number of scheduled monthly payments.
    int termMonths() const;

    // Number of payments made so far.
    int monthsElapsed() const;

    // Current status of the loan.
    Status status() const;

    // Human-readable status text, e.g. "Active".
    string statusName() const;

    // The fixed amount due every month, in cents.
    Cents monthlyPayment() const;

    // ---- Operations -----------------------------------------------------

    // Applies a payment. Interest for the month is charged first, then the
    // rest reduces the balance. Returns false if the loan is not active or
    // the amount is not positive. When the balance reaches zero the status
    // changes to PAID_OFF automatically.
    bool makePayment(Cents amount);

    // Marks the loan as defaulted. Payments are refused afterwards.
    void markDefaulted();

    // Total interest that would be paid over the whole term at the monthly payment.
    Cents totalInterestOverTerm() const;

private:
    // Standard annuity formula, rounded to the nearest cent.
    static Cents computeMonthlyPayment(Cents principal, double annualRatePercent, int termMonths);

    int id_;
    int customerId_;
    Cents principal_;
    Cents remaining_;
    double annualRate_;
    int termMonths_;
    int monthsElapsed_;
    Cents payment_;
    Status status_;

    // Protects the mutable fields above so payments and reads can run concurrently.
    mutable mutex mtx_;
};

// Loans are repaid in fixed monthly instalments. Early repayment is allowed and
// simply reduces the balance, so the next month interest is calculated on less money.
// The term and rate are fixed at creation and never change afterwards.
// Late payment penalties are handled by the collections module, not here.
// Monthly instalments are rounded to the nearest cent, so the final payment may differ slightly.
#endif // LOAN_H

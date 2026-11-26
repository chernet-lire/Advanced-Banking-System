#include "Loan.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace std;

Loan::Loan(int id, int customerId, Cents principal, double annualRatePercent, int termMonths)
    : id_(id),
      customerId_(customerId),
      principal_(principal),
      remaining_(principal),
      annualRate_(annualRatePercent),
      termMonths_(termMonths),
      monthsElapsed_(0),
      payment_(0),
      status_(Status::ACTIVE) {

    if (principal <= 0) {
        throw invalid_argument("Loan principal must be positive");
    }
    if (annualRatePercent < 0.0) {
        throw invalid_argument("Interest rate cannot be negative");
    }
    if (termMonths < 1) {
        throw invalid_argument("Loan term must be at least one month");
    }

    payment_ = computeMonthlyPayment(principal, annualRatePercent, termMonths);
}

Cents Loan::computeMonthlyPayment(Cents principal, double annualRatePercent, int termMonths) {
    double P = static_cast<double>(principal);
    double r = annualRatePercent / 100.0 / 12.0;
    double payment;

    if (r == 0.0) {
        // Interest-free loans simply divide the principal evenly.
        payment = P / termMonths;
    } else {
        double growth = pow(1.0 + r, termMonths);
        payment = P * r * growth / (growth - 1.0);
    }

    return llround(payment);
}

int Loan::id() const {
    return id_;
}

int Loan::customerId() const {
    return customerId_;
}

Cents Loan::principal() const {
    return principal_;
}

Cents Loan::remaining() const {
    lock_guard<mutex> guard(mtx_);
    return remaining_;
}

double Loan::annualRate() const {
    return annualRate_;
}

int Loan::termMonths() const {
    return termMonths_;
}

int Loan::monthsElapsed() const {
    lock_guard<mutex> guard(mtx_);
    return monthsElapsed_;
}

Loan::Status Loan::status() const {
    lock_guard<mutex> guard(mtx_);
    return status_;
}

string Loan::statusName() const {
    switch (status()) {
        case Status::ACTIVE:    return "Active";
        case Status::PAID_OFF:  return "Paid off";
        case Status::DEFAULTED: return "Defaulted";
    }
    return "Unknown";
}

Cents Loan::monthlyPayment() const {
    return payment_;
}

bool Loan::makePayment(Cents amount) {
    lock_guard<mutex> guard(mtx_);

    if (status_ != Status::ACTIVE || amount <= 0) {
        return false;
    }

    // Interest accrues on the outstanding balance for one month.
    double monthlyRate = annualRate_ / 100.0 / 12.0;
    Cents interest = llround(static_cast<double>(remaining_) * monthlyRate);

    // A payment larger than what is owed is capped so the balance never goes negative.
    Cents owed = remaining_ + interest;
    Cents applied = min(amount, owed);

    remaining_ = owed - applied;
    ++monthsElapsed_;

    if (remaining_ <= 0) {
        remaining_ = 0;
        status_ = Status::PAID_OFF;
    }
    return true;
}

void Loan::markDefaulted() {
    lock_guard<mutex> guard(mtx_);
    if (status_ == Status::ACTIVE) {
        status_ = Status::DEFAULTED;
    }
}

Cents Loan::totalInterestOverTerm() const {
    // Everything paid over the term minus what was originally borrowed.
    return payment_ * termMonths_ - principal_;
}

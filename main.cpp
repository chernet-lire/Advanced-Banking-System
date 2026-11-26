// BankingSystem - demonstration driver.
//
// This program wires every module together:
//   * Logger              - records what happens, to the console and banking.log
//   * ThreadPool          - runs transactions and fraud checks in parallel
//   * Account, Customer   - the people and their money
//   * Loan                - an amortising loan with monthly payments
//   * TransactionDatabase - append-only log of every movement
//   * FraudEngine         - watches the log for suspicious patterns
//   * ATM                 - cash machine front end
//
// Build and run with CMake (see CMakeLists.txt).

#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ATM.h"
#include "Account.h"
#include "Customer.h"
#include "FraudEngine.h"
#include "Loan.h"
#include "Logger.h"
#include "ThreadPool.h"
#include "TransactionDatabase.h"

using namespace std;

namespace {

// Prints a titled separator so the output is easy to scan.
void printSection(const string& title) {
    cout << "\n==== " << title << " ====\n";
}

// Lists every transaction for one account in a simple table.
void printHistory(const TransactionDatabase& db, int accountId) {
    for (const auto& row : db.historyFor(accountId)) {
        cout << "  #" << row.id << "  "
             << txTypeName(row.type) << "  "
             << formatCents(row.amount) << "  balance "
             << formatCents(row.balanceAfter) << "  via "
             << row.channel << "\n";
    }
}

// Moves money between two accounts and records both sides of the movement.
// Returns false if the source account cannot cover the amount.
bool transfer(Account& from, Account& to, Cents amount,
              TransactionDatabase& db, FraudEngine& fraud) {
    if (!from.withdraw(amount)) {
        return false;
    }
    if (!to.deposit(amount)) {
        // Roll back so money is never lost when the destination is frozen.
        from.deposit(amount);
        return false;
    }

    TransactionRecord out = db.record(from.id(), TxType::TRANSFER_OUT, amount,
                                      from.balance(), "ONLINE");
    TransactionRecord in = db.record(to.id(), TxType::TRANSFER_IN, amount,
                                     to.balance(), "ONLINE");

    fraud.analyzeAsync(out);
    fraud.analyzeAsync(in);
    return true;
}

}  // namespace

int main() {
    // ---- Start-up -----------------------------------------------------------
    Logger& log = Logger::instance();
    if (!log.attachFile("banking.log")) {
        log.warning("main", "Could not open banking.log; logging to console only");
    }
    log.info("main", "Banking system starting");

    // The pool is declared first so it is destroyed last. Every task that
    // refers to the fraud engine or the ATM finishes before those objects go away.
    ThreadPool pool(4);
    log.info("main", pool.describe());

    auto db = make_shared<TransactionDatabase>();
    FraudEngine fraud(pool, toCents(5000.00), 5);
    ATM atm("Bole Road Branch", toCents(20000.00), toCents(10000.00), db, fraud);

    // ---- Customers and accounts --------------------------------------------
    printSection("Opening customers and accounts");

    vector<unique_ptr<Customer>> customers;
    customers.push_back(make_unique<Customer>(1, "Abebe Kebede", "abebe@example.com", 720));
    customers.push_back(make_unique<Customer>(2, "Tigist Haile", "tigist@example.com", 655));

    map<int, unique_ptr<Account>> accounts;
    accounts[1001] = make_unique<Account>(1001, "Abebe Kebede", AccountType::CHECKING, toCents(1500.00));
    accounts[1002] = make_unique<Account>(1002, "Abebe Kebede", AccountType::SAVINGS, toCents(8000.00));
    accounts[2001] = make_unique<Account>(2001, "Tigist Haile", AccountType::CHECKING, toCents(400.00));

    customers[0]->linkAccount(1001);
    customers[0]->linkAccount(1002);
    customers[1]->linkAccount(2001);

    for (const auto& customer : customers) {
        cout << customer->summary();
    }

    // ---- ATM activity -------------------------------------------------------
    printSection("ATM withdrawals");

    Account& abebeChecking = *accounts[1001];
    Account& tigistChecking = *accounts[2001];

    ATM::Result r1 = atm.withdraw(abebeChecking, toCents(200.00));
    cout << "Abebe withdraws $200: " << ATM::resultName(r1) << "\n";

    ATM::Result r2 = atm.withdraw(abebeChecking, toCents(250.00));
    cout << "Abebe withdraws $250: " << ATM::resultName(r2) << "\n";

    ATM::Result r3 = atm.withdraw(tigistChecking, toCents(1200.00));
    cout << "Tigist withdraws $1200: " << ATM::resultName(r3) << "\n";

    cout << "Cash left in " << atm.location() << ": "
         << formatCents(atm.cashLeft()) << "\n";

    // ---- Many customers at once: thread pool demonstration ------------------
    printSection("Concurrent deposits through the thread pool");

    vector<future<ATM::Result>> pending;
    for (int i = 0; i < 20; ++i) {
        pending.push_back(pool.enqueue([&atm, &tigistChecking]() {
            return atm.deposit(tigistChecking, toCents(10.00));
        }));
    }

    int successes = 0;
    for (auto& f : pending) {
        if (f.get() == ATM::Result::OK) {
            ++successes;
        }
    }
    cout << successes << " of " << pending.size()
         << " concurrent deposits succeeded\n";
    cout << "Tigist's balance after the burst: "
         << formatCents(tigistChecking.balance()) << "\n";

    // A large deposit that should trigger the fraud engine.
    ATM::Result big = atm.deposit(tigistChecking, toCents(6000.00));
    cout << "Tigist deposits $6000: " << ATM::resultName(big) << "\n";

    // ---- Online transfer ----------------------------------------------------
    printSection("Online transfer");

    Account& abebeSavings = *accounts[1002];
    bool moved = transfer(abebeSavings, abebeChecking, toCents(500.00), *db, fraud);
    cout << "Transfer $500 from savings to checking: "
         << (moved ? "done" : "failed") << "\n";

    // ---- Interest -----------------------------------------------------------
    printSection("Monthly interest");

    Cents interest = abebeSavings.applyMonthlyInterest(4.5);
    db->record(abebeSavings.id(), TxType::INTEREST, interest,
               abebeSavings.balance(), "BATCH");
    cout << "Interest credited to savings: " << formatCents(interest) << "\n";

    // ---- Loans --------------------------------------------------------------
    printSection("Loan");

    Loan loan(9001, 1, toCents(10000.00), 12.0, 24);
    cout << "Monthly payment on $10,000 at 12% for 24 months: "
         << formatCents(loan.monthlyPayment()) << "\n";
    cout << "Total interest over the term: "
         << formatCents(loan.totalInterestOverTerm()) << "\n";

    bool paid = abebeChecking.withdraw(loan.monthlyPayment());
    if (paid && loan.makePayment(loan.monthlyPayment())) {
        db->record(abebeChecking.id(), TxType::LOAN_PAYMENT, loan.monthlyPayment(),
                   abebeChecking.balance(), "ONLINE");
        cout << "First instalment paid. Remaining balance: "
             << formatCents(loan.remaining()) << " (status: "
             << loan.statusName() << ")\n";
    } else {
        cout << "Instalment could not be paid from checking\n";
    }

    // ---- Wait for background fraud checks -----------------------------------
    printSection("Background work");

    pool.waitUntilIdle();
    log.info("main", pool.describe());

    // ---- Reports ------------------------------------------------------------
    printSection("Fraud alerts");

    auto alerts = fraud.alerts();
    if (alerts.empty()) {
        cout << "No alerts raised\n";
    }
    for (const auto& alert : alerts) {
        cout << "  account " << alert.accountId << ": " << alert.reason
             << " (" << formatCents(alert.amount) << ")\n";
    }

    printSection("Transaction history for Abebe (checking)");
    printHistory(*db, abebeChecking.id());

    printSection("Totals");
    cout << "Transactions recorded: " << db->size() << "\n";
    cout << "Total ATM deposits to Tigist: "
         << formatCents(db->totalOf(tigistChecking.id(), TxType::DEPOSIT)) << "\n";
    cout << "Errors logged this run: " << log.errorCount() << "\n";

    if (db->exportCsv("transactions.csv")) {
        log.info("main", "Exported transactions.csv");
    } else {
        log.error("main", "Could not write transactions.csv");
    }

    // ---- Shutdown -----------------------------------------------------------
    log.info("main", "Banking system stopped cleanly");
    pool.shutdown();
    return 0;
}

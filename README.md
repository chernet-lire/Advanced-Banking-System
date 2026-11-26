# Advanced Banking System

A multi-threaded banking management system written in modern C++17. It models customers, accounts, loans, ATM transactions, and fraud detection, and it uses a thread pool so that transactions and fraud checks run in parallel.

## Features

- **Customer management**: profiles, credit scores, and linked accounts and loans.
- **Accounts**: checking and savings accounts with deposits, withdrawals, freezing, and monthly interest for savings.
- **Loans**: fixed-rate, fully amortising loans with monthly payment calculation and early repayment.
- **ATM simulation**: cash-limited withdrawals in $20 notes, daily per-account limits, and deposits.
- **Fraud detection**: runs in the background and flags large amounts, structuring (amounts just under the threshold), and high transaction velocity.
- **Transaction log**: an append-only, thread-safe record of every movement, with history queries and CSV export.
- **Logging**: a thread-safe logger that writes to the console and an optional file.

## Architecture

| Module | Responsibility |
|---|---|
| `Account` | Balance, deposits, withdrawals, freezing, interest |
| `Customer` | Identity, credit score, account and loan links |
| `Loan` | Amortisation, payments, status |
| `ATM` | Cash inventory, daily limits, dispensing |
| `FraudEngine` | Rule-based analysis of transactions |
| `TransactionDatabase` | Append-only transaction log |
| `ThreadPool` | Fixed worker threads with futures |
| `Logger` | Thread-safe singleton logging |

Money is stored as an integer number of cents, so balances stay exact and never suffer from floating-point rounding errors.

## Concurrency

- Each `Account`, `Loan`, `ATM`, and `FraudEngine` protects its state with a `std::mutex`.
- `ThreadPool` runs fraud checks and concurrent deposits in parallel.
- `TransactionDatabase` and `Logger` are safe to call from any thread.

## Requirements

- A C++17 compiler: GCC 9+, Clang 10+, or MSVC 2019+
- CMake 3.16 or newer
- A pthread-capable system (Linux, macOS, or Windows)

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/BankingSystem
```

On Windows with Visual Studio, run the same commands in a Developer Command Prompt, then start `build\Debug\BankingSystem.exe`.

To enable memory and undefined-behaviour checks on GCC or Clang:

```bash
cmake -S . -B build -DBANKING_ENABLE_SANITIZERS=ON
```

## Sample output

The demo in `main.cpp` opens customers and accounts, runs ATM withdrawals, performs twenty concurrent deposits, makes an online transfer, pays interest, makes a loan repayment, and prints the fraud alerts and transaction history. It writes `banking.log` and `transactions.csv` to the working folder.

Because the demo deliberately triggers the velocity rule, the output includes several `[WARNING]` lines. These are expected.

## Project structure

```
Advanced-banking-system/
├── main.cpp
├── CMakeLists.txt
├── Account.h / Account.cpp
├── Customer.h / Customer.cpp
├── Loan.h / Loan.cpp
├── ATM.h / ATM.cpp
├── FraudEngine.h / FraudEngine.cpp
├── ThreadPool.h / ThreadPool.cpp
├── Logger.h / Logger.cpp
└── TransactionDatabase.h / TransactionDatabase.cpp
```

## Limitations and future work

- Data lives in memory and is lost when the program ends. A future version could persist it to a database such as SQLite.
- The fraud engine only raises alerts; it does not block transactions.
- There is no user interface yet. A console menu or a web front end could be added.
- Unit tests are not included yet. Adding them with GoogleTest or Catch2 is a good next step.

## License

This project is released under the MIT License. See the `LICENSE` file for details.

## Author

**Chernet Lire**, programmer in Ethiopia.

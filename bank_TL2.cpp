#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <cstring>

using namespace std;
struct timespec startTime, endTime;

class Account {
public:
    int id;
    double balance;
    mutable std::mutex accountLock;  // Lock for each account
    std::atomic<int> version{0};  // Version for TL2 
    std::atomic<int> flag{0};
    Account(int id, double balance) : id(id), balance(balance) {}

    void deposit(double amount) {
        //std::lock_guard<std::mutex> lock(accountLock);
        balance += amount;
    }

    void withdraw(double amount) {
        //std::lock_guard<std::mutex> lock(accountLock);
        if (balance >= amount) {
            balance -= amount;
        } else {
            cerr << "Insufficient funds in account " << id << endl;
        }
    }

    double getBalance() const {
        //std::lock_guard<std::mutex> lock(accountLock);
        return balance;
    }

    int getVersion() const {
        return version.load(std::memory_order_relaxed);
    }

    void updateBalance(double newBalance) {
        //std::lock_guard<std::mutex> lock(accountLock);
        balance = newBalance;
        version++;
    }

    void lock() {
        int val;
        do {
            val = flag.load();
        } while (val > 0 && !flag.compare_exchange_weak(val, -val));
    }

    void unlock(int write_time) {
        if(write_time > 0){
            return;
        }
        flag.store(write_time);
    }
};

class TL2Transaction {
public:
    TL2Transaction(Account* from, Account* to) : from(from), to(to), startTime(), endTime() {}

    void beginTransaction() {
        startTime = std::chrono::high_resolution_clock::now();
        readSetFromVersion = from->getVersion();
        readSetToVersion = to->getVersion();
    }

    void performTransaction(double amount) {
        double fromBalance = from->getBalance();
        double toBalance = to->getBalance();

        if (fromBalance >= amount) {
            from->updateBalance(fromBalance - amount);
            to->updateBalance(toBalance + amount);
        }
    }

    void endTransaction() {
        endTime = std::chrono::high_resolution_clock::now();
        // Commit the transaction if the read values are unchanged
        if (readSetFromVersion == from->getVersion() && readSetToVersion == to->getVersion()) {
            commitTransaction();
        }
    }

    void commitTransaction() {
        int writeTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        // Lock the write addresses at the end
        from->lock();
        to->lock();
        performTransaction(transferAmount);
        from->unlock(writeTime);
        to->unlock(writeTime);
    }

private:
    Account* from;
    Account* to;
    std::chrono::high_resolution_clock::time_point startTime;
    std::chrono::high_resolution_clock::time_point endTime;
    int readSetFromVersion;
    int readSetToVersion;
    static constexpr double transferAmount = 50.0;
};

class BankingSystem {
private:
    vector<Account*> accounts;

public:
    BankingSystem(int numAccounts, double initialBalance) {
        for (int i = 0; i < numAccounts; ++i) {
            Account *acc_obj = new Account(i, initialBalance);
            accounts.push_back(acc_obj);
        }
    }

    bool transfer(int from, int to, double amount) {
        TL2Transaction transaction(accounts[from], accounts[to]);
        transaction.beginTransaction();
        transaction.endTransaction();
        return true;
    }

    double getBalance(int accountID) const {
        //std::lock_guard<std::mutex> lock(accounts[accountID]->accountLock);
        double balance= accounts[accountID]->getBalance();
    }

    const vector<Account*>& getAccounts() const {
        return accounts;
    }

    int getNumAccounts() const {
        return accounts.size();
    }
};

void testBankingSystem(BankingSystem& bankingSystem, int numThreads, int transactionsPerThread, ostream& output) {
    vector<thread> threads;
    clock_gettime(CLOCK_MONOTONIC,&startTime);
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bankingSystem, transactionsPerThread, i, &output]() {
            try {
                for (int j = 0; j < transactionsPerThread; ++j) {
                    int from = rand() % bankingSystem.getNumAccounts();
                    int to = rand() % bankingSystem.getNumAccounts();
                    double amount = static_cast<double>(rand() % 100) / 10.0;

                    bankingSystem.transfer(from, to, amount);
                }
                //output << "Thread " << i << " completed its transactions." << endl;
            } catch (const exception& e) {
                cerr << "Exception in thread " << i << ": " << e.what() << endl;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    clock_gettime(CLOCK_MONOTONIC,&endTime);
}
void highContentionTest(BankingSystem& bankingSystem, int accountId, double initialBalance) {
    constexpr int NUM_THREADS = 10;
    constexpr int TRANSACTIONS_PER_THREAD = 100;
    atomic<int> abortCount(0);
    atomic<int> successCount(0);

    // Function to perform conflicting transactions
    auto performTransactions = [&](int threadId) {
        for (int i = 0; i < TRANSACTIONS_PER_THREAD; ++i) {
            // Example transaction: attempt to withdraw an amount that might lead to an abort
            double amount = (threadId % 3 + 1) * 10; // Vary amount to increase likelihood of conflict
            bool success = bankingSystem.transfer(accountId, (accountId + 1) % bankingSystem.getNumAccounts(), amount);
            if (!success) {
                abortCount++;
            } else {
                successCount++;
            }
        }
    };

    // Start threads to simulate high contention
    vector<thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(performTransactions, i);
    }

    // Join threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify system state and abort handling
    cout << "Abort count: " << abortCount << endl;
    cout << "Success count: " << successCount << endl;
    double finalBalance = bankingSystem.getBalance(accountId) + bankingSystem.getBalance((accountId + 1) % bankingSystem.getNumAccounts());
    assert(finalBalance == initialBalance * 2); // Adjust according to your system's logic
    cout << "Final combined balance is correct." << endl;
}
void testSimpleContention(BankingSystem& system) {
    int accountA = 0; // Assuming account A's ID is 0
    int accountB = 1; // Assuming account B's ID is 1
    double transferAmount = system.getBalance(accountA) - 10; // Close to A's balance
    atomic<int> abortCount(0);
    atomic<int> successCount(0);

    std::vector<std::thread> threads;
    for (int i = 0; i < 100; ++i) { // Create 10 threads to simulate contention
        threads.emplace_back([&]() {
            bool success = system.transfer(accountA, accountB, transferAmount);
            if (!success) {
                abortCount++;
            } else {
                successCount++;
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) thread.join();
    cout << "Abort count: " << abortCount << endl;
    cout << "Success count: " << successCount << endl;
    // Verify and log the outcome
}
void testCascadingAborts(BankingSystem& system) {
    int accountA = 0;
    int accountB = 1;
    int accountC = 2;
    atomic<int> abortCount(0);
    atomic<int> successCount(0);
    // Assuming each account starts with an adequate balance
    double amountAB = 100; // Transfer amount from A to B
    double amountBC = 100; // Transfer amount from B to C

    std::thread firstTransfer([&]() {
        bool success=system.transfer(accountA, accountB, amountAB);
        if (!success) {
                abortCount++;
            } else {
                successCount++;
            }
    });

    std::thread secondTransfer([&]() {
        // Optionally, add a small delay here to ensure this happens after the first transfer starts
        bool success=system.transfer(accountB, accountC, amountBC);
        if (!success) {
                abortCount++;
            } else {
                successCount++;
            }
    });

    firstTransfer.join();
    secondTransfer.join();
    cout << "Abort count: " << abortCount << endl;
    cout << "Success count: " << successCount << endl;
    // Verify and log the outcome
}
void testHighVolumeStress(BankingSystem& system) {
    int numAccounts = system.getNumAccounts();
    int numThreads = 50; // High number of threads for stress
    int transactionsPerThread = 100;
    atomic<int> abortCount(0);
    atomic<int> successCount(0);

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < transactionsPerThread; ++j) {
                int fromAccount = rand() % numAccounts;
                int toAccount = rand() % numAccounts;
                while (toAccount == fromAccount) toAccount = rand() % numAccounts; // Ensure different accounts
                double amount = (rand() % 100) + 1; // Random amount
                bool success=system.transfer(fromAccount, toAccount, amount);
                if (!success) {
                abortCount++;
            }   else {
                successCount++;
                }
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) thread.join();
    cout << "Abort count: " << abortCount << endl;
    cout << "Success count: " << successCount << endl;
    // Verify and log the outcome
}



int main(int argc, char *argv[]) {
    const int numAccounts = 100;
    const double initialBalance = 1000.0;
    BankingSystem bankingSystem(numAccounts, initialBalance);

    int numThreads = 1; // default values
    int transactionsPerThread = 100;

    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "-opt") == 0) {
                if (i + 2 < argc) {
                    numThreads = std::stoi(argv[i + 1]);
                    transactionsPerThread = std::stoi(argv[i + 2]);
                } else {
                    std::cerr << "Invalid use of -opt. Usage: -opt <numThreads> <transactionsPerThread>" << std::endl;
                    return 1;
                }
                break;
            }
            else if (std::strcmp(argv[i], "-h") == 0) {
                cout<<"Usage:./bank_TL2 -opt <numThreads> <transactionsPerThread>";
                return 0;
            }
        }
    }

    /*testBankingSystem(bankingSystem, numThreads, transactionsPerThread, std::cout);
    unsigned long long elapsed_ns;
	elapsed_ns = (endTime.tv_sec-startTime.tv_sec)*1000000000 + (endTime.tv_nsec-startTime.tv_nsec);
	printf("Elapsed (ns): %llu\n",elapsed_ns);
	double elapsed_s = ((double)elapsed_ns)/1000000000.0;
	printf("Elapsed (s): %lf\n",elapsed_s);
    cout << "Throughput: " << (numThreads * transactionsPerThread) / elapsed_s << " transactions/second" << endl;*/

    //highContentionTest(bankingSystem, 0, initialBalance);
    //testSimpleContention(bankingSystem);
    //testCascadingAborts(bankingSystem);
    testHighVolumeStress(bankingSystem);
    return 0;
}

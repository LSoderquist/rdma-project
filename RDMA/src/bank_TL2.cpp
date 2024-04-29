#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bank_common.h>

using namespace std;
struct timespec startTime, endTime;

class Account {
public:
    int id;
    //double balance;
    mutable std::mutex accountLock;  // Lock for each account
    std::atomic<int> version{0};  // Version for TL2 
    std::atomic<int> flag{0};
    Account(int id) : id(id) {}

    // void deposit(double amount) {
    //     //std::lock_guard<std::mutex> lock(accountLock);
    //     balance += amount;
    // }

    // void withdraw(double amount) {
    //     //std::lock_guard<std::mutex> lock(accountLock);
    //     if (balance >= amount) {
    //         balance -= amount;
    //     } else {
    //         cerr << "Insufficient funds in account " << id << endl;
    //     }
    // }

    double getBalance() const {
        //std::lock_guard<std::mutex> lock(accountLock);
        return stod(balance(id));
    }

    int getVersion() const {
        return version.load(std::memory_order_relaxed);
    }

    void incVersion() {
        version++;
    }

    // void updateBalance(double newBalance) {
    //     //std::lock_guard<std::mutex> lock(accountLock);
    //     balance = newBalance;
    //     version++;
    // }

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
    TL2Transaction(Account* from, Account* to, double amount) : from(from), to(to), startTime(), endTime(), transferAmount(amount) {}

    void beginTransaction() {
        startTime = std::chrono::high_resolution_clock::now();
        readSetFromVersion = from->getVersion();
        readSetToVersion = to->getVersion();
    }

    void performTransaction(double amount) {
        transfer(from->id, to->id, amount);
        from->incVersion();
        to->incVersion();
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
    double transferAmount;
};

class BankingSystem {
private:
    vector<Account*> accounts;

public:
    BankingSystem(int numAccounts, double initialBalance) {
        for (int i = 0; i < numAccounts; ++i) {
            Account *acc_obj = new Account(i);
            accounts.push_back(acc_obj);
        }
    }

    bool transfer(int from, int to, double amount) {
        TL2Transaction transaction(accounts[from], accounts[to], amount);
        transaction.beginTransaction();
        transaction.endTransaction();
        return true;
    }

    double getBalance(int accountID) const {
        //std::lock_guard<std::mutex> lock(accounts[accountID]->accountLock);
        double balance= accounts[accountID]->getBalance();
        return balance;
    }

    const vector<Account*>& getAccounts() const {
        return accounts;
    }

    int getNumAccounts() const {
        return accounts.size();
    }
};

void handleClient(int *clientSocket, BankingSystem bankingSystem) {
    constexpr int BUFSIZE = 1024;
    char buf[BUFSIZE];

    std::cout << "Thread started...\n";
    
    // Main loop
    while (true) {
        // Receive data from client
        int bytesRead = recv(*clientSocket, buf, BUFSIZE - 1, 0);
        std::cout << "data received from client" << std::endl;
        buf[bytesRead] = '\0'; // Null-terminate the received data
        
        if (bytesRead == 0) {
            std::cout << "Client disconnected" << std::endl;
            close(*clientSocket);
            free(clientSocket);
            return;
        } else if (bytesRead == -1) {
            std::cerr << "Error receiving data from client" << std::endl;
            close(*clientSocket);
            free(clientSocket);
            return;
        }

        // Parse client input
        const char delim = ' ';
        std::string action = std::strtok(buf, &delim);
        if (action == "balance") {
            // Get account balance
            try {
                std::string account = std::strtok(nullptr, &delim);
                std::string balance = std::to_string(bankingSystem.getBalance(stoi(account)));

                // Send balance back to client
                bzero(buf, BUFSIZE);
                balance.copy(buf, balance.size());
                send(*clientSocket, buf, balance.size(), 0);
            } catch (...) {
                // Send error back to client
                bzero(buf, BUFSIZE);
                std::string res = "Invalid input";
                res.copy(buf, res.size());
                send(*clientSocket, buf, res.size(), 0);
            }
        } else if (action == "transfer") {
            // Transfer money
            try {
                int account1 = stoi(std::strtok(nullptr, &delim));
                int account2 = stoi(std::strtok(nullptr, &delim));
                double amount = stod(std::strtok(nullptr, &delim));
                bool transfer = bankingSystem.transfer(account1, account2, amount);

                // Check if transfer was successful
                std::string res;
                if (transfer) {
                    res = "Transaction successful";
                } else {
                    res = "Transaction failed";
                }

                // Send balance back to client
                bzero(buf, BUFSIZE);
                res.copy(buf, res.size());
                send(*clientSocket, buf, res.size(), 0);
            } catch (...) {
                // Send error back to client
                bzero(buf, BUFSIZE);
                std::string res = "Invalid input";
                res.copy(buf, res.size());
                send(*clientSocket, buf, res.size(), 0);
            }
        } else {
            // Send error back to client
            bzero(buf, BUFSIZE);
            std::string res = "Invalid input";
            res.copy(buf, res.size());
            send(*clientSocket, buf, res.size(), 0);
        }
    }
    
    // Close the client socket
    close(*clientSocket);
    free(clientSocket);
}


int main(int argc, char *argv[]) {
    const int numAccounts = stoi(argv[1]);
    initdb(numAccounts);
    
    const double initialBalance = 1000.0;
    BankingSystem bankingSystem(numAccounts, initialBalance);

    // Create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    // Set socket option to allow reuse of the port
    int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        std::cerr << "Error setting socket option\n";
        close(serverSocket);
        return 1;
    }

    // Bind the socket to an IP address and port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Accept connections on any interface
    serverAddress.sin_port = htons(8080); // Listen on port 8080
    if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error binding socket\n";
        close(serverSocket);
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 10) == -1) {
        std::cerr << "Error listening on socket\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Server is listening on port 8080...\n";

    while (true) {
        // Accept incoming connections
        sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);
        int clientConnection = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressSize);
        if (clientConnection == -1) {
            std::cerr << "Error accepting connection\n";
            close(serverSocket);
            return 1;
        }

        // Copy socket value to heap variable to be passed into thread
        int *clientSocket = (int *)malloc(sizeof(int));
        memcpy(clientSocket, &clientConnection, sizeof(int));

        // Start a new thread to handle the client connection
        std::thread t(handleClient, clientSocket, bankingSystem);
        t.detach();
    }

    // Close the server socket 
    close(serverSocket);

    return 0;
}

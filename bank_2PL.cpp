#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <random>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

struct timespec startTime, endTime;
using namespace std;

class Account {
public:
    int id;
    double balance;
    mutable std::mutex accountLock;

    Account(int id, double balance) : id(id), balance(balance) {}

    void deposit(double amount) {
        balance += amount;
    }

    void withdraw(double amount) {
        if (balance >= amount) {
            balance -= amount;
        }
    }

    double getBalance() const {
        return balance;
    }
};

class BankingSystem {
private:
    vector<Account*> accounts;

public:
    BankingSystem(int numAccounts, double initialBalance) {
        for (int i = 0; i < numAccounts; ++i) {
            accounts.push_back(new Account(i, initialBalance));
        }
    }

    ~BankingSystem() {
        for (auto acc : accounts) {
            delete acc;
        }
    }

    bool transfer(int from, int to, double amount) {
        // Sort the account IDs to avoid deadlock
        int lowerId = min(from, to);
        int higherId = max(from, to);

        // Growing Phase: Acquire all necessary locks
        accounts[lowerId]->accountLock.lock();
        if (lowerId != higherId) {
            accounts[higherId]->accountLock.lock();
        }

        // Perform the transfer
        accounts[from]->withdraw(amount);
        accounts[to]->deposit(amount);

        // Shrinking Phase: Release all locks
        accounts[lowerId]->accountLock.unlock();
        if (lowerId != higherId) {
            accounts[higherId]->accountLock.unlock();
        }
        return true;
    }

    double getBalance(int accountID) const {
        std::lock_guard<std::mutex> lock(accounts[accountID]->accountLock);
        return accounts[accountID]->getBalance();
    }
    const vector<Account*>& getAccounts() const {
        return accounts;
    }
    int getNumAccounts() const {
        return accounts.size();
    }
};

void handleClient(int *clientSocket, BankingSystem& bankingSystem) {
    constexpr int BUFSIZE = 1024;
    char buf[BUFSIZE];

    std::cout << "Thread started...\n";
    
    // Main loop
    while (true) {
        // Receive data from client
        int bytesRead = recv(*clientSocket, buf, BUFSIZE - 1, 0);
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
    const int numAccounts = stoi(argv[1]) ;
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
        std::thread t(handleClient, clientSocket, std::ref(bankingSystem));
        t.detach();
    }

    // Close the server socket 
    close(serverSocket);

    return 0;
}

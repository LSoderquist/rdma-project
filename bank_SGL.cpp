#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std;
struct timespec startTime, endTime;
class Account {
public:
    int id;
    double balance;

    Account(int id, double balance) : id(id), balance(balance) {}

    void deposit(double amount) {
        balance += amount;
    }

    void withdraw(double amount) {
        if (balance >= amount) {
            balance -= amount;
        } else {
            //cerr << "Insufficient funds in account " << id << endl;
        }
    }

    double getBalance() const {
        return balance;
    }
};

class BankingSystem {
private:
    vector<Account> accounts;
    mutex globalLock;

public:
    BankingSystem(int numAccounts, double initialBalance) {
        accounts.reserve(numAccounts);
        for (int i = 0; i < numAccounts; ++i) {
            accounts.emplace_back(i, initialBalance);
        }
    }

    bool transfer(int from, int to, double amount) {
        lock_guard<mutex> lock(globalLock);

        if (from < 0 || from >= accounts.size() || to < 0 || to >= accounts.size()) {
            cerr << "Invalid account IDs" << endl;
            return false;
        }

        accounts[from].withdraw(amount);
        accounts[to].deposit(amount);
        return true;
    }

    double getBalance(int accountID) const {
        if (accountID < 0 || accountID >= accounts.size()) {
            cerr << "Invalid account ID" << endl;
            return -1.0;
        }

        return accounts[accountID].getBalance();
    }
    const vector<Account>& getAccounts() const {
        return accounts;
    }
    int getNumAccounts() const {
        return accounts.size();
    }
};

void handleClient(int clientSocket, BankingSystem& bankingSystem) {
    constexpr int BUFSIZE = 1024;
    char buf[BUFSIZE];

    std::cout << "Thread started...\n";
    
    // Main loop
    while (true) {
        // Receive data from client
        int bytesRead = recv(clientSocket, buf, BUFSIZE - 1, 0);
        buf[bytesRead] = '\0'; // Null-terminate the received data
        
        if (bytesRead == 0) {
            std::cout << "Client disconnected" << std::endl;
            close(clientSocket);
            return;
        } else if (bytesRead == -1) {
            std::cerr << "Error receiving data from client" << std::endl;
            close(clientSocket);
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
                send(clientSocket, buf, balance.size(), 0);
            } catch (...) {
                // Send error back to client
                bzero(buf, BUFSIZE);
                std::string res = "Invalid input";
                res.copy(buf, res.size());
                send(clientSocket, buf, res.size(), 0);
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
                send(clientSocket, buf, res.size(), 0);
            } catch (...) {
                // Send error back to client
                bzero(buf, BUFSIZE);
                std::string res = "Invalid input";
                res.copy(buf, res.size());
                send(clientSocket, buf, res.size(), 0);
            }
        } else {
            // Send error back to client
            bzero(buf, BUFSIZE);
            std::string res = "Invalid input";
            res.copy(buf, res.size());
            send(clientSocket, buf, res.size(), 0);
        }
    }
    
    // Close the client socket
    close(clientSocket);
}


int main(int argc, char *argv[]) {
    const int numAccounts = 100;
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
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddress, &clientAddressSize);
        if (clientSocket == -1) {
            std::cerr << "Error accepting connection\n";
            close(serverSocket);
            return 1;
        }

        // Start a new thread to handle the client connection
        std::thread t(handleClient, clientSocket, std::ref(bankingSystem));
        t.detach();
    }

    // Close the server socket 
    close(serverSocket);

    return 0;
}

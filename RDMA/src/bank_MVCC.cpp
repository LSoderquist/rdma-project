#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bank_common.h>
#include <rdma_server.cpp>
#include <rdma_common.cpp>

void handleClient() {
    constexpr int BUFSIZE = 1024;
    char buf[BUFSIZE];

    std::cout << "Thread started...\n";
    
    // Main loop
    while (true) {
        // Receive data from client
        char buf[BUFSIZ];
        int ret = send_server_metadata_to_client(buf);
        if (ret) {
            rdma_error("Failed to send server metadata to the client, ret = %d \n", ret);
            return;
        }

        // Parse client input
        const char delim = ' ';
        std::string action = std::strtok(buf, &delim);
        if (action == "balance") {
            // Get account balance
            try {
                std::string account = std::strtok(nullptr, &delim);
                std::string balanceval = balance(std::stoi(account));

                // Send balance back to client
                bzero(buf, BUFSIZE);
                balanceval.copy(buf, balanceval.size());
                ret = send_string_to_client(buf);
                printf("send_string_to_client\n");
                if (ret) {
                    rdma_error("Failed to send string to the client, ret = %d\n", ret);
                    return;
                }
            } catch (...) {
                // Send error back to client
                bzero(buf, BUFSIZE);
                std::string res = "Invalid input";
                res.copy(buf, res.size());
                ret = send_string_to_client(buf);
                printf("send_string_to_client\n");
                if (ret) {
                    rdma_error("Failed to send string to the client, ret = %d\n", ret);
                    return;
                }
            }
        } else if (action == "transfer") {
            // Transfer money
            try {
                int account1 = std::stoi(std::strtok(nullptr, &delim));
                int account2 = std::stoi(std::strtok(nullptr, &delim));
                double amount = std::stod(std::strtok(nullptr, &delim));
                std::string transferval = transfer(account1, account2, amount);

                // Check if transfer was successful
                std::string res;
                if (transferval.length() != 0) {
                    res = "Transaction successful";
                } else {
                    res = "Transaction failed";
                }

                // Send balance back to client
                bzero(buf, BUFSIZE);
                res.copy(buf, res.size());
                ret = send_string_to_client(buf);
                printf("send_string_to_client\n");
                if (ret) {
                    rdma_error("Failed to send string to the client, ret = %d\n", ret);
                    return;
                }
            } catch (...) {
                // Send error back to client
                bzero(buf, BUFSIZE);
                std::string res = "Invalid input";
                res.copy(buf, res.size());
                ret = send_string_to_client(buf);
                printf("send_string_to_client\n");
                if (ret) {
                    rdma_error("Failed to send string to the client, ret = %d\n", ret);
                    return;
                }
            }
        } else {
            // Send error back to client
            bzero(buf, BUFSIZE);
            std::string res = "Invalid input";
            res.copy(buf, res.size());
            ret = send_string_to_client(buf);
                printf("send_string_to_client\n");
                if (ret) {
                    rdma_error("Failed to send string to the client, ret = %d\n", ret);
                    return;
                }
        }
    }
}

int main(int argc, char *argv[]) {
    const int numAccounts = std::stoi(argv[1]);
    initdb(numAccounts);

    int ret, option;
	struct sockaddr_in server_sockaddr;
	bzero(&server_sockaddr, sizeof server_sockaddr);
	server_sockaddr.sin_family = AF_INET; /* standard IP NET address */
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* passed address */
    ret = get_addr(argv[2], (struct sockaddr*) &server_sockaddr);
    if (ret) {
        rdma_error("Invalid IP \n");
        return ret;
    }

    if(!server_sockaddr.sin_port) {
		/* If still zero, that mean no port info provided */
		server_sockaddr.sin_port = htons(DEFAULT_RDMA_PORT); /* use default port */
	}

    ret = start_rdma_server(&server_sockaddr);
	if (ret) {
		rdma_error("RDMA server failed to start cleanly, ret = %d \n", ret);
		return ret;
	}

    while (true) {
        ret = setup_client_resources();
        if (ret) { 
            rdma_error("Failed to setup client resources, ret = %d \n", ret);
            return ret;
        }

        ret = accept_client_connection();
        if (ret) {
            rdma_error("Failed to handle client cleanly, ret = %d \n", ret);
            return ret;
        }

        // Copy socket value to heap variable to be passed into thread
        // int *clientSocket = (int *)malloc(sizeof(int));
        // memcpy(clientSocket, &clientConnection, sizeof(int));

        // Start a new thread to handle the client connection
        std::thread t(handleClient);
        t.detach();
    }

    return 0;
}

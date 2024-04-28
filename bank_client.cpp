#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "128.110.218.39"
#define SERVER_PORT 8080
#define MAXINPUTLEN 32

int main(int argc, char *argv[]) {
    // Create a socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        std::cerr << "Error creating socket\n";
        return 1;
    }

    // Server address and port
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT); // Assuming the server is running on port 8080
    inet_pton(AF_INET, SERVER_IP, &serverAddress.sin_addr); 

    // Connect to the server
    if (connect(clientSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        std::cerr << "Error connecting to server\n";
        close(clientSocket);
        return 1;
    }

    // Print usage
    std::cout << "Usage 1: [balance] [account id]\nUsage 2: [transfer] [from account id] [to account id] [amount]" << std::endl;

    // Main loop
    while (true) {
        
        // Get input from user
        char message[MAXINPUTLEN];
        std::cin.getline(message, MAXINPUTLEN);

        // Check for exit command
        if (!strncmp(message, "exit", MAXINPUTLEN)) {
            close(clientSocket);
            exit(0);
        }

        // Send data to the server
        if (send(clientSocket, message, strlen(message), 0) == -1) {
            std::cerr << "Error sending data to server\n";
            close(clientSocket);
            return 1;
        }

        // Receive response from the server
        char buffer[1024];
        int bytesReceived = recv(clientSocket, buffer, 1024, 0);
        if (bytesReceived == -1) {
            std::cerr << "Error receiving data from server\n";
            close(clientSocket);
            return 1;
        } else if (bytesReceived == 0) {
            std::cout << "Server disconnected\n";
        } else {
            buffer[bytesReceived] = '\0'; // Null-terminate the received data
            std::cout << "Server response: " << buffer << std::endl;
        }
    }

    // Close the socket
    close(clientSocket);

    return 0;
}

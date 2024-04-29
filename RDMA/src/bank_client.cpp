#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <rdma_client.cpp>
#include <rdma_common.cpp>

#define SERVER_PORT 8080
#define MAXINPUTLEN 32

int main(int argc, char *argv[]) {
    // Print usage
    std::cout << "Usage 1: [balance] [account id]\nUsage 2: [transfer] [from account id] [to account id] [amount]" << std::endl;

    // RDMA boilerplate
    struct sockaddr_in server_sockaddr;
	int ret, option;
	bzero(&server_sockaddr, sizeof server_sockaddr);
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ret = get_addr(argv[1], (struct sockaddr*) &server_sockaddr);
    if (ret) {
        rdma_error("Invalid IP \n");
        return ret;
    }
    if (!server_sockaddr.sin_port) {
        /* no port provided, use the default port */
        server_sockaddr.sin_port = htons(DEFAULT_RDMA_PORT);
	}

    ret = client_prepare_connection(&server_sockaddr);
	if (ret) { 
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	 }
	ret = client_pre_post_recv_buffer(); 
	if (ret) { 
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	}
	ret = client_connect_to_server();
	if (ret) { 
		rdma_error("Failed to setup client connection , ret = %d \n", ret);
		return ret;
	}

    // Main loop
    while (true) {
        
        // Get input from user
        char *message = (char *)malloc(MAXINPUTLEN);
        std::cin.getline(message, MAXINPUTLEN);

        // Check for exit command
        if (!strncmp(message, "exit", MAXINPUTLEN)) {
            exit(0);
        }

        // Send data to the server
        ret = client_xchange_metadata_with_server(message);
        if (ret) {
            rdma_error("Failed to setup client connection , ret = %d \n", ret);
            return ret;
        }

        // Receive response from the server
        char res[BUFSIZ];
        ret = receive_string_from_server(res);
        if (ret) {
            rdma_error("Failed to receive string from server, ret = %d\n", ret);
            return ret;
        }

        std::cout << "Server response: " << res << std::endl;

        free(message);
    }

    return 0;
}

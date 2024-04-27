#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <iostream>
#include <infiniband/verbs.h>
#include <limits>

const char* SERVER_IP = "128.110.217.38";  // Updated as per the latest instructions
const int PORT = 12345;
const int BUFFER_SIZE = 1024;

int main() {
    rdma_cm_id* connection = nullptr;
    rdma_event_channel* ec = nullptr;
    sockaddr_in addr;

    ec = rdma_create_event_channel();
    if (!ec) {
        perror("rdma_create_event_channel failed");
        exit(EXIT_FAILURE);
    }

    if (rdma_create_id(ec, &connection, nullptr, RDMA_PS_TCP)) {
        perror("rdma_create_id failed");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (rdma_resolve_addr(connection, nullptr, (struct sockaddr *)&addr, 2000)) {
        perror("rdma_resolve_addr failed");
        exit(EXIT_FAILURE);
    }

    rdma_cm_event* event;
    while (rdma_get_cm_event(ec, &event) == 0) {
        auto event_copy = *event;
        rdma_ack_cm_event(event);

        if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED) {
            rdma_resolve_route(connection, 2000);
        } else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
            rdma_conn_param conn_param = {};
            memset(&conn_param, 0, sizeof(conn_param));
            conn_param.initiator_depth = 1;
            conn_param.responder_resources = 1;
            conn_param.rnr_retry_count = 7;

            if (rdma_connect(connection, &conn_param)) {
                fprintf(stderr, "Failed to connect\n");
                exit(EXIT_FAILURE);
            }
        }
     else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {
            printf("Connected to server\n");

            // Send a message
            const char* msg = "Hello, RDMA World!";
            size_t msg_size = strlen(msg) + 1;
            if (msg_size > std::numeric_limits<uint32_t>::max()) {
                fprintf(stderr, "Message size too large\n");
                exit(EXIT_FAILURE);
            }
            char* send_buf = new char[msg_size];
            memcpy(send_buf, msg, msg_size);

            ibv_mr* mr = ibv_reg_mr(connection->pd, send_buf, msg_size, IBV_ACCESS_LOCAL_WRITE);
            ibv_sge sge = {
                .addr = (uintptr_t)send_buf,
                .length = static_cast<uint32_t>(msg_size),
                .lkey = mr->lkey
            };

            ibv_send_wr send_wr = {}, *bad_send_wr;
            send_wr.wr_id = (uintptr_t)send_buf;
            send_wr.sg_list = &sge;
            send_wr.num_sge = 1;
            send_wr.opcode = IBV_WR_SEND;
            send_wr.send_flags = IBV_SEND_SIGNALED;

            if (ibv_post_send(connection->qp, &send_wr, &bad_send_wr)) {
                fprintf(stderr, "Failed to post send WR\n");
                exit(EXIT_FAILURE);
            }
            printf("Message sent to server\n");

        } 
        if (event->event == RDMA_CM_EVENT_REJECTED) {
    // The status should be an error code, but it's being misinterpreted or mislogged
    printf("Connection rejected by server. Reason: %s\n", rdma_event_str(event->event));
    if (event->status) {
        // Assuming status is an integer error code, which typically doesn't need conversion for RDMA_CM_EVENT_REJECTED
        printf("Error code: %d\n", event->status);
        // Optionally provide more detailed interpretation if specific error codes are known
    }
} else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED) {
            rdma_disconnect(connection);
            break;
        }
        else {
            printf("Connection rejected by server. Status: %s\n", rdma_event_str(event_copy.event));
            }
    }

    rdma_destroy_id(connection);
    rdma_destroy_event_channel(ec);

    return 0;
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int PORT = 12345;
const int BUFFER_SIZE = 1024;

int main() {
    rdma_cm_id *listener = nullptr;
    rdma_cm_event *event = nullptr;
    rdma_event_channel *ec = nullptr;
    sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    ec = rdma_create_event_channel();
    if (!ec) {
        perror("rdma_create_event_channel failed");
        exit(EXIT_FAILURE);
    }

    if (rdma_create_id(ec, &listener, nullptr, RDMA_PS_TCP)) {
        perror("rdma_create_id failed");
        exit(EXIT_FAILURE);
    }

    if (rdma_bind_addr(listener, (struct sockaddr *)&addr)) {
        perror("rdma_bind_addr failed");
        exit(EXIT_FAILURE);
    }

    if (rdma_listen(listener, 1)) {
        perror("rdma_listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is listening on port " << PORT << "...\n";

    ibv_pd *pd = nullptr;
    ibv_comp_channel *comp_chan = nullptr;
    ibv_cq *cq = nullptr;
    ibv_qp *qp = nullptr;
    ibv_mr *mr = nullptr;
    char *recv_buf = nullptr;

    while (rdma_get_cm_event(ec, &event) == 0) {
        auto event_copy = *event;
        rdma_ack_cm_event(event);

        if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST) {
            pd = ibv_alloc_pd(event_copy.id->verbs);
            comp_chan = ibv_create_comp_channel(event_copy.id->verbs);
            cq = ibv_create_cq(event_copy.id->verbs, 10, nullptr, comp_chan, 0);
            ibv_qp_init_attr qp_attr = {};
            qp_attr.send_cq = cq;
            qp_attr.recv_cq = cq;
            qp_attr.qp_type = IBV_QPT_RC;
            qp_attr.cap.max_send_wr = 10;
            qp_attr.cap.max_recv_wr = 10;
            qp_attr.cap.max_send_sge = 1;
            qp_attr.cap.max_recv_sge = 1;

            qp = ibv_create_qp(pd, &qp_attr);
            if (!qp) {
                fprintf(stderr, "Failed to create QP\n");
                continue;
            }

            recv_buf = new char[BUFFER_SIZE];
            mr = ibv_reg_mr(pd, recv_buf, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

            ibv_sge sge = {};
            sge.addr = (uintptr_t)recv_buf;
            sge.length = BUFFER_SIZE;
            sge.lkey = mr->lkey;

            ibv_recv_wr recv_wr = {};
            recv_wr.sg_list = &sge;
            recv_wr.num_sge = 1;

            ibv_recv_wr *bad_wr;
            if (ibv_post_recv(qp, &recv_wr, &bad_wr)) {
                fprintf(stderr, "Failed to post RR\n");
                continue;
            }

            rdma_conn_param conn_param = {};
            memset(&conn_param, 0, sizeof(conn_param));
            conn_param.initiator_depth = 1;
            conn_param.responder_resources = 1;
            conn_param.rnr_retry_count = 7;

            if (rdma_accept(event_copy.id, &conn_param)) {
                fprintf(stderr, "Failed to accept connection\n");
                continue;
            }

            std::cout << "Connection accepted\n";
        } else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED) {
            std::cout << "Connection established. Waiting for messages...\n";

            ibv_wc wc;
            int num_comp;
            while (true) {
                num_comp = ibv_poll_cq(cq, 1, &wc);
                if (num_comp < 0) {
                    fprintf(stderr, "Poll CQ failed\n");
                    continue;
                } else if (num_comp > 0) {
                    if (wc.status == IBV_WC_SUCCESS && wc.opcode == IBV_WC_RECV) {
                        printf("Received message: %s\n", (char *)wc.wr_id);
                        break;  // Assuming one message, exit after receipt
                    } else {
                        fprintf(stderr, "Received failed or wrong opcode: %d\n", wc.status);
                    }
                }
            }
        } else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED) {
            std::cout << "Client disconnected.\n";
            rdma_disconnect(event_copy.id);
            if (qp) {
                ibv_destroy_qp(qp);
            }
            if (cq) {
                ibv_destroy_cq(cq);
            }
            if (comp_chan) {
                ibv_destroy_comp_channel(comp_chan);
            }
            if (mr) {
                ibv_dereg_mr(mr);
            }
            if (recv_buf) {
                delete[] recv_buf;
            }
            if (pd) {
                ibv_dealloc_pd(pd);
            }
            rdma_destroy_id(event_copy.id);
            break;
        }
        else {
    printf("Unhandled event: %d\n", event_copy.event);
}
    }

    rdma_destroy_id(listener);
    rdma_destroy_event_channel(ec);

    return 0;
}

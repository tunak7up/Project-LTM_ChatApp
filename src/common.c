#include "common.h"

void send_message(int socket_fd, Message *msg) {
    if (send(socket_fd, msg, sizeof(Message), 0) < 0) {
        perror("Send failed");
    }
}

int receive_message(int socket_fd, Message *msg) {
    int bytes_received = recv(socket_fd, msg, sizeof(Message), 0);
    if (bytes_received <= 0) {
        return bytes_received; // 0 for disconnect, -1 for error
    }
    return bytes_received;
}

void print_message(Message *msg) {
    printf("Type: %d, Sender: %s, Recipient: %s, Payload: %s\n", 
           msg->type, msg->sender, msg->recipient, msg->payload);
}

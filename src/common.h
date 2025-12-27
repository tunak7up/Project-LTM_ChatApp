#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define MAX_USERNAME 32
#define MAX_PASSWORD 32
#define MAX_GROUP_NAME 32
#define MAX_FRIENDS 50
#define MAX_GROUPS 20
#define MAX_MEMBERS 50

// Message Types
typedef enum {
    MSG_UNKNOWN,
    MSG_REGISTER,
    MSG_LOGIN,
    MSG_LOGOUT,
    MSG_FRIEND_REQUEST,
    MSG_FRIEND_ACCEPT,
    MSG_FRIEND_REJECT,
    MSG_REMOVE_FRIEND,
    MSG_GET_FRIEND_LIST,
    MSG_PRIVATE_CHAT,
    MSG_CREATE_GROUP,
    MSG_INVITE_GROUP,
    MSG_JOIN_GROUP,
    MSG_LEAVE_GROUP,
    MSG_KICK_GROUP,
    MSG_GROUP_CHAT,
    MSG_SUCCESS,
    MSG_ERROR,
    MSG_NOTIFY
} MessageType;

// Standard Message Structure
typedef struct {
    MessageType type;
    char sender[MAX_USERNAME];
    char recipient[MAX_USERNAME]; // Can be username or group ID (as string)
    char payload[BUFFER_SIZE];
} Message;

// Function Prototypes
void send_message(int socket_fd, Message *msg);
int receive_message(int socket_fd, Message *msg);
void print_message(Message *msg);

#endif

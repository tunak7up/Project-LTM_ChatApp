#include "common.h"

int sockfd;
char my_username[MAX_USERNAME];
int logged_in = 0;

void print_help() {
    printf("\n=== COMMANDS ===\n");
    printf("register <user> <pass>\n");
    printf("login <user> <pass>\n");
    printf("logout\n");
    printf("list_friends\n");
    printf("add_friend <username>\n");
    printf("accept_friend <username>\n");
    printf("reject_friend <username>\n");
    printf("chat <username> <message>\n");
    printf("create_group <groupname>\n");
    printf("invite_group <groupname> <username>\n");
    printf("join_group <groupname>\n");
    printf("group_chat <groupname> <message>\n");
    printf("leave_group <groupname>\n");
    printf("kick_group <groupname> <username>\n");
    printf("unfriend <username>\n");
    printf("help\n");
    printf("================\n");
}

void process_input(char *buffer) {
    char cmd[32], arg1[32], arg2[256];
    int n = sscanf(buffer, "%s %s %[^\n]", cmd, arg1, arg2);
    
    Message msg;
    memset(&msg, 0, sizeof(msg));
    
    if (strcmp(cmd, "register") == 0) {
        if (n < 3) { printf("Usage: register <user> <pass>\n"); return; }
        msg.type = MSG_REGISTER;
        strcpy(msg.sender, arg1); // Send user in sender field for register
        strcpy(msg.payload, arg2); // Password
        send_message(sockfd, &msg);
        
    } else if (strcmp(cmd, "login") == 0) {
        if (n < 3) { printf("Usage: login <user> <pass>\n"); return; }
        msg.type = MSG_LOGIN;
        strcpy(msg.sender, arg1);
        strcpy(msg.payload, arg2); 
        // Store conceptually until success
        strcpy(my_username, arg1);
        send_message(sockfd, &msg);
        
    } else if (strcmp(cmd, "logout") == 0) {
        msg.type = MSG_LOGOUT;
        strcpy(msg.sender, my_username);
        send_message(sockfd, &msg);
        logged_in = 0;
        
    } else if (strcmp(cmd, "help") == 0) {
        print_help();
        
    } else {
        // Must be logged in for others
        if (!logged_in) {
            printf("Please login first.\n");
            return;
        }
        strcpy(msg.sender, my_username);

        if (strcmp(cmd, "list_friends") == 0) {
            msg.type = MSG_GET_FRIEND_LIST;
            send_message(sockfd, &msg);
            
        } else if (strcmp(cmd, "add_friend") == 0) {
            if(n < 2) { printf("Usage: add_friend <user>\n"); return; }
            msg.type = MSG_FRIEND_REQUEST;
            strcpy(msg.recipient, arg1);
            send_message(sockfd, &msg);
            
        } else if (strcmp(cmd, "accept_friend") == 0) {
            if(n < 2) { printf("Usage: accept_friend <user>\n"); return; }
            msg.type = MSG_FRIEND_ACCEPT;
            strcpy(msg.recipient, arg1);
            send_message(sockfd, &msg);

        } else if (strcmp(cmd, "reject_friend") == 0) {
            if (n < 2) { printf("Usage: reject_friend <user>\n"); return; }
            msg.type = MSG_FRIEND_REJECT;
            strcpy(msg.recipient, arg1);
            send_message(sockfd, &msg);

        } else if (strcmp(cmd, "unfriend") == 0) {
            if (n < 2) { printf("Usage: unfriend <user>\n"); return; }
            msg.type = MSG_REMOVE_FRIEND;
            strcpy(msg.recipient, arg1);
            send_message(sockfd, &msg);

        } else if (strcmp(cmd, "chat") == 0) {
            if(n < 3) { printf("Usage: chat <user> <msg>\n"); return; }
            msg.type = MSG_PRIVATE_CHAT;
            strcpy(msg.recipient, arg1);
            strcpy(msg.payload, arg2);
            send_message(sockfd, &msg);
            
        } else if (strcmp(cmd, "create_group") == 0) {
            if(n < 2) { printf("Usage: create_group <name>\n"); return; }
            msg.type = MSG_CREATE_GROUP;
            strcpy(msg.payload, arg1);
            send_message(sockfd, &msg);
            
        } else if (strcmp(cmd, "invite_group") == 0) {
             if(n < 3) { printf("Usage: invite_group <group> <user>\n"); return; }
             msg.type = MSG_INVITE_GROUP;
             strcpy(msg.payload, arg1); // Group Name
             strcpy(msg.recipient, arg2); // Target User
             send_message(sockfd, &msg);

        } else if (strcmp(cmd, "join_group") == 0) {
             if(n < 2) { printf("Usage: join_group <name>\n"); return; }
             msg.type = MSG_JOIN_GROUP;
             strcpy(msg.payload, arg1);
             send_message(sockfd, &msg);
        
        } else if (strcmp(cmd, "group_chat") == 0) {
             if(n < 3) { printf("Usage: group_chat <group> <msg>\n"); return; }
             msg.type = MSG_GROUP_CHAT;
             strcpy(msg.recipient, arg1); // Group name
             strcpy(msg.payload, arg2);
             send_message(sockfd, &msg);
        
        } else if (strcmp(cmd, "leave_group") == 0) {
             if(n < 2) { printf("Usage: leave_group <name>\n"); return; }
             msg.type = MSG_LEAVE_GROUP;
             strcpy(msg.payload, arg1); // Group name
             send_message(sockfd, &msg);

        } else if (strcmp(cmd, "kick_group") == 0) {
             if(n < 3) { printf("Usage: kick_group <group> <user>\n"); return; }
             msg.type = MSG_KICK_GROUP;
             strcpy(msg.payload, arg1); // Group name
             strcpy(msg.recipient, arg2); // Target User
             send_message(sockfd, &msg);

        } else {
            printf("Unknown command.\n");
        }
    }
}

void process_server_message(Message *msg) {
    if (msg->type == MSG_SUCCESS) {
        printf("[SUCCESS] %s\n", msg->payload);
        if (strcmp(msg->payload, "Login successful") == 0) {
            logged_in = 1;
        }
    } else if (msg->type == MSG_ERROR) {
        printf("[ERROR] %s\n", msg->payload);
    } else if (msg->type == MSG_NOTIFY) {
        printf("\n[NOTIFY] From %s: %s\n> ", msg->sender, msg->payload);
        fflush(stdout);
    } else if (msg->type == MSG_PRIVATE_CHAT) {
        printf("\n[CHAT] %s: %s\n> ", msg->sender, msg->payload);
        fflush(stdout);
    } else if (msg->type == MSG_GROUP_CHAT) {
        printf("\n[GROUP %s] %s: %s\n> ", msg->recipient, msg->sender, msg->payload);
        fflush(stdout);
    } else if (msg->type == MSG_GET_FRIEND_LIST) {
        // Payload: name:status:online;...
        printf("\n--- Friend List ---\n");
        char *token = strtok(msg->payload, ";");
        while(token) {
            int stat, on;
            char name[32];
            sscanf(token, "%[^:]:%d:%d", name, &stat, &on);
            printf("- %s [%s] [%s]\n", name, 
                   (stat==2 ? "Friend" : "Pending"), 
                   (on ? "Online" : "Offline"));
            token = strtok(NULL, ";");
        }
        printf("-------------------\n");
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;
    fd_set read_fds;
    char buffer[BUFFER_SIZE];

    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        // Default to localhost if not provided for convenience? 
        // Strict CLI usually requires it, but let's default to 127.0.0.1
        // exit(1);
    }
    const char* ip = (argc > 1) ? argv[1] : "127.0.0.1";

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    printf("Connected to server at %s:%d\n", ip, PORT);
    print_help();

    while (1) {
        printf("> ");
        fflush(stdout);
        
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(sockfd, &read_fds);

        if (select(sockfd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                // Remove newline
                buffer[strcspn(buffer, "\n")] = 0;
                process_input(buffer);
            } else {
                // EOF on stdin (e.g. script finished or Ctrl+D)
                break;
            }
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            Message msg;
            int n = receive_message(sockfd, &msg);
            if (n <= 0) {
                printf("Server disconnected.\n");
                break;
            }
            // Clear current prompt line to make output clean
            printf("\r"); 
            process_server_message(&msg);
        }
    }

    close(sockfd);
    return 0;
}

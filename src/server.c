#include "common.h"
#include "db.h"
#include <errno.h>

void handle_client_message(int fd, Message *msg);
void remove_client(int fd);

int main() {
    int listener_fd, new_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len;
    fd_set master_set, read_set;
    int max_fd;

    load_data();
    printf("Data loaded.\n");

    if ((listener_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket");
        exit(1);
    }

    int yes = 1;
    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("Setsockopt");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    if (bind(listener_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind");
        exit(1);
    }

    if (listen(listener_fd, 10) == -1) {
        perror("Listen");
        exit(1);
    }

    printf("Server running on port %d...\n", PORT);

    FD_ZERO(&master_set);
    FD_SET(listener_fd, &master_set);
    max_fd = listener_fd;

    while (1) {
        read_set = master_set;
        if (select(max_fd + 1, &read_set, NULL, NULL, NULL) == -1) {
            perror("Select");
            exit(1);
        }

        for (int i = 0; i <= max_fd; i++) {
            if (FD_ISSET(i, &read_set)) {
                if (i == listener_fd) {
                    addr_len = sizeof client_addr;
                    if ((new_fd = accept(listener_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
                        perror("Accept");
                    } else {
                        FD_SET(new_fd, &master_set);
                        if (new_fd > max_fd) max_fd = new_fd;
                        printf("New connection from %s on socket %d\n", inet_ntoa(client_addr.sin_addr), new_fd);
                    }
                } else {
                    Message msg;
                    int nbytes = receive_message(i, &msg);
                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            printf("Socket %d hung up\n", i);
                        } else {
                            perror("Recv");
                        }
                        remove_client(i);
                        close(i);
                        FD_CLR(i, &master_set);
                    } else {
                        handle_client_message(i, &msg);
                    }
                }
            }
        }
    }
    return 0;
}

void remove_client(int fd) {
    User *u = find_user_by_fd(fd);
    if (u) {
        u->socket_fd = -1;
        u->is_online = 0;
        printf("User %s logged out (disconnect)\n", u->username);
        // Notify friends?
    }
}

void handle_client_message(int fd, Message *msg) {
    Message response;
    memset(&response, 0, sizeof(response));

    if (msg->type == MSG_REGISTER) {
        // Payload: "password" (Sender has username)
        if (register_user(msg->sender, msg->payload)) {
            response.type = MSG_SUCCESS;
            strcpy(response.payload, "Registration successful");
        } else {
            response.type = MSG_ERROR;
            strcpy(response.payload, "Username already exists");
        }
        send_message(fd, &response);

    } else if (msg->type == MSG_LOGIN) {
        if (check_login(msg->sender, msg->payload)) {
            User *u = find_user(msg->sender);
            if(u->is_online) {
                response.type = MSG_ERROR;
                strcpy(response.payload, "Already logged in");
            } else {
                u->is_online = 1;
                u->socket_fd = fd;
                response.type = MSG_SUCCESS;
                strcpy(response.payload, "Login successful");
                printf("User %s logged in on fd %d\n", u->username, fd);
            }
        } else {
            response.type = MSG_ERROR;
            strcpy(response.payload, "Invalid credentials");
        }
        send_message(fd, &response);

    } else if (msg->type == MSG_LOGOUT) {
        remove_client(fd);
        response.type = MSG_SUCCESS;
        strcpy(response.payload, "Logged out");
        send_message(fd, &response);

    } else if (msg->type == MSG_FRIEND_REQUEST) {
        // Sender wants to friend Recipient
        User *target = find_user(msg->recipient);
        if (!target) {
            response.type = MSG_ERROR;
            strcpy(response.payload, "User not found");
            send_message(fd, &response);
        } else {
            User *me = find_user(msg->sender);
            add_friend_request(me, target);
            
            response.type = MSG_SUCCESS;
            strcpy(response.payload, "Request sent");
            send_message(fd, &response);
            
            // Notify target if online
            if (target->is_online) {
                Message notify;
                notify.type = MSG_NOTIFY;
                strcpy(notify.sender, msg->sender);
                sprintf(notify.payload, "Friend request from %s", msg->sender);
                send_message(target->socket_fd, &notify);
            }
        }

    } else if (msg->type == MSG_GET_FRIEND_LIST) {
        User *u = find_user(msg->sender);
        if(!u) return;

        response.type = MSG_SUCCESS;
        // Build payload manually: "name:status,name:status..."
        strcpy(response.payload, "");
        for(int i=0; i<u->friend_count; i++) {
            char buf[64];
            // Get online status of friend
            User *f = find_user(u->friends[i].username);
            int online = f ? f->is_online : 0;
            sprintf(buf, "%s:%d:%d;", u->friends[i].username, u->friends[i].status, online);
            strcat(response.payload, buf);
        }
        send_message(fd, &response);

    } else if (msg->type == MSG_FRIEND_ACCEPT) {
         // msg->sender accepts msg->recipient
         // Update DB
         // Simple update: find friend entry and change status to ACCEPTED
         // Bidirectional
         User *me = find_user(msg->sender);
         User *other = find_user(msg->recipient);
         if(me && other) {
             for(int i=0; i<me->friend_count; i++) {
                 if(strcmp(me->friends[i].username, other->username)==0) {
                     me->friends[i].status = FRIEND_ACCEPTED;
                 }
             }
             for(int i=0; i<other->friend_count; i++) {
                 if(strcmp(other->friends[i].username, me->username)==0) {
                     other->friends[i].status = FRIEND_ACCEPTED;
                 }
             }
             save_data();
             response.type = MSG_SUCCESS;
             strcpy(response.payload, "Friend accepted");
             send_message(fd, &response);
             
             if(other->is_online) {
                 Message notify;
                 notify.type = MSG_NOTIFY;
                 strcpy(notify.sender, me->username);
                 sprintf(notify.payload, "%s accepted your friend request", me->username);
                 send_message(other->socket_fd, &notify);
             }
         }
    } else if (msg->type == MSG_PRIVATE_CHAT) {
        User *target = find_user(msg->recipient);
        if(!target) {
            response.type = MSG_ERROR;
            strcpy(response.payload, "User not found");
            send_message(fd, &response);
        } else {
            // Check friend status? Requirement says "Send msg between 2 users".
            // Typically request friends first, but assignment might not strictly enforce.
            // Let's enforce friendship for cleaner logic, or allow all?
            // "Gửi nhận tin nhắn giữa 2 người dùng"
            // Let's allow if they exist.
            
            if(target->is_online) {
                Message forward = *msg; // Copy message
                send_message(target->socket_fd, &forward);
                response.type = MSG_SUCCESS; // Ack to sender
                send_message(fd, &response);
            } else {
                // Offline msg
                // Implement offline storage or simple notify
                // Requirement: "Gửi tin nhắn offline: 1 điểm"
                // Append to a file `offline_msgs.txt`?
                FILE *fp = fopen("data/offline_msgs.txt", "a");
                if(fp) {
                    fprintf(fp, "%s %s %s\n", msg->recipient, msg->sender, msg->payload);
                    fclose(fp);
                }
                
                response.type = MSG_SUCCESS;
                strcpy(response.payload, "User offline, message saved.");
                send_message(fd, &response);
            }
        }
    } else if (msg->type == MSG_CREATE_GROUP) {
        if(create_group(msg->payload, msg->sender)) { // Payload is group name
            response.type = MSG_SUCCESS;
            strcpy(response.payload, "Group created");
        } else {
            response.type = MSG_ERROR;
            strcpy(response.payload, "Group creation failed");
        }
        send_message(fd, &response);

    } else if (msg->type == MSG_INVITE_GROUP) {
        // sender invites recipient to payload (groupName)
        char groupName[MAX_GROUP_NAME];
        strcpy(groupName, msg->payload);
        
        Group *g = find_group(groupName);
        if(!g) {
             response.type = MSG_ERROR; strcpy(response.payload, "Group not found");
             send_message(fd, &response);
             return;
        }
        
        User *target = find_user(msg->recipient);
        if(!target) {
             response.type = MSG_ERROR; strcpy(response.payload, "User not found");
             send_message(fd, &response);
             return;
        }

        // Send notification to target to Join? Or auto-add?
        // Req: "User gửi yêu cầu mời... tham gia"
        // Target needs to Accept/Join.
        // Send NOTIFY to target with "INVITE groupname"
        if(target->is_online) {
            Message notify;
            notify.type = MSG_NOTIFY;
            strcpy(notify.sender, msg->sender);
            sprintf(notify.payload, "INVITE %s", groupName); // Client logic to parse
            send_message(target->socket_fd, &notify);
            response.type = MSG_SUCCESS; strcpy(response.payload, "Invitation sent");
        } else {
             // Offline invite?
             response.type = MSG_ERROR; strcpy(response.payload, "User is offline");
        }
        send_message(fd, &response);

    } else if (msg->type == MSG_JOIN_GROUP) {
        // payload is groupname
        if(join_group(msg->payload, msg->sender)) {
            response.type = MSG_SUCCESS; strcpy(response.payload, "Joined group");
        } else {
            response.type = MSG_ERROR; strcpy(response.payload, "Failed to join");
        }
        send_message(fd, &response);
    
    } else if (msg->type == MSG_GROUP_CHAT) {
        // recipient is groupName
        Group *g = find_group(msg->recipient);
        if(!g) {
            response.type = MSG_ERROR; strcpy(response.payload, "Group not found");
            send_message(fd, &response);
        } else {
            // Broadcast to all members (except sender)
            for(int i=0; i<g->member_count; i++) {
                User *member = find_user(g->members[i]);
                if(member && member->is_online && strcmp(member->username, msg->sender) != 0) {
                     send_message(member->socket_fd, msg);
                }
            }
            response.type = MSG_SUCCESS; // Ack
            send_message(fd, &response);
        }
    }
}

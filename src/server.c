#include "common.h"
#include "db.h"
#include <errno.h>
#include <time.h>
#include <stdarg.h>

void log_activity(const char *format, ...) {
    va_list args;
    time_t now;
    struct tm *local;
    char time_str[64];

    time(&now);
    local = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local);

    // Print to console
    printf("[%s] ", time_str);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Append to file
    FILE *fp = fopen("data/log.txt", "a");
    if (fp) {
        fprintf(fp, "[%s] ", time_str);
        va_start(args, format);
        vfprintf(fp, format, args);
        va_end(args);
        fclose(fp);
    }
}

void handle_client_message(int fd, Message *msg);
void remove_client(int fd);

int main() {
    //listener File descriptor của socket server để socket bind listen accept và 
    //new_fd File descriptor của socket client để socket accept
    //file descriptor là số nguyên đại diện cho socket
    //đây là 2 loại socket, listener chỉ có 1, còn new_fd có thể có nhiều dựa theo số user đang login
    int listener_fd, new_fd;
    //struct sockaddr_in gồm port(8080), address(ip 127.0.0.1), family(AF_INET)
    struct sockaddr_in server_addr, client_addr;
    //socklen_t là kích thước của struct sockaddr_in
    socklen_t addr_len;
    //fd_set là tập hợp các file descriptor, master_set chứa tất cả các file descriptor, read_set chứa các file descriptor có dữ liệu
    fd_set master_set, read_set;
    //max_fd là file descriptor lớn nhất trong master_set, để select biết phải duyệt đến đâu
    //select là hàm để chọn các file descriptor có dữ liệu
    int max_fd;

    load_data();
    load_data();
    log_activity("Data loaded.\n");

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

    log_activity("Server running on port %d...\n", PORT);

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
                        log_activity("New connection from %s on socket %d\n", inet_ntoa(client_addr.sin_addr), new_fd);
                    }
                } else {
                    Message msg;
                    int nbytes = receive_message(i, &msg);
                    if (nbytes <= 0) {
                        if (nbytes == 0) {
                            log_activity("Socket %d hung up\n", i);
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

// Helper to notify friends of status change
void notify_friends(User *u, int online) {
    for (int i = 0; i < u->friend_count; i++) {
        // Only notify if accepted friend
        if (u->friends[i].status == FRIEND_ACCEPTED) {
            User *f = find_user(u->friends[i].username);
            if (f && f->is_online) {
                Message msg;
                memset(&msg, 0, sizeof(msg));
                msg.type = MSG_NOTIFY;
                strcpy(msg.sender, "SYSTEM");
                sprintf(msg.payload, "Friend %s is now %s", u->username, online ? "Online" : "Offline");
                send_message(f->socket_fd, &msg);
            }
        }
    }
}

void remove_client(int fd) {
    User *u = find_user_by_fd(fd);
    if (u) {
        u->socket_fd = -1;
        u->is_online = 0;
        log_activity("User %s logged out (disconnect)\n", u->username);
        notify_friends(u, 0); // Notify friends of disconnection
    }
}

// Helper to check and deliver offline messages
void check_offline_messages(User *u) {
    FILE *fp = fopen("data/offline_msgs.txt", "r");
    if (!fp) return; // No offline messages

    FILE *tmp = fopen("data/offline_msgs.tmp", "w");
    if (!tmp) {
        fclose(fp);
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        char recipient[MAX_USERNAME], sender[MAX_USERNAME], payload[BUFFER_SIZE];
        // Parse line: recipient sender payload
        // Note: sscanf with %[^\n] handles spaces in payload
        if (sscanf(line, "%s %s %[^\n]", recipient, sender, payload) == 3) {
            if (strcmp(recipient, u->username) == 0) {
                // Message for me
                Message msg;
                memset(&msg, 0, sizeof(msg));
                msg.type = MSG_PRIVATE_CHAT;
                strcpy(msg.sender, sender);
                strcpy(msg.recipient, recipient);
                strcpy(msg.payload, payload);
                send_message(u->socket_fd, &msg);
                found = 1;
            } else {
                // Not for me, keep it
                fprintf(tmp, "%s", line);
            }
        } else {
             // Malformed line, keep it just in case or drop? Keep to be safe.
             fprintf(tmp, "%s", line);
        }
    }

    fclose(fp);
    fclose(tmp);

    if (found) {
        remove("data/offline_msgs.txt");
        rename("data/offline_msgs.tmp", "data/offline_msgs.txt");
    } else {
        remove("data/offline_msgs.tmp"); // No changes
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
                send_message(fd, &response);
            } else {
                u->is_online = 1;
                u->socket_fd = fd;
                response.type = MSG_SUCCESS;
                strcpy(response.payload, "Login successful");
                log_activity("User %s logged in on fd %d\n", u->username, fd);
                send_message(fd, &response);

                // Notify friends
                notify_friends(u, 1);

                // Send Friend List Immediately
                Message fl_msg;
                memset(&fl_msg, 0, sizeof(fl_msg));
                fl_msg.type = MSG_GET_FRIEND_LIST;
                strcpy(fl_msg.payload, "");
                for(int i=0; i<u->friend_count; i++) {
                    char buf[64];
                    User *f = find_user(u->friends[i].username);
                    int online = f ? f->is_online : 0;
                    sprintf(buf, "%s:%d:%d;", u->friends[i].username, u->friends[i].status, online);
                    strcat(fl_msg.payload, buf);
                }
                send_message(fd, &fl_msg);
                
                // Check offline messages
                check_offline_messages(u);
                
                return; // Response already sent
            }
        } else {
            response.type = MSG_ERROR;
            strcpy(response.payload, "Invalid credentials");
            send_message(fd, &response);
        }

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

        if(!u) return;

        response.type = MSG_GET_FRIEND_LIST;
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
    } else if (msg->type == MSG_FRIEND_REJECT) {
        // Remove the pending friend request
        User *me = find_user(msg->sender);
        User *other = find_user(msg->recipient);
        
        if (me && other) {
            // Remove from me (rejection)
            // Ideally we should have a remove_friend_db function but lets do inline for now or add to db.c
            // We reuse remove_friend logic or implement "reject" specifically?
            // "reject" is essentially removing the pending status.
            
            // Remove from me
            int found = 0;
            for(int i=0; i<me->friend_count; i++) {
                if(strcmp(me->friends[i].username, other->username)==0) {
                     // Shift remaining
                     for(int j=i; j<me->friend_count-1; j++) me->friends[j] = me->friends[j+1];
                     me->friend_count--;
                     found = 1;
                     break;
                }
            }
            // Remove from other
            for(int i=0; i<other->friend_count; i++) {
                if(strcmp(other->friends[i].username, me->username)==0) {
                     for(int j=i; j<other->friend_count-1; j++) other->friends[j] = other->friends[j+1];
                     other->friend_count--;
                     break;
                }
            }
            
            save_data();
            response.type = MSG_SUCCESS;
            strcpy(response.payload, "Friend request rejected");
            send_message(fd, &response);

            if(other->is_online && found) {
                 Message notify;
                 notify.type = MSG_NOTIFY;
                 strcpy(notify.sender, me->username);
                 sprintf(notify.payload, "%s rejected your friend request", me->username);
                 send_message(other->socket_fd, &notify);
            }
        } else {
             response.type = MSG_ERROR;
             strcpy(response.payload, "User not found");
             send_message(fd, &response);
        }

    } else if (msg->type == MSG_REMOVE_FRIEND) {
        // me removed other
        User *me = find_user(msg->sender);
        User *other = find_user(msg->recipient);
        if(me && other) {
            // Remove from me
            int found = 0;
            for(int i=0; i<me->friend_count; i++) {
                if(strcmp(me->friends[i].username, other->username)==0) {
                     for(int j=i; j<me->friend_count-1; j++) me->friends[j] = me->friends[j+1];
                     me->friend_count--;
                     found = 1;
                     break;
                }
            }
            // Remove from other
            for(int i=0; i<other->friend_count; i++) {
                if(strcmp(other->friends[i].username, me->username)==0) {
                     for(int j=i; j<other->friend_count-1; j++) other->friends[j] = other->friends[j+1];
                     other->friend_count--;
                     break;
                }
            }
            save_data();
            response.type = MSG_SUCCESS;
            strcpy(response.payload, "Friend removed");
            
             // Notify other
            if(other->is_online && found) {
                 Message notify;
                 notify.type = MSG_NOTIFY;
                 strcpy(notify.sender, me->username);
                 sprintf(notify.payload, "%s removed you from friends", me->username);
                 send_message(other->socket_fd, &notify);
            }
        } else {
             response.type = MSG_ERROR; strcpy(response.payload, "User not found");
        }
        send_message(fd, &response);

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

    } else if (msg->type == MSG_LEAVE_GROUP) {
        if(leave_group(msg->payload, msg->sender)) {
             response.type = MSG_SUCCESS; strcpy(response.payload, "Left group");
        } else {
             response.type = MSG_ERROR; strcpy(response.payload, "Failed to leave group");
        }
        send_message(fd, &response);

    } else if (msg->type == MSG_KICK_GROUP) {
         // payload=group, recipient=target_user, sender=requester
         Group *g = find_group(msg->payload);
         if(!g) {
              response.type = MSG_ERROR; strcpy(response.payload, "Group not found");
         } else {
             // Check if sender is owner (member[0])
             if(strcmp(g->members[0], msg->sender) != 0) {
                  response.type = MSG_ERROR; strcpy(response.payload, "Only owner can kick members");
             } else {
                 if(strcmp(msg->recipient, msg->sender) == 0) {
                      response.type = MSG_ERROR; strcpy(response.payload, "Cannot kick yourself");
                 } else {
                      if(leave_group(msg->payload, msg->recipient)) { // Reuse leave logic
                           response.type = MSG_SUCCESS; strcpy(response.payload, "Member kicked");
                           // Notify kicked user?
                           User* kicked = find_user(msg->recipient);
                           if(kicked && kicked->is_online) {
                                Message notify; notify.type = MSG_NOTIFY; strcpy(notify.sender, "SYSTEM");
                                sprintf(notify.payload, "You were kicked from group %s", msg->payload);
                                send_message(kicked->socket_fd, &notify);
                           }
                      } else {
                           response.type = MSG_ERROR; strcpy(response.payload, "Member not found in group");
                      }
                 }
             }
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

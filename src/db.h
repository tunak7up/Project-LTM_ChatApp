#ifndef DB_H
#define DB_H

#include "common.h"

// Define Friend Status
typedef enum {
    FRIEND_NONE,
    FRIEND_PENDING,
    FRIEND_ACCEPTED
} FriendStatus;

typedef struct {
    char username[MAX_USERNAME];
    FriendStatus status; // PENDING means 'username' sent request to 'me' or vice versa? 
                         // Implementation detail: Simple list of friends with status.
} Friend;

typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int socket_fd; // Runtime only
    int is_online; // Runtime only
    
    // Friends
    Friend friends[MAX_FRIENDS];
    int friend_count;
    
    // Offline Messages (Simple implementation: just store last few? or append to file?)
    // For now, let's keep it simple in memory or basic file append.
} User;

typedef struct {
    char name[MAX_GROUP_NAME];
    char members[MAX_MEMBERS][MAX_USERNAME];
    int member_count;
} Group;

// Globals (In a real app, pass context. Here simple globals for assignment)
extern User users[100]; // Max 100 users for simplicity
extern int user_count;

extern Group groups[MAX_GROUPS];
extern int group_count;

// Functions
void load_data();
void save_data();

// User Ops
int register_user(const char* username, const char* password);
User* find_user(const char* username);
User* find_user_by_fd(int fd); // For runtime
int check_login(const char* username, const char* password);

// Friend Ops
void add_friend_request(User* sender, User* receiver);
void accept_friend(User* user, const char* friend_name);
void reject_friend(User* user, const char* friend_name);
void remove_friend(User* user, const char* friend_name);
FriendStatus get_friend_status(User* user, const char* other_name);

// Group Ops create_group
int create_group(const char* group_name, const char* owner);
int join_group(const char* group_name, const char* username);
int leave_group(const char* group_name, const char* username);
Group* find_group(const char* group_name);

#endif

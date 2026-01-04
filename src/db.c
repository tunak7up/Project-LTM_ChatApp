#include "db.h"

User users[100];
int user_count = 0;

Group groups[MAX_GROUPS];
int group_count = 0;

void load_data() {
    FILE *fp = fopen("data/users.txt", "r");
    if (fp) {
        char name[MAX_USERNAME], pass[MAX_PASSWORD];
        while (user_count < 100 && fscanf(fp, "%31s %31s", name, pass) != EOF) {
            if (find_user(name)) continue; // Skip duplicates
            
            strcpy(users[user_count].username, name);
            strcpy(users[user_count].password, pass);
            users[user_count].socket_fd = -1;
            users[user_count].is_online = 0;
            users[user_count].friend_count = 0;
            user_count++;
        }
        fclose(fp);
    }

    fp = fopen("data/friends.txt", "r");
    char u1[MAX_USERNAME], u2[MAX_USERNAME];
    int status;
    if (fp) {
        while (fscanf(fp, "%s %s %d", u1, u2, &status) != EOF) {
            User *user1 = find_user(u1);
            User *user2 = find_user(u2);
            if (user1 && user2) {
                // Check if already friends
                int exists = 0;
                for(int i=0; i<user1->friend_count; i++) {
                    if(strcmp(user1->friends[i].username, u2) == 0) {
                        exists = 1; 
                        break;
                    }
                }
                
                if (!exists) {
                    strcpy(user1->friends[user1->friend_count].username, u2);
                    user1->friends[user1->friend_count].status = (FriendStatus)status;
                    user1->friend_count++;
                }

                // Add to user2 (symmetrically check)
                // Note: The file might contain "u2 u1 status" later, so we should be careful.
                // Current logic seems to rely on the file having one or both directions?
                // The existing code didn't add to user2 here explicitly?
                // Wait, looking at previous code (lines 39-60 in view_file 318):
                // It treats file line "u1 u2 status" as defining the relation.
                // The block I see in step 318 lines 33-60:
                /*
                if (user1 && user2) {
                    // Add to user1
                    strcpy(user1->friends[user1->friend_count].username, u2);
                     ...
                }
                */
                // It ONLY adds to user1. This implies the file is expected to have "u2 u1" as well?
                // OR the loop in save_data writes both?
                // save_data (lines 114-126 of step 318) iterates ALL users and saves their friends.
                // So yes, the file contains "u1 u2" AND "u2 u1".
                // So we only need to add to user1 here, and let the loop handle the "u2 u1" line when it comes.
                
                // HOWEVER, if the file is corrupted and has "u1 u2" twice, we need the check above.
            }
        }
        fclose(fp);
    }
    
    // Load Groups
    // Format: groupname member1 member2 ... (one line per group)
    // Actually, simple format: 'groupname member_count m1 m2 ...'
    fp = fopen("data/groups.txt", "r");
    if (fp) {
        char gname[MAX_GROUP_NAME];
        int count;
        while(group_count < MAX_GROUPS && fscanf(fp, "%31s %d", gname, &count) != EOF) {
            if(find_group(gname)) {
                // Skip members if group exists
                char tmp[MAX_USERNAME];
                for(int i=0; i<count; i++) fscanf(fp, "%31s", tmp);
                continue; 
            }
            strcpy(groups[group_count].name, gname);
            groups[group_count].member_count = 0; // Load carefully
            
            for(int i=0; i<count && i < MAX_MEMBERS; i++) {
                char memberName[MAX_USERNAME];
                fscanf(fp, "%31s", memberName);
                strcpy(groups[group_count].members[i], memberName);
                groups[group_count].member_count++;
            }
            group_count++;
        }
        fclose(fp);
    }
}

void save_data() {
    FILE *fp = fopen("data/users.txt", "w");
    if (fp) {
        for (int i = 0; i < user_count; i++) {
            fprintf(fp, "%s %s\n", users[i].username, users[i].password);
        }
        fclose(fp);
    }
    
    // Save friends
    // We need to avoid duplicates.
    // Allow naive save for now: only save if u1 < u2 lexicographically to avoid double lines?
    // Or just append.
    // Better: Re-write friends.txt
    fp = fopen("data/friends.txt", "w");
    if (fp) {
        // Iterate all users, save their friends.
        // This might duplicate.
        // Strategy: Save all directed relations.
        // Format: u1 u2 status
        for(int i=0; i<user_count; i++) {
            for(int j=0; j<users[i].friend_count; j++) {
                // To avoid duplicates, only save if users[i].username < friend.username?
                // But status implies direction for PENDING.
                // If ACCEPTED, it's bidirectional.
                // Let's just save "Requester Responder Status"
                // This is getting complex to sync.
                // SIMPLIFICATION:
                // Only save when the event happens (register, friend request).
                // But we need safe exit.
                fprintf(fp, "%s %s %d\n", users[i].username, users[i].friends[j].username, users[i].friends[j].status);
            }
        }
        fclose(fp);
    }

    // Save Groups
    fp = fopen("data/groups.txt", "w");
    if(fp) {
        for(int i=0; i<group_count; i++) {
            fprintf(fp, "%s %d", groups[i].name, groups[i].member_count);
            for(int j=0; j<groups[i].member_count; j++) {
                fprintf(fp, " %s", groups[i].members[j]);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
}

int register_user(const char* username, const char* password) {
    if (find_user(username)) return 0; // Exists
    strcpy(users[user_count].username, username);
    strcpy(users[user_count].password, password);
    users[user_count].is_online = 0; // False
    users[user_count].socket_fd = -1;
    users[user_count].friend_count = 0;
    user_count++;
    save_data();
    return 1;
}

User* find_user(const char* username) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return &users[i];
        }
    }
    return NULL;
}

User* find_user_by_fd(int fd) {
    for (int i = 0; i < user_count; i++) {
        if (users[i].socket_fd == fd && users[i].is_online) {
            return &users[i];
        }
    }
    return NULL;
}

int check_login(const char* username, const char* password) {
    User* u = find_user(username);
    if (u && strcmp(u->password, password) == 0) return 1;
    return 0;
}

// Friend Stubs (Need more logic in Server usually, but here is fine)
// We will implement full logic in server to handle notifications
// But helper functions here:

void add_friend_request(User* sender, User* receiver) {
    // Add receiver to sender's list as PENDING
    // Add sender to receiver's list as PENDING_INCOMING (Let's use a convention)
    // For simplicity, handle generic "PENDING"
    // sender -> PENDING -> receiver
    // receiver -> PENDING -> sender
    
    // Check if already friends
    for(int i=0; i<sender->friend_count; i++) {
        if(strcmp(sender->friends[i].username, receiver->username) == 0) return;
    }

    strcpy(sender->friends[sender->friend_count].username, receiver->username);
    sender->friends[sender->friend_count].status = FRIEND_PENDING;
    sender->friend_count++;
    
    // For receiver, maybe we don't add yet? OR we add with a different status?
    // Let's assume symmetric storage.
    strcpy(receiver->friends[receiver->friend_count].username, sender->username);
    receiver->friends[receiver->friend_count].status = FRIEND_PENDING; // Needs Differentiation? 
    // Usually: 
    // Alice requests Bob.
    // Alice: friend Bob (Status: SentRequest)
    // Bob: friend Alice (Status: RecvRequest)
    // We defined enum: FRIEND_NONE, FRIEND_PENDING, FRIEND_ACCEPTED.
    // Let's add FRIEND_INCOMING to enum in db.h or just use PENDING?
    // Let's stick to PENDING. Logic in server: if I didn't send it, it's incoming.
    receiver->friend_count++;
    
    save_data();
}

int create_group(const char* group_name, const char* owner) {
    if(find_group(group_name)) return 0;
    strcpy(groups[group_count].name, group_name);
    groups[group_count].member_count = 1;
    strcpy(groups[group_count].members[0], owner);
    group_count++;
    save_data();
    return 1;
}

Group* find_group(const char* group_name) {
    for(int i=0; i<group_count; i++) {
        if(strcmp(groups[i].name, group_name) == 0) return &groups[i];
    }
    return NULL;
}

int join_group(const char* group_name, const char* username) {
    Group* g = find_group(group_name);
    if(!g) return 0;
    // Check if member
    for(int i=0; i<g->member_count; i++) {
        if(strcmp(g->members[i], username) == 0) return 0; // Already in
    }
    if(g->member_count >= MAX_MEMBERS) return 0;
    strcpy(g->members[g->member_count], username);
    g->member_count++;
    save_data();
    return 1;
}

int leave_group(const char* group_name, const char* username) {
    Group* g = find_group(group_name);
    if(!g) return 0;
    int idx = -1;
    for(int i=0; i<g->member_count; i++) {
        if(strcmp(g->members[i], username) == 0) {
            idx = i;
            break;
        }
    }
    if(idx == -1) return 0;
    
    // Remove
    for(int i=idx; i<g->member_count-1; i++) {
        strcpy(g->members[i], g->members[i+1]);
    }
    g->member_count--;
    save_data();
    return 1;
}


#ifndef CONNECTED_CLIENT_H_
#define CONNECTED_CLIENT_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#include "packet.h"

// #define MAX_PASSWORD_LENGTH 30

typedef struct connected_client_struct {
    char username[MAX_USERNAME_LENGTH]; // only valid if logged_in
    bool logged_in;
    // char pwd[MAX_PASSWORD_LENGTH];

    int client_sockfd;
    struct sockaddr client_addr;
    pthread_t thread_num;
    struct session_struct *joined_sessions;
    struct connected_client_struct *next;     

} ConnectedClient;

typedef struct session_struct {
    int ID;
    struct session_struct *next;
    struct connected_client_struct *all_clients;     

} Session;

bool in_list_connected_client(const ConnectedClient *list_head, const char *username){
    const ConnectedClient *ptr = list_head;
    while(ptr != NULL){
        if (strcasecmp(ptr->username, username) == 0){
            return true;
        }
        ptr = ptr -> next;
    }
    return false;
}

bool in_list_session(const Session *list_head, int session_id){
    const Session *ptr = list_head;
    while(ptr != NULL){
        if (ptr->ID == session_id){
            return true;
        }
        ptr = ptr -> next;
    }
    return false;
}

bool client_in_session(const Session *list_head, int session_id, const char *username){
    const Session *ptr = list_head;
    while(ptr != NULL){
        if (ptr->ID == session_id){
            return in_list_connected_client(ptr->all_clients, username);
        }
        ptr = ptr -> next;
    }
    perror("SERVER: session does not exist!");
    exit(EXIT_FAILURE);
}

ConnectedClient *add_to_connected_list(ConnectedClient *list_head, ConnectedClient *new_head){
    new_head->next = list_head;
    return new_head;    
}

ConnectedClient *remove_from_connected_list(ConnectedClient *list_head, const char* username){
    ConnectedClient *ptr = list_head;
    ConnectedClient *prev_ptr = NULL;
    ConnectedClient *new_head;
    while(ptr != NULL){
        if (strcasecmp(ptr->username, username) == 0){
            if (prev_ptr != NULL){
                prev_ptr->next = ptr->next;
                free(ptr);
            }
            else{
                list_head = ptr->next;
                free(ptr);
            }
            return list_head;
            // remove
            // remember to free mem
        }
        prev_ptr = ptr;
        ptr = ptr->next;
    }
    perror("SERVER <remove_from_connected_list> username does not exist!\n");
    exit(EXIT_FAILURE);
}

Session *add_to_active_sessions(Session *list_head, Session *new_head){
    new_head->next = list_head;
    return new_head;  
}

// Session *remove_from_active_sessions(Session *list_head, int session_id){
//     new_head->next = list_head;
//     return new_head;  
// }

void add_client_to_session(Session *list_head, ConnectedClient *new_client_head, int session_id){
    Session *ptr = list_head;
    while(ptr != NULL){
        if (ptr->ID == session_id){
            ptr->all_clients = add_to_connected_list(ptr->all_clients, new_client_head);
            return;
        }
        ptr = ptr -> next;
    }
}
Session *remove_client_from_sessions(Session *list_head, const char *username, int session_id){
    Session *ptr = list_head;
    Session *prev_ptr = NULL;
    while(ptr != NULL){
        if (ptr->ID == session_id){
            ptr->all_clients = remove_from_connected_list(ptr->all_clients, username);
            if (ptr->all_clients == NULL){
                // this session has no more users, delete this session from active sessions
                if (prev_ptr == NULL){
                    list_head = ptr->next;
                }
                else{
                    prev_ptr->next = ptr->next;
                }
                free(ptr);
            }
            return list_head;
        }
        prev_ptr = ptr;
        ptr = ptr -> next;

    }
    perror("SERVER <remove_client_from_sessions>: session does not exist!\n");
    exit(EXIT_FAILURE);
}

const ConnectedClient *get_all_active_clients_in_session(int session_id, const Session* all_sessions){
    const Session *ptr = all_sessions;
    while(ptr != NULL){
        if (ptr->ID == session_id){
            return ptr->all_clients;
        }
        ptr = ptr->next;
    }
    perror("SERVER <get_all_active_clients_in_session> session does not exist!\n");
    exit(EXIT_FAILURE);
}

#endif
// take in command

// hard code a list of username and passwords


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include "connected_client.h"
#define MAX_PENDING_CONNECTIONS 15

// hard-coded list of users and passwords
char* usernames[5] = {"aaa", "bbb", "ccc", "eee", "fff"};
char* passwords[5] = {"sdf", "234", "xfg", "d3g", "fg5"};

Session *all_active_sessions = NULL;
ConnectedClient *all_logged_in_clients = NULL;
int largest_session_created = -1;

//locks
pthread_mutex_t all_logged_in_clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t all_active_sessions_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t largest_session_created_mutex = PTHREAD_MUTEX_INITIALIZER;

bool validate_username_password(char *username, char *password){
    printf("SERVER: validating username %s with password %s\n", username, password);
    for (int i = 0; i < sizeof(usernames) / sizeof(usernames[0]); i++){
        if (strcasecmp(usernames[i], username) == 0){
            printf("SERVER: found matching usernames\n");
            //check if password is right
            if (strcasecmp(passwords[i], password) == 0){
                printf("SERVER: valid!\n");
                return true;
            }
            return false;
        }
    }
    return false;
}

void *client_thread_fn(void *arg){
    // handles connected users
    printf("SERVER: thread started\n");
    ConnectedClient *client = (ConnectedClient *)arg;
    // initialize newly connected client information
    client -> logged_in = false;
    client -> joined_sessions = NULL;
    client -> next = NULL;
    
    // helper variables
    char buffer[BUFFER_LEN];
    char list_buffer[BUFFER_LEN];
    Packet packet_from_client;
    Packet packet_to_client;
    ssize_t recv_len;
    while(1) {
        // memset(buffer, 0, BUFFER_LEN);
        memset(buffer, 0, sizeof(char) * BUFFER_LEN);
        memset(&packet_from_client, 0, sizeof(packet_from_client));
        memset(&packet_to_client, 0, sizeof(packet_to_client));

        // wait for client's packets
        if((recv_len = recv(client->client_sockfd, buffer, BUFFER_LEN - 1, 0)) == -1) {
            perror("SERVER: receiving from client failed!\n");
            exit(EXIT_FAILURE);
        }
        buffer[recv_len] = '\0';
        if (!strlen(buffer)){
            // printf("empty string received, skipping this section\n");
            continue;
        }
        buffer_to_packet(buffer, &packet_from_client);
        
        bool send_packet = true;
        int session_id;
        switch(packet_from_client.type){
            case LOGIN:
                printf("SERVER: client attempting log in...\n");
                if (!client->logged_in){
                    printf("packet data is %s\n", packet_from_client.data);
                    char *username = strtok(packet_from_client.data, ",");
                    char *password = username + strlen(username) + 1;
                    bool valid = validate_username_password(username, password);
                    if (valid){
                        // check if another user with the same credential has already logged in
                        pthread_mutex_lock(&all_logged_in_clients_mutex);
                        bool credential_in_use = in_list_connected_client(all_logged_in_clients, username);
                        pthread_mutex_unlock(&all_logged_in_clients_mutex);
                        if (credential_in_use){
                            packet_to_client.type = LO_NAK;
                            strcpy(packet_to_client.data, "credential already in use by another client");
                            printf("SERVER: log in failed! credential already in use by another client\n");
                        }
                        else{
                            packet_to_client.type = LO_ACK;
                            printf("SERVER: %s successful log in\n", client->username);

                            client->logged_in = true;
                            strcpy(client->username, username);
                            // logging user in
                            ConnectedClient *new_head = (ConnectedClient *)malloc(sizeof(ConnectedClient));
                            memcpy(new_head, client, sizeof(ConnectedClient));
                            pthread_mutex_lock(&all_logged_in_clients_mutex);
                            all_logged_in_clients = add_to_connected_list(all_logged_in_clients, new_head);
                            pthread_mutex_unlock(&all_logged_in_clients_mutex);
                        }
                        // if everything works out, you should log them in
                        // and update the list of logged in users (may need to acquire a lock for that)
                        // update the connected_user struct as well
                    }
                    else{
                        packet_to_client.type = LO_NAK;
                        strcpy(packet_to_client.data, "incorrect username or password");
                        printf("SERVER: log in failed! incorrect username or password\n");
                    }
                }
                else{
                    // send failure message: you are already logged in
                    packet_to_client.type = LO_NAK;
                    strcpy(packet_to_client.data, "already logged in");
                    printf("SERVER: log in failed! already logged in\n");
                }
                break;
            case EXIT:
                printf("SERVER: client is requesting exiting...\n");
                if (client->logged_in){
                    //leave all sessions
                    Session *ptr = client->joined_sessions;
                    Session *temp_ptr;
                    client->joined_sessions = NULL;
                    while (ptr != NULL){
                        temp_ptr = ptr;
                        ptr = ptr->next;
                        pthread_mutex_lock(&all_active_sessions_mutex);
                        all_active_sessions = remove_client_from_sessions(all_active_sessions, client->username, temp_ptr->ID);
                        pthread_mutex_unlock(&all_active_sessions_mutex);
                        free(temp_ptr);
                    }
                    printf("SERVER: %s successfully left from all sessions\n", client->username);

                    // remove it from all logged in users
                    pthread_mutex_lock(&all_logged_in_clients_mutex);
                    all_logged_in_clients = remove_from_connected_list(all_logged_in_clients, client->username);
                    pthread_mutex_unlock(&all_logged_in_clients_mutex);
                    printf("SERVER: %s successfully logged out\n", client->username);
                }
                close(client->client_sockfd);
                free(client);
                printf("SERVER: successfully exits, see you next time!\n");
                return NULL;
                break;
            case JOIN:
                session_id = atoi(packet_from_client.data);
                printf("SERVER: client is requesting to join session %d\n", session_id);
                if (client->logged_in){
                    // does this session exist?
                    pthread_mutex_lock(&all_active_sessions_mutex);
                    bool session_exist = in_list_session(all_active_sessions, session_id);
                    pthread_mutex_unlock(&all_active_sessions_mutex);
                    if (session_exist){
                        // is it already in the session?
                        pthread_mutex_lock(&all_active_sessions_mutex);
                        bool already_in_session = client_in_session(all_active_sessions, session_id, client->username);
                        pthread_mutex_unlock(&all_active_sessions_mutex);
                        if (already_in_session){
                            packet_to_client.type = JN_NAK;
                            snprintf(packet_to_client.data, MAX_DATA, "%d,%s", session_id, "You are already in!");
                        }
                        else{
                            printf("SERVER: %s successfully join session %d\n", client->username, session_id);
                            packet_to_client.type = JN_ACK;
                            snprintf(packet_to_client.data, MAX_DATA, "%d", session_id);

                            // add this client to that session's active users
                            ConnectedClient *new_client_head = (ConnectedClient *)malloc(sizeof(ConnectedClient));
                            memcpy((void *)new_client_head, (void *)client, sizeof(ConnectedClient));
                            pthread_mutex_lock(&all_active_sessions_mutex);
                            add_client_to_session(all_active_sessions, new_client_head, session_id);
                            pthread_mutex_unlock(&all_active_sessions_mutex);

                            //add this session to client's sessions_joined
                            Session *new_session = (Session *)malloc(sizeof(Session));
                            new_session->ID = session_id;
                            new_session->next = NULL;
                            client->joined_sessions = add_to_active_sessions(client->joined_sessions, new_session);
                        }
                    }
                    else{
                        packet_to_client.type = JN_NAK;
                        snprintf(packet_to_client.data, MAX_DATA, "%d,%s", session_id, "session does not exist!");
                    }
                }
                else{
                    packet_to_client.type = JN_NAK;
                    snprintf(packet_to_client.data, MAX_DATA, "%d,%s", session_id, "log in first!");
                }
                break;
            case LEAVE_SESS:
                // assuming leave all sessions
                printf("client is requesting to leave all sessions...\n");
                if (client->logged_in){
                    Session *ptr = client->joined_sessions;
                    Session *temp_ptr;
                    client->joined_sessions = NULL;
                    while (ptr != NULL){
                        temp_ptr = ptr;
                        ptr = ptr->next;
                        pthread_mutex_lock(&all_active_sessions_mutex);
                        all_active_sessions = remove_client_from_sessions(all_active_sessions, client->username, temp_ptr->ID);
                        pthread_mutex_unlock(&all_active_sessions_mutex);
                        free(temp_ptr);
                    }
                    printf("SERVER: %s successfully left from all sessions\n", client->username);
                }
                else{
                    printf("SERVER: client not logged in, nothing needs to be done\n");
                }
                send_packet = false;
                break;
            case NEW_SESS:
                printf("client is requesting to create a new session...\n");
                // assuming that the user automatically joins the session when it created it
                if (client->logged_in){
                    printf("successfully created new session\n");
                    session_id = largest_session_created + 1;
                    pthread_mutex_lock(&largest_session_created_mutex);
                    largest_session_created += 1;
                    pthread_mutex_unlock(&largest_session_created_mutex);

                    // add to list of active_Sessions
                    Session *new_session = (Session *)malloc(sizeof(Session));
                    ConnectedClient *new_client_head = (ConnectedClient *)malloc(sizeof(ConnectedClient));
                    memcpy((void *)new_client_head, (void *)client, sizeof(ConnectedClient));
                    new_session->all_clients = new_client_head;
                    new_session->ID = session_id;
                    new_session->next = NULL;
                    
                    // add this session to active_sessions
                    pthread_mutex_lock(&all_active_sessions_mutex);
                    all_active_sessions = add_to_active_sessions(all_active_sessions, new_session);
                    pthread_mutex_unlock(&all_active_sessions_mutex);
                        
                    // add another session to the client's joined sessions
                    Session *new_session2 = (Session *)malloc(sizeof(Session));
                    new_session2->ID = session_id;
                    new_session2->next = NULL;
                    client->joined_sessions = add_to_active_sessions(client->joined_sessions, new_session2);

                    printf("%s joins session %d\n", client->username, session_id);
                    //send packet response
                    packet_to_client.type = NS_ACK;
                    snprintf(packet_to_client.data, MAX_DATA, "%d", session_id);
                }
                else{
                    printf("SERVER: client not logged in, no session is created\n");
                    // nothing can happen because it is not logged in yet
                }
                break;
            case MESSAGE:
                // check if it is logged in
                printf("client is trying to send a message to all...\n");
                if (client -> logged_in){
                    strcpy(packet_to_client.data, packet_from_client.data);
                    strcpy(packet_to_client.source, packet_from_client.source);
                    packet_to_client.size = strlen(packet_to_client.data);
                    packet_to_client.type = MESSAGE;
                    packet_to_buffer(&packet_to_client, buffer);

                    printf("SERVER broadcastiing %s's message: %s\n", client->username, buffer);

                    // find all the sessions this client is in
                    // for every session, send a message to everyone (except this node) in the session
                    // this message. 
                    Session *session_ptr = client->joined_sessions;
                    while(session_ptr != NULL){
                        pthread_mutex_lock(&all_active_sessions_mutex);
                        const ConnectedClient *active_client_ptr = get_all_active_clients_in_session(session_ptr->ID, all_active_sessions);
                        pthread_mutex_unlock(&all_active_sessions_mutex);
                        while(active_client_ptr != NULL){
                            if (active_client_ptr->client_sockfd != client->client_sockfd){
                                // send this message to them
                                ssize_t send_len;
                                if((send_len = send(active_client_ptr->client_sockfd, buffer, BUFFER_LEN - 1, 0)) == -1) {
                                    perror("SERVER: error when trying to broadcast the message\n");
                                    exit(EXIT_FAILURE);
                                }
                                printf("successfully broadcasted message %s to %s\n", packet_to_client.data, active_client_ptr->username);
                            }
                            active_client_ptr = active_client_ptr->next;
                        }
                        session_ptr = session_ptr->next;
                    }
                    printf("SERVER: successfully broadcasted all messages\n");
                }
                else{
                    printf("client is not logged in, no message is sent\n");
                }
                send_packet = false;
                break;
            case QUERY:
                // format: "username:username,session_id:session_id"
                // use the buffer temporarily
                printf("SERVER: client is requesting a query...\n");
                packet_to_client.type = QU_ACK;
                memset(buffer, 0, BUFFER_LEN);
                memset(list_buffer, 0, BUFFER_LEN);
                pthread_mutex_lock(&all_logged_in_clients_mutex);
                ConnectedClient *logged_in_client_ptr = all_logged_in_clients;
                // find all logged in client
                while (logged_in_client_ptr != NULL){
                    printf("active user %s\n", logged_in_client_ptr->username);
                    strcpy(list_buffer, buffer);
                    snprintf(buffer, BUFFER_LEN, "%s:%s", list_buffer, logged_in_client_ptr->username);
                    logged_in_client_ptr = logged_in_client_ptr->next;
                }
                pthread_mutex_unlock(&all_logged_in_clients_mutex);
                buffer[strlen(buffer)] = ',';
                // find all active sessions
                pthread_mutex_lock(&all_active_sessions_mutex);
                Session *session_ptr = all_active_sessions;
                while (session_ptr != NULL){
                    strcpy(list_buffer, buffer);
                    snprintf(buffer, BUFFER_LEN, "%s:%d", list_buffer, session_ptr->ID);
                    session_ptr = session_ptr->next;
                }
                pthread_mutex_unlock(&all_active_sessions_mutex);
                strcpy((char *)packet_to_client.data, buffer);
                printf("query successfully processed: %s\n", buffer);
                break;
        }
        // within each cases, it is expected that you set the type and set the msg variable
        printf("%s\n", packet_to_client.data);
        if (send_packet){ //whether to send a packet back to the client
            packet_to_client.size = strlen(packet_to_client.data);
            strcpy(packet_to_client.source, packet_from_client.source);
            packet_to_buffer(&packet_to_client, buffer);

            ssize_t send_len;
            if((send_len = send(client->client_sockfd, buffer, BUFFER_LEN - 1, 0)) == -1) {
                perror("SERVER: error when trying to reply to client\n");
                exit(EXIT_FAILURE);
            }
            printf("llalala\n");
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: <TCP port number to listen on>\n");
        return 1;
    }

    int port = atoi(argv[1]);

    // Create a TCP socket
    int sockfd;
    // open a UDP socket with IPv4
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("SERVER: socket creation error!\n");
        exit(EXIT_FAILURE);
    }

    // specify which address and port the socket should listen to
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr)); // set server address to 0
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port); // Specify the desired port

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("SERVER: binding error!\n");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sockfd, MAX_PENDING_CONNECTIONS) == -1) {
        perror("SERVER: Listen failed!\n");
        exit(EXIT_FAILURE);
    }

    printf("SERVER: Server listening on port %d...\n", port);


    while (1){
        // wait for incoming user connection
        ConnectedClient *client = (ConnectedClient *)malloc(sizeof(ConnectedClient));
        struct sockaddr_storage client_addr;
        socklen_t client_addr_size;
        
        // connect and set up thread
        client -> client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client -> client_sockfd == -1){
            perror("SERVER: accept incoming client connection failed!\n");
            exit(EXIT_FAILURE);
        }
        printf("SERVER: successfully received an incoming client connection\n");
        //handle connected user requests inside threads
        pthread_create(&(client -> thread_num), NULL, client_thread_fn, (void *)client);
    }

    close(sockfd);
    return 0;
}

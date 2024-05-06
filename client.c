#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "packet.h"

// #define MAX_USERNAME_LENGTH 20
// #define MAX_DATA 1024
// #define BUFFER_LEN 1024
#define PORT_RANGE_START 0
#define PORT_RANGE_END 65536

// int sockfd;
bool is_client_active = true; 
bool in_session = false; 
bool is_connected = false; 

char buffer[BUFFER_LEN];
char command[BUFFER_LEN];
// char data[MAX_DATA];

char client_id[MAX_USERNAME_LENGTH]; 
char* client_pwd = NULL; 
char* server_ip = NULL; 
char* server_port = NULL; 
struct addrinfo *server_address_ptr;

void message(const char *m) {
    printf("%s", m);
    return; 
}
void *receive_fn(void *arg){
    printf("inside thread, thread started...\n");
    int *sockfd_ptr = (int *)arg;
    Packet p;
    ssize_t recv_len;
    memset(buffer, 0, sizeof(char) * BUFFER_LEN);
    while(1){
        if ((recv_len = recv(*sockfd_ptr, buffer, BUFFER_LEN - 1, 0)) == -1) {
			perror("error receiving message from server\n");
			return NULL;
		}
        buffer[recv_len] = '\0';
        if (!strlen(buffer)){
            // printf("empty string received, skipping this section\n");
            continue;
        }
    

        buffer_to_packet(buffer, &p);
        switch (p.type){
            case JN_ACK:
                in_session = true;
                message("received JN_ACK from server\n");
                printf("Joined session %s successfully.\n", p.data); 
                break;
            case JN_NAK:
                perror("ERROR: join session failed. Received JN_NAK\n"); 
                printf("Error: %s\n", p.data);
                break;
            case NS_ACK:
                in_session = true;
                message("received NS_ACK from server\n"); 
                printf("Created session %s successfully.\n", p.data);
                break;
            case MESSAGE:
                message("received a message from server\n");
                printf("message is %s\n", p.data);
                break;
            case QU_ACK:
                message("received QU_ACK from server, display list of online users and available sessions below \n");
                printf("%s\n", p.data);
                break;
        }
    }
}
// bool is_port_available(int port) {
//     int sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock == -1) {
//         perror("ERROR: socket creation failed in is_port_available \n");
//         exit(EXIT_FAILURE);
//     }

//     struct sockaddr_in server_addr;
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(port);
//     server_addr.sin_addr.s_addr = INADDR_ANY;

//     if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
//         close(sock);
//         return false;
//     }

//     close(sock);
//     return true;
// }

// int find_available_port() {
//     for (int port = PORT_RANGE_START; port <= PORT_RANGE_END; ++port) {
//         char command[50];
//         snprintf(command, sizeof(command), "netstat -an | grep %d", port);
//         FILE* result = popen(command, "r");

//         if (result == NULL) {
//             perror("ERROR: cannot check port availability");
//             return -1; 
//         }

//         char output[1024];
//         if (fgets(output, sizeof(output), result) == NULL) {
//             pclose(result);
//             return port;
//         }
//         pclose(result);
//     }

//     perror("ERROR: no available ports in the specified range\n"); 
//     return -1; 
// } 

void establish_connection(int *sockfd){
    char *client_id_ptr = strtok(NULL, " ");
    client_pwd = strtok(NULL, " ");
    server_ip = strtok(NULL, " ");
    server_port = strtok(NULL, "\n"); 
    if (! client_id_ptr|| ! client_pwd || ! server_ip || ! server_port){
        message("the log in input format is incorrect. \n");
        return;
    }
    strcpy(client_id, client_id_ptr);
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd == -1) {
        perror("ERROR: socket creation failed in is_port_available \n");
        exit(EXIT_FAILURE);
    }
    printf("server port %d\n", atoi(server_port));
    printf("server ip %s\n", server_ip);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(server_port));
    if (inet_pton(AF_INET, server_ip, &(server_addr.sin_addr)) <= 0) {
        perror("Error converting IP address");
        close(*sockfd);
        exit(EXIT_FAILURE);
    }
    // inet_aton(server_ip, &server_addr.sin_addr);
    
    // Connect to the server
    if (connect(*sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to the server?");
        close(*sockfd);
        exit(EXIT_FAILURE);
    }

    // if (! is_port_available(atoi(server_port))) {
    //     message("inputted port not available, trying to find a new available port... \n"); 
    //     int new_port = find_available_port(); 
    //     memset(server_port, 0, sizeof(server_port));
    //     sprintf(server_port, "%d", new_port);
    //     message("new port found \n"); 
    // }

    // struct addrinfo server_address; 
    // memset(&server_address, 0, sizeof(server_address));
    // server_address.ai_family = AF_INET; 
    // server_address.ai_socktype = SOCK_STREAM; 

    // if (getaddrinfo(server_ip, server_port, &server_address, &server_address_ptr) != 0){
    //     message("cannot get address from server\n"); 
    //     return -1;
    // }

    // for (server_address_ptr; server_address_ptr; server_address_ptr = server_address_ptr->ai_next){
    //     *sockfd = socket(server_address_ptr->ai_family, server_address_ptr->ai_socktype, server_address_ptr->ai_protocol);
        
    //     if (*sockfd == -1) {
    //         message("cannot create socket froms server\n"); 
    //         continue;
    //     }

    //     if (connect(*sockfd, server_address_ptr->ai_addr, server_address_ptr->ai_addrlen) == -1) {
    //         message("cannot establish connection from server\n"); 
    //         close(*sockfd);
    //         continue;
    //     }

    //     is_connected = true;
    //     break; 
    // }
    // if (server_address_ptr == NULL){
    //     message("cannot establish connection with server\n"); 
    //     return -1;
    // }


}
int log_in(int *sockfd, pthread_t *t) {
    if (is_connected) {
        message("You are already logged in. \n"); 
        return 0; 
    }
    establish_connection(sockfd);
    Packet p; 
    memset(&p, 0, sizeof(p));
    strncpy(p.source, client_id, MAX_USERNAME_LENGTH);
    snprintf(p.data, MAX_DATA, "%s,%s", client_id, client_pwd);
    // strncpy(p.data, client_pwd, MAX_DATA);
    p.type = LOGIN; 
    p.size = strlen(p.data);
    

    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    packet_to_buffer(&p, new_buffer);
    // buffer_to_packet(new_buffer, &p); 
    printf("new buffer %s \n", new_buffer);
    if (send(*sockfd, new_buffer, BUFFER_LEN - 1, 0) == -1) {
        message("cannot send login information to server\n"); 
        is_connected = false; 
        close(*sockfd);
        return -1;
    }

    memset(new_buffer, 0, sizeof(new_buffer));
    if (recv(*sockfd, new_buffer, BUFFER_LEN, 0) == -1) {
        message("cannot receive login response from server\n"); 
        is_connected = false; 
        close(*sockfd);
        return -1;
    }

    // packet_to_buffer(&p, new_buffer); 
    buffer_to_packet(new_buffer, &p);
    if (p.type == LO_ACK) {
        message("received LO_ACK from server\n"); 
        is_connected = true;
        // start a receive thread
        pthread_create(t, NULL, receive_fn, (void *)sockfd);
        printf("started a receive thread...\n");
        return 0; 
    } else if (p.type == LO_NAK) {
        message("received LO_NAK from server\n"); 
        is_connected = false; 
        // close(*sockfd);
        // printf("closing connection...\n");
        return -1;
    } else {
        message("unknown response from server\n"); 
        is_connected = false; 
        // close(*sockfd);
        // printf("closing connection...\n");
        return -1;
    }

}

int log_out(int sockfd, pthread_t *t) {
    if (! is_connected) {
        message("Cannot log out if you are not logged in \n"); 
        return -1; 
    }

    Packet p; 
    memset(&p, 0, sizeof(p));
    p.type = EXIT; 
    printf("client id is %s\n", client_id);
    strcpy(p.source, client_id);
    p.size = 0; 

    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    // buffer_to_packet(new_buffer, &p); 
    packet_to_buffer(&p, new_buffer); 
    printf("buffer is %s\n", new_buffer);

    if (send(sockfd, new_buffer, BUFFER_LEN, 0) == -1) {
        message("cannot send logout information to server\n"); 
        return -1;
    }
    
    is_connected = false; 
    if (pthread_cancel(*t)) {
			printf("thread cancelling failure\n");
	} else {
			printf("sccuessfully canceld thread\n");	
	}
    sleep(1);
    close(sockfd);
    freeaddrinfo(server_address_ptr);
    return 0; 
}

int join_session(int sockfd, pthread_t *t) {
    if (! is_connected) {
        message("cannot join session if you are not connected \n"); 
        return -1; 
    }

    // if (in_session) {
    //    message("cannot join session if you are currently in a session \n"); 
    //    return -1; 
    //}
    char *session_name = strtok(NULL, "\n");
    printf("session name length %d\n", strlen(session_name));

    if (! session_name) {
        message("session name is not inputted in correct format \n"); 
    }

    Packet p; 
    memset(&p, 0, sizeof(p));
    strcpy(p.source, client_id);
    p.type = JOIN; 
    p.size = strlen(session_name); 
    strcpy(p.data, session_name);
    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    packet_to_buffer(&p, new_buffer);

    if (send(sockfd, new_buffer, BUFFER_LEN, 0) == -1) {
        message("cannot send join_session information to server\n"); 
        return -1;
    }
    return 0;
}

int leave_session(int sockfd, pthread_t *t) {
    if (! is_connected) {
        message("cannot leave session if you are not connected \n"); 
        return -1; 
    }
    if (! in_session) {
        message("cannot leave session if you are not currently in a session \n"); 
        return -1; 
    }

    Packet p; 
    memset(&p, 0, sizeof(p));
    p.type = LEAVE_SESS; 
    p.size = 0; 
    strcpy(p.source, client_id);
    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    // buffer_to_packet(new_buffer, &p); 
    packet_to_buffer(&p, new_buffer);

    if (send(sockfd, new_buffer, BUFFER_LEN, 0) == -1) {
        message("cannot send leave_session info to server\n"); 
        return -1;
    }
    in_session = false; 
    return 0; 
}

int create_session(int sockfd, pthread_t *t) {
    if (! is_connected) {
        message("cannot create session if you are not connected \n"); 
        return -1; 
    }
    char *session_name = strtok(NULL, "\n");

    if (! session_name) {
        message("session name is not inputted in correct format \n"); 
        return -1; 
    }

    Packet p; 
    memset(&p, 0, sizeof(p));
    p.type = NEW_SESS; 
    p.size = strlen(session_name); 
    strcpy(p.data, session_name);
    strcpy(p.source, client_id);
    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    // buffer_to_packet(new_buffer, &p);  
    packet_to_buffer(&p, new_buffer);

    if (send(sockfd, new_buffer, BUFFER_LEN, 0) == -1) {
        message("cannot send create_session information to server\n"); 
        return -1;
    }
    return 0;

}

int list(int sockfd, pthread_t *t) {
    if (! is_connected) {
        message("cannot query list if you are not connected \n"); 
        return -1; 
    }

    Packet p; 
    memset(&p, 0, sizeof(p));
    p.type = QUERY; 
    p.size = 0; 
    strcpy(p.source, client_id);
    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    packet_to_buffer(&p, new_buffer); 

    if (send(sockfd, new_buffer, BUFFER_LEN, 0) == -1) {
        message("cannot send list request to server\n"); 
        return -1;
    }
    return 0;
}

int quit(int sockfd, pthread_t *t) {
    return 0; 
}

int send_text(int sockfd, pthread_t *t, char * start) {
    Packet p; 
    memset(&p, 0, sizeof(p));
    p.type = MESSAGE; 
    p.size = strlen(start); 
    strcpy(p.source, client_id);
    strcpy(p.data, start); 
    
    char new_buffer[BUFFER_LEN]; 
    memset(new_buffer, 0, sizeof(new_buffer));
    
    packet_to_buffer(&p, new_buffer); 
    int recv = send(sockfd, new_buffer, BUFFER_LEN, 0); 
    return recv; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {

    int sockfd;
    pthread_t t;

    while (is_client_active) {
        // reset everything at each start 
        // if(buffer){
            memset(buffer, 0, sizeof(buffer));
        // }

        // if(command){
            memset(command, 0, sizeof(command));
        // }

        // if(data){
        //     memset(data, 0, sizeof(data));
        // }

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            perror("Error: cannot get stdin. \n"); 
            break;
        }

        // parse buffer to get rid of start or end whitespaces 
        char *start = buffer;
        while (*start == ' ' || *start == '\t' || *start == '\n') {
            start++;
        }
        char *end = buffer + strlen(buffer) - 1;
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n')) {
            *end-- = '\0';
        }
        if (*start == '\0') {
            continue;
        }

        // copy the cleaned input to command, and extract the first argument 
        strcpy(command, start);
        char *data = strtok(command, " ");
        

        if (strcmp(data, "/login") == 0){
            if (log_in(&sockfd, &t) == -1) {
                perror("ERROR: log in failed.\n");
                // break; 
            } else {
                message("Logged in successfully.\n"); 
            }
            
        } else if (strcmp(data, "/logout") == 0){
            if (log_out(sockfd, &t) == -1) {
                perror("ERROR: log out failed.\n");
                // break; 
            } else {
                message("Logged out successfully.\n"); 
            }

        } else if (strcmp(data, "/joinsession") == 0){
            join_session(sockfd, &t);
        } else if (strcmp(data, "/leavesession") == 0){
            if (leave_session(sockfd, &t)== -1) {
                perror("ERROR: leave session failed.\n");
                // break; 
            } else {
                message("leave session successfully.\n"); 
            }
        } else if (strcmp(data, "/createsession") == 0){
            if (create_session(sockfd, &t)== -1) {
                perror("ERROR: create session failed.\n");
                // break; 
            }
        } else if (strcmp(data, "/list") == 0){
            list(sockfd, &t);
        } else if (strcmp(data, "/quit") == 0){
            if (is_connected) {
                if (log_out(sockfd, &t) == -1) {
                    message("cannot log out with quit \n"); 
                    perror("ERROR: quit failed.\n");
                    // break; 
                }
                // is_connected = false; 
                // close(sockfd);
                message("Quitted successfully.\n"); 
                break; 
            }
            break;
        } else {
            // the user inputted a message to be sent in the current conference session 
            if (! is_connected || ! in_session) {
                message("cannot send message if you have not logged in or joined a session.\n"); 
                continue; 
            }

            if (send_text(sockfd, &t, start) == -1) { // start contains cleaned data 
                perror("ERROR: fail to send text message.\n"); 
                // break; 
            } 

        }

    }
    return 0; 
}



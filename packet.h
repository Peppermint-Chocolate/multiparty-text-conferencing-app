#ifndef PACKET_H_
#define PACKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_USERNAME_LENGTH 20
#define MAX_DATA 1024
#define BUFFER_LEN 1024

typedef struct packet_struct {
	unsigned int type; // packet type, see enum
    unsigned int size; // data length
    unsigned char source[MAX_USERNAME_LENGTH]; // sending client username
    unsigned char data[MAX_DATA];
} Packet;

enum pktType {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
};

void buffer_to_packet(const char *buffer, Packet *packet) {
    //buffer representation: type:size:source:data
    // printf("SERVER: buffer is %s\n", buffer);
    memset((packet -> data), 0, MAX_DATA);
    memset((packet -> source), 0, MAX_USERNAME_LENGTH);
    
    char *new_buffer = (char *)malloc(strlen(buffer) + 1);
    if (buffer != NULL){
        strcpy(new_buffer, buffer);
    }
    else{
        perror("buffer is NULL");
        exit(EXIT_FAILURE);
    }
    char *token;

    token = strtok(new_buffer, ":");
    packet -> type = atoi(token);
    // printf("SERVER: packet type %d\n", packet -> type);
    token = strtok(NULL, ":");
    packet -> size = atoi(token);
    // printf("SERVER: packet size %d\n", packet -> size);
    token = strtok(NULL, ":"); // pointing to the source
    if (strlen(token) >= MAX_USERNAME_LENGTH){
        perror("source length bigger than MAX_USERNAME_LENGTH");
        exit(EXIT_FAILURE);
    }
    memcpy(packet -> source, token, strlen(token));
    (packet -> source)[strlen(token)] = '\0';
    token = token + strlen(token) + 1; // token now pointing to the data part
    if (strlen(token) >= MAX_DATA){
        perror("data length bigger than MAX_DATA");
        exit(EXIT_FAILURE);
    }
    memcpy(packet -> data, token, strlen(token));
    (packet -> data)[strlen(token)] = '\0';
    // printf("SERVER: packet data %s\n", packet -> data);
    
    free(new_buffer);
}

void packet_to_buffer(const Packet *packet, char *buffer) {
    memset(buffer, 0, BUFFER_LEN);
    snprintf(buffer, BUFFER_LEN, "%u:%u:%s:%s", packet -> type, packet -> size, packet -> source, packet -> data);
    buffer[strlen(buffer)] = '\0';
}

#endif
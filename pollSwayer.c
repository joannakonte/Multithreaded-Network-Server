#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>	         

// Structure to hold thread data
typedef struct {
    char serverName[256];
    int portNum;
    char name[256];
    char surname[256];
    char vote[256];
} ThreadData;

// Function executed by threads
void* sendVote(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char port[6];  // Max length of port number as string
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    sprintf(port, "%d", data->portNum);

    if ((rv = getaddrinfo(data->serverName, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        free(data);  
        return NULL;
    }

    // Loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        freeaddrinfo(servinfo);  // Free the linked list
        free(data);              // Free the memory allocated for thread data
        return NULL;    
    }

    freeaddrinfo(servinfo); // Done with the structure

    // Send the name and vote to the server
    char msgName[512], msgVote[512];
    sprintf(msgName, "%s %s", data->name, data->surname);
    sprintf(msgVote, "%s", data->vote);
    
    // Wait for the server to request the name
    char buffer[256];
    ssize_t n = read(sockfd, buffer, 255);
    if (n < 0) {
        perror("Read error");
        free(data);        // Free the memory allocated for thread data
        return NULL;
    } else {
        buffer[n] = '\0';  // Ensure null termination for correct printing
        printf("%s\n", buffer);
    }
    // Send name
    write(sockfd, msgName, strlen(msgName));
    
    // Wait for the server to request the vote
    n = read(sockfd, buffer, 255);
    if (n < 0) {
        perror("Read error");
        free(data);  
        return NULL;
    } else {
        buffer[n] = '\0';  
        printf("%s\n", buffer);
    }

    // Send vote
    write(sockfd, msgVote, strlen(msgVote));

    // Read the server's confirmation message
    n = read(sockfd, buffer, 255);
    if (n < 0) {
        perror("Read error");
        free(data);  
        return NULL;
    } else {
        buffer[n] = '\0';  
        printf("%s\n", buffer);
    }
    
    // Close connection
    close(sockfd);

    // Free the memory allocated for the thread data
    free(data);

    return NULL;
}


int main(int argc, char **argv) {
    // Check the number of arguments
    if (argc != 4) {
        printf("Usage: %s [serverName] [portNum] [inputFile.txt]\n", argv[0]);
        exit(1);
    }

    char* serverName = argv[1];
    int portNum = atoi(argv[2]);
    char* inputFile = argv[3];

    // Open input file
    FILE* file = fopen(inputFile, "r");
    if (file == NULL) {
        perror("Failed to open input file");
        exit(1);
    }
    
    // Read lines from the input file and create a thread for each one
    char name[256], surname[256], vote[256];
    while (fscanf(file, "%255s %255s %255s", name, surname, vote) == 3) {
        ThreadData* data = malloc(sizeof(ThreadData));
        strncpy(data->serverName, serverName, sizeof(data->serverName) - 1);
        data->portNum = portNum;
        strncpy(data->name, name, sizeof(data->name) - 1);
        strncpy(data->surname, surname, sizeof(data->surname) - 1);
        strncpy(data->vote, vote, sizeof(data->vote) - 1);

        // Create and detach thread
        pthread_t thread;
        if (pthread_create(&thread, NULL, sendVote, data) != 0) {
            perror("Failed to create thread");
            free(data);
            continue;
        }
        pthread_detach(thread);
    }

    // Close input file
    fclose(file);
    
    // Wait for all threads to finish
    pthread_exit(NULL);
    
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 100

// Define the buffer structure  
typedef struct {
    int* connections;           // Array to hold the socket descriptors
    int size;                   // The size of the buffer, indicating the maximum number of connections it can hold
    int front;
    int rear;
    int count;                  // The number of connections currently in the buffer
    pthread_mutex_t mutex;
    pthread_cond_t full;        // Condition variable to handle synchronization between threads
    pthread_cond_t empty;       // Condition variable to handle synchronization between threads
} Buffer;

typedef struct {
    Buffer* buffer;
    char* pollLog;
} ThreadArgs;

typedef struct {
    char partyName[256];
    int votes;
} Party;

// Global variables
Party parties[256];
int totalVotes = 0;
char* pollStats;  

// Custom comparison function for sorting Party votes alphabetically by party name
int comparePartyNames(const void* a, const void* b) {
    const Party* partyA = (const Party*)a;
    const Party* partyB = (const Party*)b;
    return strcmp(partyA->partyName, partyB->partyName);
}

// Write voting statistics to the file and terminate the server
void writeStats(int signum) {
    // Open the stats file
    FILE* file = fopen(pollStats, "w");
    if (file == NULL) {
        perror("Failed to open stats file");
        exit(1);
    }

    // Sort the party votes in alphabetically
    qsort(parties, 256, sizeof(Party), comparePartyNames);

    // Write the sorted party votes to the file
    for (int i = 0; i < 256; i++) {
        if (parties[i].votes > 0) {
            fprintf(file, "%s %d\n", parties[i].partyName, parties[i].votes);
        }
    }

    // Write the total votes
    fprintf(file, "\nTOTAL %d\n", totalVotes);

    // Close the file
    fclose(file);

    // Exit the program
    printf("\nTerminating server...\n");
    exit(0);
}

// Mutex for synchronization in worker threads
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize the buffer  
void initializeBuffer(Buffer* buffer, int size) {
    buffer->connections = malloc(size * sizeof(int));
    buffer->size = size;
    buffer->front = 0;
    buffer->rear = -1;
    buffer->count = 0;
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->full, NULL);
    pthread_cond_init(&buffer->empty, NULL);
}

// Add a connection to the buffer 
void addToBuffer(Buffer* buffer, int connection) {
    // Lock mutex
    pthread_mutex_lock(&buffer->mutex);

    // Wait until there is space available in the buffer
    while (buffer->count == buffer->size) {
        // Wait until the buffer is no longer full
        pthread_cond_wait(&buffer->full, &buffer->mutex);
    }

    // Update the rear index and add the connection to the buffer
    buffer->rear = (buffer->rear + 1) % buffer->size;
    buffer->connections[buffer->rear] = connection;

    // Increment the count to indicate that a new connection has been added
    buffer->count++;

    // Notify waiting worker threads that a connection is available
    pthread_cond_signal(&buffer->empty);

    // Unlock the mutex  
    pthread_mutex_unlock(&buffer->mutex);
}

// Remove a connection from the buffer  
int removeFromBuffer(Buffer* buffer) {
    // Lock mutex
    pthread_mutex_lock(&buffer->mutex);

    // Wait until there is a connection available in the buffer
    while (buffer->count == 0) {
        // Wait until a connection is available in the buffer
        pthread_cond_wait(&buffer->empty, &buffer->mutex);
    }

    // Retrieve the connection from the front of the buffer
    int connection = buffer->connections[buffer->front];

    // Update the front index and decrement the count 
    buffer->front = (buffer->front + 1) % buffer->size;
    buffer->count--;

    // Notify the master thread that there's space available in the buffer
    pthread_cond_signal(&buffer->full);

    // Unlock mutex
    pthread_mutex_unlock(&buffer->mutex);

    // Return the retrieved connection
    return connection;
}

// Function executed by worker threads
void* handleConnection(void* arg) {
    // Get the buffer and the poll-log filename from the arguments
    Buffer* buffer = ((ThreadArgs*)arg)->buffer;
    char* pollLog = ((ThreadArgs*)arg)->pollLog;

    // The worker thread's main loop
    while (1) {
        // Remove connection from the buffer
        int connection = removeFromBuffer(buffer);

        // Request the voter's name
        const char* msgRequestName = "SEND NAME PLEASE\n";
        if (send(connection, msgRequestName, strlen(msgRequestName), 0) < 0) {
            perror("Failed to send request for name");
            close(connection);
            continue;
        }

        // Read the voter's name
        char voterName[256];
        ssize_t n = recv(connection, voterName, sizeof(voterName) - 1, 0);
        if (n < 0) {
            perror("Failed to receive voter name");
        } else {
            voterName[n] = '\0';  // Ensure null termination for correct printing
            printf("%s\n", voterName);
        }

        pthread_mutex_lock(&mutex);  // Lock the mutex

        // Check if voter has already voted
        FILE* file = fopen(pollLog, "r");
        int hasVoted = 0; // Flag to mark if the voter has already voted
        if (file != NULL) {
            char line[512];
            while (fgets(line, sizeof(line), file)) {
                // Remove newline character if present at end of line
                size_t len = strlen(line);
                if (len > 0 && line[len - 1] == '\n') {
                    line[--len] = '\0';
                }
                char* lineName = strtok(line, " ");     // Extract name from line
                char* lineSurname = strtok(NULL, " ");  // Extract surname from line
                if (lineName != NULL && lineSurname != NULL) {
                    char fullName[512];
                    sprintf(fullName, "%s %s", lineName, lineSurname);
                    if (strcmp(fullName, voterName) == 0) {
                        // This voter has already voted
                        const char* msgAlreadyVoted = "ALREADY VOTED\n";
                        if (send(connection, msgAlreadyVoted, strlen(msgAlreadyVoted), 0) < 0) {
                            perror("Failed to send already voted message");
                        }
                        close(connection);
                        hasVoted = 1;
                        break;
                    }
                }
            }
            fclose(file);
        }

        if(hasVoted) {
            pthread_mutex_unlock(&mutex);  // Unlock the mutex
            continue;
        }

        // Request the party's name
        const char* msgRequestVote = "SEND VOTE PLEASE\n";
        if (send(connection, msgRequestVote, strlen(msgRequestVote), 0) < 0) {
            perror("Failed to send request for vote");
            close(connection);
            pthread_mutex_unlock(&mutex);  // Unlock the mutex
            continue;
        }

        // Read the party's name
        char partyName[256];
        n = recv(connection, partyName, sizeof(partyName) - 1, 0);
        if (n < 0) {
            perror("Failed to receive party name");
        } else {
            partyName[n] = '\0';  
            printf("%s\n", partyName);
        }

        // Record the vote
        int i;
        for (i = 0; i < 256; i++) {
            if (strcmp(parties[i].partyName, partyName) == 0) {
                // Found the party, increment its vote count
                parties[i].votes++;
                break;
            } else if (parties[i].votes == 0) {
                // If party wasn't found in the array add the party to the array
                strcpy(parties[i].partyName, partyName);
                parties[i].votes = 1;
                break;
            }
        }
        if (i == 256) {
            fprintf(stderr, "No more room for additional parties\n");
        }

        totalVotes++;

        // Update the poll-log file 
        file = fopen(pollLog, "a");
        if (file != NULL) {
            fprintf(file, "%s %s\n", voterName, partyName);
            fclose(file);
        } else {
            perror("Failed to open poll-log file for appending");
        }

        pthread_mutex_unlock(&mutex);  // Unlock the mutex

        // Send confirmation
        char msgConfirm[512];
        sprintf(msgConfirm, "VOTE for Party %s RECORDED\n", partyName);
        if (send(connection, msgConfirm, strlen(msgConfirm), 0) < 0) {
            perror("Failed to send confirmation");
            close(connection);
            continue;
        }

        // Close the connection
        close(connection);
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    // Check the number of arguments
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [portnum] [numWorkerThreads] [bufferSize] [poll-log] [poll-stats]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int numWorkerThreads = atoi(argv[2]);
    int bufferSize = atoi(argv[3]);
    pollStats = argv[5];

    // Check if bufferSize and numWorkerThreads are greater than 0
    if (bufferSize <= 0 || numWorkerThreads <= 0) {
        fprintf(stderr, "Error: bufferSize and numWorkerThreads must be greater than 0.\n");
        return 1;
    }

    // Initialize parties
    memset(parties, 0, sizeof(parties));

    // Register signal handler for SIGINT
    signal(SIGINT, writeStats);

    // Create socket
    int serverSocket;
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket");
        return 1;
    }

    // Enable socket reuse option
    int reuse = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Failed to set socket option");
        close(serverSocket);
        return 1;
    }

    // Prepare the server address structure
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(port);

    // Bind the socket to the specified port
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Failed to bind");
        close(serverSocket);
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, SOMAXCONN) < 0) {
        perror("Failed to listen");
        close(serverSocket);
        return 1;
    }

    printf("Server listening on port %d\n", port);

    // Initialize the buffer 
    Buffer buffer;
    initializeBuffer(&buffer, bufferSize);
    
    ThreadArgs args;
    args.buffer = &buffer;
    args.pollLog = argv[4];  

    // Create the worker threads
    pthread_t* workerThreads = malloc(numWorkerThreads * sizeof(pthread_t));
    for (int i = 0; i < numWorkerThreads; i++) {
        if (pthread_create(&workerThreads[i], NULL, handleConnection, &args) != 0) {
            perror("Failed to create worker thread");
            return 1;
        }
    }

    // Accept and handle incoming connections in the master thread 
    while (1) {
        // Accept a connection
        int clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == -1) {
            perror("Failed to accept connection");
            continue;
        }
        printf("Accepted a connection.\n"); fflush(stdout);

        // Add the connection to the buffer
        addToBuffer(&buffer, clientSocket);
    }

    // Wait for all worker threads to finish
    for (int i = 0; i < numWorkerThreads; i++) {
        pthread_join(workerThreads[i], NULL);
    }

    free(workerThreads);

    // Close the server socket
    close(serverSocket);

    return 0;
}

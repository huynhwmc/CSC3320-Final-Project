#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#define SHM_SIZE 1024
#define MAX_LIMIT 200 // Maximum buffer/character limit for messages
pthread_mutex_t mutex;  
pthread_cond_t full;
pthread_cond_t empty;

/* Add custom data to shared memory. */
typedef struct
{
    int flag;                    // 0 = empty, 1 = client message, 2 = server response
    pid_t client_pid;            // Client's process ID
    char message[SHM_SIZE - 12]; // Message content
} shared_data_t;

/* Chat client */
int main(int argc, char *argv[])
{
    char *strings = NULL; // Holds the message in dynamic memory
    int shmid; // Shared memory's ID
    void *shared_memory;

    // Define mutual exclusion and conditions
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);

    // Generate unique key
    key_t key = ftok("shmfile", 65);
    shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("Shared memory error: shmget");
        return 1;
    }

    // Create shared memory
    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("Shared memory error: shmat");
        return 1;
    }
    // Allocate dynamic memory for the message
    strings = (char *)malloc(SHM_SIZE);
    if (!strings)
    {
        perror("Memory allocation error: malloc");
        exit(1);
    }

    // User enters a message
    printf("\nEnter a message: ");
    fgets(strings, SHM_SIZE, stdin);
    strings[strcspn(strings, "\n")] = '\0'; // Remove newline
    // Check that the message is not longer than the maximum allowed character limit
    if (strlen(strings) >= SHM_SIZE)
    {
        fprintf(stderr, "The message is too long. Maximum allowed size is %d characters.\n", SHM_SIZE - 1);
        free(strings);
        exit(1);
    }

    // Connect to the chat server using shared memory
    pthread_mutex_lock(&mutex); // Use mutexes to ensure no other process is accessing or updating that memory
    
    // Wait if shared memory is not empty
    while (strlen((char *)shared_memory) != 0)
    {
        pthread_cond_wait(&empty, &mutex);
    }
    
    // Write to shared memory
    strncpy((char *)shared_memory, strings, SHM_SIZE);
    
    // Signal the server that a message is available
    pthread_cond_signal(&full);
    printf("Message successfully sent: %s\n", strings);
    
    
    //ciara work:
    snprintf(strings, SHM_SIZE, "Client PID: %d - %s", getpid(), strings);
    strncpy((char *)shared_memory, strings, SHM_SIZE);

    pthread_cond_signal(&full);
    printf("Sent message: %s\n", strings);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 5; // Timeout in 5 seconds

    int ret = pthread_cond_timedwait(&empty, &mutex, &ts);
    if (ret == 0) {
        printf("Server response: %s\n", (char *)shared_memory);
    } else {
        printf("No response from server within timeout period.\n");
    }
    
    
    pthread_mutex_unlock(&mutex);

    /*
    // Wait for the server's acknowledgment
    pthread_mutex_lock(&mutex);
    pthread_cond_wait(&empty, &mutex);

    // Read acknowledgment
    printf("Server acknowledgment: %s\n", (char *)shared_memory);
    pthread_mutex_unlock(&mutex);
    */

    // Free allocated memory
    free(strings);

    // Detach from shared memory
    shmdt(shared_memory);

    // Mutex cleanup
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);
    return 0;
}

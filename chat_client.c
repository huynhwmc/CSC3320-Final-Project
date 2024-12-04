#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#define SHM_SIZE 1024
#define MAX_LIMIT 200 // Maximum buffer/character limit for messages
pthread_mutex_t mutex;
pthread_cond_t full;
pthread_cond_t empty;

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
        perror("shmget");
        return 1;
    }

    // Create shared memory
    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("shmat");
        return 1;
    }
    // Allocate dynamic memory for the message
    strings = (char *)malloc(SHM_SIZE);
    if (!strings)
    {
        perror("malloc");
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
    printf("Sent message: %s\n", strings);
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

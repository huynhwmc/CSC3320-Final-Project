#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

#define MAX_LIMIT 200 // Maximum buffer/character limit for messages
pthread_mutex_t mutex;
pthread_cond_t full;
pthread_cond_t empty;

/* Chat client */
int main(int argc, char *argv[])
{
    char **strings = NULL; // Holds the message in dynamic memory
    char str[MAX_LIMIT], ch;
    int shmid; // Shared memory's ID
    void *shared_memory;
    char buff[100];
    // Define mutual exclusion
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);

    // Create shared memory
    // Generate unique key
    key_t key = ftok("shmfile", 65);
    shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget");
        return 1;
    }

    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("shmat");
        return 1;
    }

    // User enters a message
    printf("\nEnter a message: ");
    fgets(str, MAX_LIMIT, stdin);
    
    // Allocate dynamic memory for the message
    // TODO: Create dynamic memory for the message using malloc()
    
    
    // Connect to the chat server using shared memory
    // TODO: Connect to the chat server using shared memory
    
    // Use mutexes to ensure no other process is accessing or updating that memory
    
    // After the message has been successfully sent, free the allocated memory
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);
    return 0;
}

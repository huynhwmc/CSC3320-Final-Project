#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

#define SHM_SIZE 1024
#define RESPONSE_TIMEOUT 20 // timeout in seconds for server

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
    int shmid;            // Shared memory's ID
    void *shared_memory;

    // Generate unique key
    key_t key = ftok("shmfile", 65);
    shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("Shared memory error: shmget");
        return 1;
    }

    // Create shared memory and cast to the shared_data_t type
    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("Shared memory error: shmat");
        return 1;
    }
    shared_data_t *sh_data = (shared_data_t *)shared_memory;

    // Allocate dynamic memory for the message
    strings = (char *)malloc(SHM_SIZE);
    if (!strings)
    {
        perror("Memory allocation error: malloc");
        exit(1);
    }

    // User enters a message
    printf("\nWelcome. Your PID is %d. Enter a message: ", getpid());
    fgets(strings, SHM_SIZE, stdin);
    strings[strcspn(strings, "\n")] = '\0'; // Remove newline
    // Check that the message is not longer than the maximum allowed character limit
    if (strlen(strings) >= SHM_SIZE)
    {
        fprintf(stderr, "The message is too long. Maximum allowed size is %d characters.\n", SHM_SIZE - 1);
        free(strings);
        exit(1);
    }

    // Write to shared memory
    sh_data->flag = 1;              // Indicates that the message is from the client
    sh_data->client_pid = getpid(); // Store the PID
    
    strncpy(sh_data->message, strings, SHM_SIZE - sizeof(int) - sizeof(pid_t));
    printf("Sending message: %s\n", strings);

    // Wait for the server response
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < RESPONSE_TIMEOUT) {
        // Consistently check shared memory for updates by the server
        shared_data_t *sh_data = (shared_data_t *)shared_memory;
        if (sh_data->flag == 2) {
            printf("Server response: %s\n", sh_data->message);
            sh_data->flag = 0; // Mark shared memory as empty
            break;
        }
        usleep(100000); // Sleep for 100ms to avoid busy-waiting
    }

    if (sh_data->flag != 0) { // If no response is received within the timeout period
        printf("No response from server within timeout period.\n");
    }

    // Free allocated memory
    free(strings);

    // Detach from shared memory
    shmdt(shared_memory);
    return 0;
}

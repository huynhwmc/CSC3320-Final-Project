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

// Function declarations
void allocate_message_buffer(char **buffer);
int create_shared_memory(key_t key);
void *attach_shared_memory(int shmid);
void write_to_shared_memory(shared_data_t *sh_data, const char *message);
void wait_for_response(shared_data_t *sh_data, void *shared_memory);
void cleanup(char *buffer, void *shared_memory);

/* Chat client */
int main(int argc, char *argv[])
{
    char *message_buffer = NULL; // Holds the message in dynamic memory
    int shmid;            // Shared memory's ID
    void *shared_memory;

    // Generate unique key and create shared memory
    key_t key = ftok("shmfile", 65);
    shmid = create_shared_memory(key);

    // Attach to shared memory
    shared_memory = attach_shared_memory(shmid);
    shared_data_t *sh_data = (shared_data_t *)shared_memory;

    // Allocate dynamic memory for the message
    allocate_message_buffer(&message_buffer);

    // User enters a message
    printf("\nWelcome. Your PID is %d. Enter a message: ", getpid());
    fgets(message_buffer, SHM_SIZE, stdin);
    message_buffer[strcspn(message_buffer, "\n")] = '\0'; // Remove newline
    // Check that the message is not longer than the maximum allowed character limit
    if (strlen(message_buffer) >= SHM_SIZE)
    {
        fprintf(stderr, "The message is too long. Maximum allowed size is %d characters.\n", SHM_SIZE - 1);
        free(message_buffer);
        exit(1);
    }

    // Write to shared memory
    write_to_shared_memory(sh_data, message_buffer);

    // Wait for server response
    wait_for_response(sh_data, shared_memory);

    // Cleanup, free allocated memory and detach from shared memory
    cleanup(message_buffer, shared_memory);
    return 0;
}

void allocate_message_buffer(char **buffer)
{
    *buffer = (char *)malloc(SHM_SIZE);
    if (!*buffer)
    {
        perror("Memory allocation error: malloc");
        exit(1);
    }
}

int create_shared_memory(key_t key)
{
    int shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("Shared memory error: shmget");
        exit(1);
    }
    return shmid;
}

void *attach_shared_memory(int shmid)
{
    void *shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("Shared memory error: shmat");
        exit(1);
    }
    return shared_memory;
}

void write_to_shared_memory(shared_data_t *sh_data, const char *msg)
{
    // Write to shared memory
    sh_data->flag = 1;              // Indicates that the message is from the client
    sh_data->client_pid = getpid(); // Store the PID

    strncpy(sh_data->message, msg, SHM_SIZE - sizeof(int) - sizeof(pid_t));
    printf("Sending message: %s\n", msg);
}

void wait_for_response(shared_data_t *sh_data, void *shared_memory)
{
    // Wait for the server response
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < RESPONSE_TIMEOUT)
    {
        // Consistently check shared memory for updates by the server
        shared_data_t *sh_data = (shared_data_t *)shared_memory;
        if (sh_data->flag == 2)
        {
            printf("Server response: %s\n", sh_data->message);
            sh_data->flag = 0; // Mark shared memory as empty
            break;
        }
        usleep(100000); // Sleep for 100ms to avoid busy-waiting
    }

    if (sh_data->flag != 0)
    { // If no response is received within the timeout period
        printf("No response from server within timeout period.\n");
    }
}

void cleanup(char *buffer, void *shared_memory)
{
    free(buffer);
    shmdt(shared_memory);
}
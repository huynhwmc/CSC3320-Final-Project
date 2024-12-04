#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>

#define SHM_SIZE 1024
#define MAX_LIMIT 200 // Maximum buffer/character limit for messages
pthread_mutex_t mutex;
pthread_cond_t full;
pthread_cond_t empty;

typedef struct {
    pid_t client_pid;         // Client Process ID
    char message[SHM_SIZE];   // Message content
} SharedMemory;

// function declarations
void initialize_sync_primitives();
int create_shared_memory(key_t key);
void *attach_shared_memory(int shmid);
void allocate_message_buffer(char **buffer);
void get_message(char message_buffer[]);
void write_to_shared_memory(void *shared_memory, char *buffer);
void cleanup(char *buffer, void *shared_memory, int shmid);



/* Chat client */
int main(int argc, char *argv[])
{
    char *message_buffer = NULL;
    int shmid;
    void *shared_memory;

    // initialize synchronization primitives
    initialize_sync_primitives();

    // generate unique key and create shared memory
    key_t key = ftok("chat_server.c", 65);
    shmid = create_shared_memory(key);

    // attach to a shared memory
    shared_memory = attach_shared_memory(shmid);

    // allocate dynamic memory for the message
    allocate_message_buffer(&message_buffer);

    // get user message
    get_message(message_buffer);

    // write message to shared memory
    write_to_shared_memory(shared_memory, message_buffer);

    // clean up
    cleanup(message_buffer, shared_memory, shmid);

    return 0;
}

void initialize_sync_primitives()
{
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);
}

int create_shared_memory(key_t key)
{
    int shmid = shmget(key, sizeof(SharedMemory), 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("shmget");
        exit(1);
    }
    return shmid;
}

void *attach_shared_memory(int shmid)
{
    void *shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("shmat");
        exit(1);
    }
    return shared_memory;
}

void allocate_message_buffer(char **buffer)
{
    *buffer = (char *)malloc(SHM_SIZE);
    if (!*buffer)
    {
        perror("malloc");
        exit(1);
    }
}

void get_message(char message_buffer[])
{
    printf("Enter message: ");
    fgets(message_buffer, MAX_LIMIT, stdin);
    message_buffer[strcspn(message_buffer, "\n")] = 0; // remove newline

    // check that the message is not longer than the maximum allowed character limit 
    if (strlen(message_buffer)>= MAX_LIMIT)
    {
        printf("Message is too long. Maximum character limit is %d\n", MAX_LIMIT - 1);
        exit(1);
    }
}

void write_to_shared_memory(void *shared_memory, char *buffer)
{
    pthread_mutex_lock(&mutex);

    SharedMemory *shm = (SharedMemory *)shared_memory;

    // wait if shared memory is not empty
    while (strlen(shm->message) != 0)
    {
        pthread_cond_wait(&empty, &mutex);
    }

    // write message to shared memory
    shm->client_pid = getpid(); 
    strncpy(shm->message, buffer, SHM_SIZE - 1);

    // signal to the server that a message is available
    pthread_cond_signal(&full);
    printf("Message sent to server: %s\n", buffer);

    pthread_mutex_unlock(&mutex);
}

void cleanup(char *buffer, void *shared_memory, int shmid)
{
    
    // free allocated memory
    free(buffer);

    //detach from shared memory
    shmdt(shared_memory);

    //destory synchronization 
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);
}
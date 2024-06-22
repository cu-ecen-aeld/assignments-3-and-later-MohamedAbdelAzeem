#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/queue.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define MAX_IP_LEN INET_ADDRSTRLEN

/************ Global variables ***************/
int Socketfd;
const char *Datafilename = "/var/tmp/aesdsocketdata";
struct addrinfo *servinfo = NULL; // will point to the results

int data_fd; // data file to save the data received

char client_ip[MAX_IP_LEN]; // to store information about the client's IP address.

char *buffer; // malloc Buffer to receive from the socket

// Define a mutex variable
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t Timer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t timer_thread;
bool ExittimerFlag = 0;

/* Flags for closing unclosed sockets and files */
typedef struct aesdsocket_flags
{
    bool ServerSocket_flag;
    bool ClientSocket_flag;
    bool data_file_flag;
    bool servinfo_flag;
    bool buffer_flag;
    bool logs_flag;
    bool deamon_mode_flag;
} aesdsocket_flags;

aesdsocket_flags flags = {false, false, false, false, false, false, false};

/************ Function Prototypes ************/
void cleanup_on_exit();
void *client_handler(void *arg);
void *timestamp_timer();
void HandleSignal(int signal);
int run_daemon(void);
int setup_server(const char *port);
void accept_connections(int Socketfd);
void join_all_threads();
void thread_ls_insert(pthread_t thread_id);
void delete_Completed_threads();
void thread_ls_free();
void thread_ls_print();

/*********************************************************************/
/******************** Linked List functions ****************************/
/*********************************************************************/

// Structure for a node in the linked list
typedef struct ThreadNode
{
    pthread_t thread_id;
    bool completeFlag;
    SLIST_ENTRY(ThreadNode)
    entries;
} ThreadNode;

// Define the list head
SLIST_HEAD(ThreadListHead, ThreadNode)
head = SLIST_HEAD_INITIALIZER(head);

int threadCount = 0;
// Function to insert a new node into the linked list
void thread_ls_insert(pthread_t thread_id)
{
    ThreadNode *new_node = (ThreadNode *)malloc(sizeof(ThreadNode));
    if (new_node == NULL)
    {
        syslog(LOG_ERR, "Thread Memory Allocation Failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    new_node->thread_id = thread_id;
    new_node->completeFlag = false;
    SLIST_INSERT_HEAD(&head, new_node, entries);
    threadCount++;
}

// Function to delete entries from the SLIST if `delete_flag` is 1
void delete_Completed_threads()
{
    ThreadNode *current = SLIST_FIRST(&head); // Start at the head
    ThreadNode *previous = NULL;              // To track the previous node

    while (current != NULL)
    {
        ThreadNode *next = SLIST_NEXT(current, entries); // Keep track of the next node

        if (current->completeFlag == 1)
        {
            pthread_join(current->thread_id, NULL); // Join the thread to clean up
            // Condition to delete the node
            if (previous == NULL)
            {
                // If the node is the first in the list
                SLIST_REMOVE_HEAD(&head, entries); // Remove the head
            }
            else
            {
                // If the node is somewhere in the middle or end
                SLIST_REMOVE(&head, current, ThreadNode, entries); // Remove by reference
            }

            // Free the memory of the deleted node
            free(current);
        }
        else
        {
            // Update previous only if not deleting
            previous = current;
        }

        // Move to the next node
        current = next;
    }
}

// Function to print the linked list
void thread_ls_print()
{
    ThreadNode *node;

    if (SLIST_EMPTY(&head))
    {
        syslog(LOG_INFO, "The list is empty.\n");
        return;
    }

    syslog(LOG_INFO, "****************************************");
    // Iterate through the list and print each node's information
    SLIST_FOREACH(node, &head, entries)
    {

        syslog(LOG_INFO, "Thread ID: %lu, Complete: %s\n",
               (unsigned long)node->thread_id,
               node->completeFlag ? "true" : "false");
    }
    syslog(LOG_INFO, "****************************************");
}

// Function to free the entire linked list
void thread_ls_free()
{
    ThreadNode *node, *temp;

    // Iterate through the list
    node = SLIST_FIRST(&head); // Start from the head
    while (node != NULL)
    {
        temp = SLIST_NEXT(node, entries); // Store reference to the next node
        free(node);                       // Free the current node
        node = temp;                      // Move to the next node
    }

    // Reset the list to an empty state
    SLIST_INIT(&head);
}

void join_all_threads()
{
    ThreadNode *current;

    // Iterate through the list of threads
    SLIST_FOREACH(current, &head, entries)
    {
        // Join the thread
        pthread_join(current->thread_id, NULL);
    }
}
/*********************************************************************/

/**************************Timer functions***************************/
// Function to add a timestamp to the data file
void add_timestamp()
{
    time_t current_time;
    struct tm *time_info;
    char time_str[128];

    time(&current_time); // Get current system time
    time_info = localtime(&current_time);

    // Format time according to RFC 2822 (year, month, day, hour, minute, second)
    strftime(time_str, sizeof(time_str), "timestamp:%a, %d %b %Y %H:%M:%S\n", time_info);

    // Lock mutex to ensure atomic write
    pthread_mutex_lock(&mutex);
    syslog(LOG_INFO, "Writing time stamp");
    // Open the data file in append mode
    int fd = open(Datafilename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
    {
        perror("Error opening data file");
        pthread_mutex_unlock(&mutex);
        cleanup_on_exit();
        return;
    }

    // Write the timestamp to the file
    write(fd, time_str, strlen(time_str));

    // Close the file
    close(fd);

    // Unlock mutex after writing
    pthread_mutex_unlock(&mutex);
}

// Timer thread to add timestamps every 10 seconds
void *timestamp_timer()
{
    while (true)
    {
        sleep(10);       // Wait for 10 seconds
        add_timestamp(); // Append timestamp to the data file
        pthread_mutex_lock(&Timer_mutex);
        if (ExittimerFlag)
        {
            pthread_mutex_unlock(&Timer_mutex);
            break; // Exit the loop to terminate the thread
        }
        pthread_mutex_unlock(&Timer_mutex);
    }
    return NULL;
}

/*****************************************************************/
void cleanup_on_exit()
{
    ExittimerFlag = 1;
    syslog(LOG_INFO, "Cleaning up resources");
    syslog(LOG_INFO, "flags: %d, %d, %d, %d, %d, %d", flags.ClientSocket_flag, flags.ServerSocket_flag, flags.buffer_flag, flags.data_file_flag, flags.deamon_mode_flag, ExittimerFlag);
    join_all_threads();

    thread_ls_free();

    if (flags.servinfo_flag)
    {
        freeaddrinfo(servinfo); // free the linked-list
    }
    if (flags.buffer_flag)
    {
        free(buffer);
    }
    if (flags.ServerSocket_flag)
    {
        if (close(Socketfd) == -1)
        {
            perror("Error closing server socket");
            syslog(LOG_ERR, "Error closing server fd");
            exit(1);
        }
        flags.ServerSocket_flag = 0;
    }
    if (flags.data_file_flag)
    {
        if (close(data_fd) == -1)
        {
            perror("Error closing file");
            syslog(LOG_ERR, "Error closing data file");
            exit(1);
        }
    }
    // Delete the file
    if (access(Datafilename, F_OK) != -1)
    {
        if (remove(Datafilename) != 0)
        {
            perror("Error deleting data file");
            exit(1);
        }
    }
    pthread_join(timer_thread, NULL);
    if (flags.logs_flag)
    {
        closelog();
    }
}

void HandleSignal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        syslog(LOG_INFO, "Caught signal, exiting");
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        cleanup_on_exit();
        exit(EXIT_SUCCESS);
    }
}

/**
 * @brief Forks the current process to create a daemon.
 *
 * This function forks the current process to create a daemon by detaching itself
 * from the terminal. It performs necessary setup like changing the file mode,
 * setting a new session, changing the current working directory, and closing
 * standard I/O file descriptors.
 *
 * @return Returns 0 on success and -1 on failure.
 */
int run_daemon(void)
{
    pid_t forked_pid = fork(); // Fork the current process

    // Check for fork failure
    if (forked_pid < 0)
    {
        syslog(LOG_ERR, "Failed to fork process");
        return -1;
    }

    // Exit the parent process.
    if (forked_pid > 0)
    {
        syslog(LOG_INFO, "Termination of parent process completed");
        exit(EXIT_SUCCESS); // Exit the parent process
    }

    // Create a new session ID
    pid_t session_id = setsid();

    // Check for session ID failure
    if (session_id < 0)
    {
        exit(EXIT_SUCCESS);
    }

    // Change the current working directory to root
    if (chdir("/") < 0)
    {
        exit(EXIT_FAILURE);
    }
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
}

/*************** Server Setup *******************/
int setup_server(const char *port)
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, port, &hints, &servinfo);
    if (status != 0)
    {
        syslog(LOG_ERR, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    flags.servinfo_flag = true;
    Socketfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (Socketfd == -1)
    {
        syslog(LOG_ERR, "Socket error: %s\n", strerror(errno));
        return -1;
    }
    flags.ServerSocket_flag = true;

    int reuse = 1;
    if (setsockopt(Socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        perror("setsockopt");
        return -1;
    }

    if (bind(Socketfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0)
    {
        perror("bind");
        syslog(LOG_ERR, "bind error: %s\n", strerror(errno));
        return -1;
    }

    freeaddrinfo(servinfo);
    flags.servinfo_flag = false;

    if (listen(Socketfd, 5) != 0)
    {
        perror("listen");
        syslog(LOG_ERR, "listen error: %s\n", strerror(errno));
        return -1;
    }

    return Socketfd;
}

/*************** Client Handling ****************/
void *client_handler(void *arg)
{

    pthread_t tid = pthread_self();
    syslog(LOG_INFO, "Thread : %ld started\n", tid);

    // Lock the mutex before writing to the file
    syslog(LOG_INFO, "mutex lock \n");

    pthread_mutex_lock(&mutex);

    int NewSocketfd = *((int *)arg);
    free(arg);
    // Create thread for each accepted connection
    buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE); // buffer used to read and write data to and from the client
    if (buffer == NULL)
    {
        syslog(LOG_ERR, "malloc error: %s\n", strerror(errno));
        cleanup_on_exit();
        exit(1);
    }
    else
    {
        syslog(LOG_INFO, "Successfully created buffer\n");
        flags.buffer_flag = true;
    }

    // opening a data file (DATA_FILE) in write-only mode with options to create the file if it doesn't exist and to append data to it.
    data_fd = open(Datafilename, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (data_fd == -1)
    {
        // syslog(LOG_ERR, "open error: %s\n", gai_strerror(status));
        cleanup_on_exit();
        syslog(LOG_INFO, "mutex unlock \n");
        pthread_mutex_unlock(&mutex); // Unlock the mutex before returning
        return NULL;
    }

    flags.data_file_flag = true;
    syslog(LOG_INFO, "Successfully opend data file fd = %d\n", data_fd);

    // Receive data packets
    int bytes_received = 0;
    while ((bytes_received = recv(NewSocketfd, buffer, BUFFER_SIZE, 0)) > 0)
    {
        write(data_fd, buffer, bytes_received);
        if (memchr(buffer, '\n', bytes_received) != NULL)
        {
            // New line character found
            break;
        }
    }

    if (bytes_received < 0)
    {
        syslog(LOG_ERR, "recv error: %s\n", strerror(errno));
        syslog(LOG_INFO, "mutex unlock \n");
        pthread_mutex_unlock(&mutex); // Unlock the mutex before returning
        return NULL;
    }

    // syslog(LOG_INFO, "Received data from %s: %s ( %d )", client_ip, buffer, bytes_received);

    int reposition_status = lseek(data_fd, 0, SEEK_SET); // Reposition the read/write offset of the file to the beginning
    if (reposition_status == -1)
    {
        perror("lseek");
        syslog(LOG_ERR, "lseek operation failed");
        cleanup_on_exit();
        syslog(LOG_INFO, "mutex unlock \n");
        pthread_mutex_unlock(&mutex); // Unlock the mutex before returning
        return NULL;
    }

    memset(buffer, 0, sizeof(char) * BUFFER_SIZE); // clear the buffer

    // reading data from the data file into the buffer in chunks of up to BUFFER_SIZE bytes using the read function
    int bytes_read = 0;

    while ((bytes_read = read(data_fd, buffer, sizeof(char) * BUFFER_SIZE)) > 0)
    {
        // syslog(LOG_INFO, "sending data back to  %s: %s, %d", client_ip, buffer, (int)bytes_received);
        int sendstatus = send(NewSocketfd, buffer, bytes_read, 0); // sending the data read from the file back to the client
        if (sendstatus == -1)
        {
            perror("send");
            syslog(LOG_INFO, "mutex unlock \n");
            pthread_mutex_unlock(&mutex); // Unlock the mutex before returning
            return NULL;
        }
    }

    free(buffer);
    flags.buffer_flag = false;

    if (close(NewSocketfd) == -1)
    {
        perror("client socket fd");
        syslog(LOG_ERR, "Error closing client fd");
        syslog(LOG_INFO, "mutex unlock \n");
        pthread_mutex_unlock(&mutex); // Unlock the mutex before returning
        return NULL;
    }
    flags.ClientSocket_flag = false;
    if (close(data_fd) == -1)
    {
        perror("data file");
        syslog(LOG_ERR, "Error closing data file");
        syslog(LOG_INFO, "mutex unlock \n");
        pthread_mutex_unlock(&mutex); // Unlock the mutex before returning
        return NULL;
    }
    flags.data_file_flag = false;
    syslog(LOG_INFO, "Closed connection from %s", client_ip);

    syslog(LOG_INFO, "Main mutex unlock \n");
    pthread_mutex_unlock(&mutex); // Unlock the mutex before returning

    ThreadNode *current;
    SLIST_FOREACH(current, &head, entries)
    {
        if (current->thread_id == tid)
        {
            current->completeFlag = true;
            break;
        }
    }

    syslog(LOG_INFO, "Thread : %ld Ended\n", tid);
    return NULL;
}

void accept_connections(int Socketfd)
{
    while (1)
    {
        delete_Completed_threads();

        struct sockaddr_in client_addr;
        unsigned int client_len = sizeof(client_addr);

        int NewSocketfd = accept(Socketfd, (struct sockaddr *)&client_addr, &client_len);
        if (NewSocketfd < 0)
        {
            syslog(LOG_ERR, "accept error: %s\n", strerror(errno));
            continue;
        }

        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, MAX_IP_LEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        int *thread_socket = malloc(sizeof(int));
        if (!thread_socket)
        {
            syslog(LOG_ERR, "Memory allocation failed");
            close(NewSocketfd);
            continue;
        }
        *thread_socket = NewSocketfd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_handler, thread_socket) != 0)
        {
            syslog(LOG_ERR, "pthread_create error: %s\n", strerror(errno));
            close(NewSocketfd);
            free(thread_socket);
            continue;
        }

        thread_ls_insert(tid);
        thread_ls_print();
    }
}

/************ Main Function ******************/
int main(int argc, char *argv[])
{
    openlog("Logs", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Start logging");
    flags.logs_flag = true;

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        syslog(LOG_INFO, "aesdsocket socket started");
        if (run_daemon() == -1)
        {
            cleanup_on_exit();
            exit(EXIT_FAILURE);
        }
    }

    Socketfd = setup_server("9000");
    if (Socketfd == -1)
    {
        cleanup_on_exit();
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&timer_thread, NULL, timestamp_timer, NULL) != 0)
    {
        perror("Error creating timer thread");
        cleanup_on_exit();
        exit(EXIT_FAILURE);
    }

    accept_connections(Socketfd);

    closelog();
    return 0;
}
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024
#define MAX_IP_LEN INET_ADDRSTRLEN

/************ Global variables ***************/
int Socketfd;
const char *Datafilename = "/var/tmp/aesdsocketdata";
struct addrinfo *servinfo; // will point to the results

int NewSocketfd;
int data_fd; // data file to save the data received

char client_ip[MAX_IP_LEN]; // to store information about the client's IP address.

char *buffer; // malloc Buffer to receive from the socket

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

/*********************************************************************/
void cleanup_on_exit()
{
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
    }
    if (flags.ClientSocket_flag)
    {
        if (close(NewSocketfd) == -1)
        {
            perror("Error closing client socket");
            syslog(LOG_ERR, "Error closing client fd");
            exit(1);
        }
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
    if (remove(Datafilename) != 0)
    {
        perror("Error deleting file");
        exit(1);
    }
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
    }
    exit(EXIT_SUCCESS);
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
        exit(1);
    }

    // Change the current working directory to root
    chdir("/");

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
}

int main(int argc, char *argv[])
{

    // open connection for system logging
    openlog("Logs", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Start logging");
    flags.logs_flag = true;

    // Register signal handlers for SIGINT and SIGTERM
    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    if (argc > 1 && strcmp(argv[1], "-d") == 0)
    {
        syslog(LOG_INFO, "aesdsocket socket started");
        flags.deamon_mode_flag = true;
    }
    else
    {
        syslog(LOG_ERR, "-d isn't passed");
    }

    // Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
    int status;
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets, SOCK_DGRAM for UDP
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0)
    {
        syslog(LOG_ERR, "getaddrinfo error: %s\n", gai_strerror(status));
        cleanup_on_exit();
        exit(1);
    }

    flags.servinfo_flag = true; // set  flags.servinfo_flag = true; to indicate that it need to be freed

    // servinfo now points to a linked list of 1 or more struct addrinfos

    // create a socket
    /*
         AF_INET      IPv4 Internet protocols
         SOCK_DGRAM   UPD
         SOCK_STREAM  TCP
    */
    Socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (Socketfd == -1)
    {
        syslog(LOG_ERR, "Socket error: %s\n", gai_strerror(status));
        cleanup_on_exit();
        exit(1);
    }
    syslog(LOG_INFO, "Server Socket created Socketfd = %d", Socketfd);
    flags.ServerSocket_flag = true;

    // Set SO_REUSEADDR option
    int reuse = 1;
    if (setsockopt(Socketfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        perror("setsockopt");
        cleanup_on_exit();
        exit(EXIT_FAILURE);
    }

    // bind to the socket
    if (bind(Socketfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0)
    {
        perror("bind");
        syslog(LOG_ERR, "bind error: %s\n", gai_strerror(status));
        cleanup_on_exit();
        exit(1);
    }

    syslog(LOG_INFO, "Bind to the server socket.");

    freeaddrinfo(servinfo); // free the linked-list
    flags.servinfo_flag = false;

    if (flags.deamon_mode_flag)
    {
        int status = run_daemon();
        if (status == -1)
        {
            syslog(LOG_ERR, "Run_daemon error");
            cleanup_on_exit();
            exit(1);
        }
        syslog(LOG_DEBUG, "Daemon is successfully created ");
    }

    // listen to a connection
    if (listen(Socketfd, 5) != 0)
    {
        perror("listen");
        syslog(LOG_ERR, "listen error: %s\n", gai_strerror(status));
        cleanup_on_exit();
        exit(1);
    }
    syslog(LOG_INFO, "Listen on the server socket ");

    while (1)
    {
        //  accept the connection
        struct sockaddr_in client_addr;
        unsigned int client_len = sizeof(client_addr);
        syslog(LOG_INFO, "Wating for new connection...");

        int NewSocketfd = accept(Socketfd, (struct sockaddr *)&client_addr, &client_len);
        if (NewSocketfd < 0)
        {
            syslog(LOG_ERR, "accept error: %s\n", gai_strerror(status));
            continue;
        }
        flags.ClientSocket_flag = true;
        // extracting the client's IP address from the client_addr structure and converting it from binary form to a string using inet_ntop.
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, MAX_IP_LEN);

        // Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client.
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE); // buffer used to read and write data to and from the client
        if (buffer == NULL)
        {
            syslog(LOG_ERR, "malloc error: %s\n", gai_strerror(status));
            cleanup_on_exit();
            exit(1);
        }
        else
        {
            syslog(LOG_INFO, "Successfully created buffer\n");
            flags.buffer_flag = true;
        }

        // opening a data file (DATA_FILE) in write-only mode with options to create the file if it doesn't exist and to append data to it.
        data_fd = open(Datafilename, O_RDWR | O_CREAT | O_APPEND, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH);
        if (data_fd == -1)
        {
            syslog(LOG_ERR, "open error: %s\n", gai_strerror(status));
            cleanup_on_exit();
            exit(1);
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
            syslog(LOG_ERR, "recv error: %s\n", gai_strerror(status));
            continue;
        }

        // syslog(LOG_INFO, "Received data from %s: %s, %d", client_ip, buffer, (int)bytes_received);

        int reposition_status = lseek(data_fd, 0, SEEK_SET); // Reposition the read/write offset of the file to the beginning
        if (reposition_status == -1)
        {
            perror("lseek");
            syslog(LOG_ERR, "lseek operation failed");
            cleanup_on_exit();
            exit(1);
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
            }
        }

        syslog(LOG_INFO, "flags: %d, %d, %d, %d, %d", flags.ClientSocket_flag, flags.ServerSocket_flag, flags.buffer_flag, flags.data_file_flag, flags.deamon_mode_flag);

        free(buffer);
        flags.buffer_flag = false;
        if (close(NewSocketfd) == -1)
        {
            perror("client socket fd");
            syslog(LOG_ERR, "Error closing client fd");
            exit(1);
        }
        flags.ClientSocket_flag = false;
        if (close(data_fd) == -1)
        {
            perror("data file");
            syslog(LOG_ERR, "Error closing data file");
            exit(1);
        }
        flags.data_file_flag = false;
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    closelog();

    exit(0);
}

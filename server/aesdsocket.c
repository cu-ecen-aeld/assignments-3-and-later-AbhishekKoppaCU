#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <errno.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

int sockfd;
volatile sig_atomic_t stop = 0;  // Changed to `sig_atomic_t` for signal safety

void handle_signal(int sig)
{
    syslog(LOG_INFO, "Caught signal %d, exiting", sig);
    remove(FILE_PATH);

    if (sockfd != -1) 
    {
        close(sockfd);
    }

    closelog();
    exit(0);
}

void daemonize() 
{
    pid_t pid = fork();
    if (pid < 0) 
    {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) 
    {
        exit(EXIT_SUCCESS);
    }
    
    if (setsid() < 0) 
    {
        exit(EXIT_FAILURE);
    }
    
    signal(SIGHUP, SIG_IGN);
    pid = fork();
    
    if (pid < 0) 
    {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) 
    {
        exit(EXIT_SUCCESS);
    }

    umask(0);
    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
    int status, new_fd, file_fd;
    struct addrinfo hints, *servinfo;
    int daemonize_flag = 0;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];

	printf("Hi");
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (argc == 2 && strcmp(argv[1], "-d") == 0)
    {
        daemonize_flag = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0)
    {
        syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1)
    {
        syslog(LOG_ERR, "Failed to create socket");
        freeaddrinfo(servinfo);
        exit(EXIT_FAILURE);
    }
    
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) 
    {
        syslog(LOG_ERR, "Failed to set SO_REUSEADDR: %s", strerror(errno));
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)  // Removed semicolon
    {
        syslog(LOG_ERR, "Failed to bind socket to port %d", PORT);
        freeaddrinfo(servinfo);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(servinfo);

    if (daemonize_flag)
    {
        daemonize();
        syslog(LOG_INFO, "Daemonized the server");
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        syslog(LOG_ERR, "Failed to listen on socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Server started on port %d", PORT);

    while (!stop)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Failed to accept connection");
            continue;
        }

        struct sockaddr_in *addr = (struct sockaddr_in *)&their_addr;
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr->sin_addr));

        file_fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (file_fd == -1)
        {
            syslog(LOG_ERR, "Failed to open file");
            close(new_fd);
            continue;
        }

                ssize_t bytes_received;
        while ((bytes_received = recv(new_fd, buffer, sizeof(buffer) - 1, 0)) > 0)
        {
            buffer[bytes_received] = '\0';
            if (write(file_fd, buffer, bytes_received) == -1)
            {
                syslog(LOG_ERR, "Failed to write to file");
                break;
            }

            // Ensure all writes are flushed to disk
            fsync(file_fd);

            if (strchr(buffer, '\n')) // Full line received, now send file contents
{
    // Close write mode after receiving a line
    close(file_fd);  
    file_fd = open(FILE_PATH, O_RDONLY);  // Reopen in read mode

    if (file_fd == -1)
    {
        syslog(LOG_ERR, "Failed to open file for reading");
        break;
    }

    ssize_t bytes_written;
    while ((bytes_written = read(file_fd, buffer, sizeof(buffer))) > 0)
    {
        if (send(new_fd, buffer, bytes_written, 0) == -1)
        {
            syslog(LOG_ERR, "Failed to send data to client");
            break;
        }
    }

    close(file_fd);  // Close after reading
    
    // Reopen the file in append mode for the next write
    file_fd = open(FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, 0666);  // Ensure appending data
    if (file_fd == -1)
    {
        syslog(LOG_ERR, "Failed to reopen file for writing");
        break;
    }
}
        }


        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(addr->sin_addr));
        close(new_fd);
    }

    close(sockfd);
    remove(FILE_PATH);
    closelog();
    return 0;
}

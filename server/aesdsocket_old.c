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

#define PORT 9000
#define BACKLOG 10
#define FILE_PATH "/var/tmp/aesdsocketdata"

int sockfd;

void handle_signal(int sig)
{
    syslog(LOG_INFO, "Caught signal %d, exiting", sig);

    // Cleanup
    close(sockfd);
    remove(FILE_PATH);

    closelog();
    exit(0);
}

void daemonize()
{
    pid_t pid, sid;

    // Fork the process
    pid = fork();
    if (pid < 0)
    {
        syslog(LOG_ERR, "Failed to fork process");
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }

    // Create a new session and detach from the terminal
    sid = setsid();
    if (sid < 0)
    {
        syslog(LOG_ERR, "Failed to create new session");
        exit(EXIT_FAILURE);
    }

    // Change the working directory to root
    if (chdir("/") < 0)
    {
        syslog(LOG_ERR, "Failed to change working directory to /");
        exit(EXIT_FAILURE);
    }

    // Close all open file descriptors
    for (int fd = 0; fd < sysconf(_SC_OPEN_MAX); fd++)
    {
        close(fd);
    }

    // Redirect standard input, output, and error to /dev/null
    open("/dev/null", O_RDWR);  // stdin
    open("/dev/null", O_RDWR);  // stdout
    open("/dev/null", O_RDWR);  // stderr
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024];
    int new_fd, file_fd;
    ssize_t bytes_received, bytes_written;
    int daemonize_flag = 0;

    // Parse command line arguments
    if (argc == 2 && strcmp(argv[1], "-d") == 0)
    {
        daemonize_flag = 1;
    }

    // Open syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Register signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        syslog(LOG_ERR, "Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Setup address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        syslog(LOG_ERR, "Failed to bind socket to port %d", PORT);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // If daemon mode is requested, daemonize the process
    if (daemonize_flag)
    {
        daemonize();
        syslog(LOG_INFO, "Daemonized the server");
    }

    // Listen for connections
    if (listen(sockfd, BACKLOG) == -1)
    {
        syslog(LOG_ERR, "Failed to listen on socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Server started on port %d", PORT);

    // Accept connections in a loop
    while (1)
    {
        new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Failed to accept connection");
            continue;
        }

        // Log accepted connection
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        // Open file for appending
        file_fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (file_fd == -1)
        {
            syslog(LOG_ERR, "Failed to open file");
            close(new_fd);
            continue;
        }

        // Receive data and write to file
        while ((bytes_received = recv(new_fd, buffer, sizeof(buffer) - 1, 0)) > 0)
        {
            buffer[bytes_received] = '\0';  // Null-terminate the received data
            write(file_fd, buffer, bytes_received);

            // If newline is found, send the whole file to the client
            if (strchr(buffer, '\n'))
            {
                close(file_fd);

                // Reopen the file to send its contents
                file_fd = open(FILE_PATH, O_RDONLY);
                if (file_fd == -1)
                {
                    syslog(LOG_ERR, "Failed to reopen file for reading");
                    close(new_fd);
                    continue;
                }

                // Read and send the file content
                while ((bytes_written = read(file_fd, buffer, sizeof(buffer))) > 0)
                {
                    send(new_fd, buffer, bytes_written, 0);
                }
                close(file_fd);
                break;  // Break out of the loop after sending the file content
            }
        }

        // Close connection
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
        close(new_fd);
    }

    // Cleanup (unreachable due to infinite loop, but useful for clarity)
    close(sockfd);
    closelog();
    return 0;
}

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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define PORT 9000
#define BACKLOG 10
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

// SLIST.
typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    pthread_t threadID;  
    int socket_fd;
    bool status;
    int new_fd;
    struct sockaddr_in *socket_address;
    SLIST_ENTRY(slist_data_s) entries;
};
SLIST_HEAD(slisthead, slist_data_s) head = SLIST_HEAD_INITIALIZER(head);


int sockfd;
volatile sig_atomic_t stop = 0;  // Changed to `sig_atomic_t` for signal safety
pthread_t timestamp_thread;

#define MAX_THREADS 100
pthread_t threads[MAX_THREADS];  // Store active thread IDs
int thread_count = 0;
pthread_mutex_t aesdsocket_mutex = PTHREAD_MUTEX_INITIALIZER;

struct thread_data
{
    int new_fd;
    struct sockaddr_in *socket_address;
};


void handle_signal(int sig)
{
    syslog(LOG_INFO, "Caught signal %d, exiting", sig);
    stop = 1;
    remove(FILE_PATH);

    if (sockfd != -1) 
    {
        close(sockfd);
    }

    
    // for (int i = 0; i < thread_count; i++) 
    // {
    //     pthread_join(threads[i], NULL);
    // }
    pthread_join(timestamp_thread, NULL);
    pthread_mutex_destroy(&aesdsocket_mutex);  // Destroy mutex
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

void* timestamp_threadfunc(void* arg) 
{
    while (!stop) 
    {  
        sleep(10);  

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[100];
        strftime(timestamp, sizeof(timestamp), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", tm_info);

        pthread_mutex_lock(&aesdsocket_mutex);  
        int file_fd = open(FILE_PATH, O_WRONLY | O_APPEND | O_CREAT, 0666);
        if (file_fd != -1) 
        {
            write(file_fd, timestamp, strlen(timestamp));
            close(file_fd);
        } 
        else 
        {
            syslog(LOG_ERR, "Failed to write timestamp");
        }
        pthread_mutex_unlock(&aesdsocket_mutex);
    }
    return NULL;
}

void* threadfunc(void* thread_param)
{
    char buffer[BUFFER_SIZE];
    slist_data_t *thread_struct = (slist_data_t *)thread_param;

    struct sockaddr_in *addr = thread_struct->socket_address;
    int new_fd = thread_struct->new_fd;
    int file_fd;
    ssize_t bytes_received;
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr->sin_addr));

        // Lock mutex before writing to the file
        pthread_mutex_lock(&aesdsocket_mutex);

        file_fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (file_fd == -1)
        {
            syslog(LOG_ERR, "Failed to open file");
            close(new_fd);
            pthread_mutex_unlock(&aesdsocket_mutex);
            return NULL;
        }

        
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

        // Unlock the mutex after writing and reading
        pthread_mutex_unlock(&aesdsocket_mutex);


        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(addr->sin_addr));
        close(new_fd);
        thread_struct->status = true;

        //free(thread_param);
        return NULL;
    
}

int main(int argc, char *argv[])
{
    int status, new_fd, file_fd;
    struct addrinfo hints, *servinfo;
    int daemonize_flag = 0;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    
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

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)  
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
    
    
    pthread_create(&timestamp_thread, NULL, timestamp_threadfunc, NULL);

    while (!stop)
    {
        addr_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_fd == -1)
        {
            syslog(LOG_ERR, "Failed to accept connection");
            continue;
        }
        //struct thread_data *thread_struct = (struct thread_data*)malloc(sizeof(struct thread_data));
        slist_data_t *thread_struct=(slist_data_t *)malloc(sizeof(slist_data_t));
        thread_struct->new_fd = new_fd;
        thread_struct->socket_address = (struct sockaddr_in *)&their_addr;
        thread_struct->status = false;
        
        int ret = pthread_create(&thread_struct->threadID, NULL, threadfunc, (void *)thread_struct);
        if (ret) 
        {
            errno = ret;
            perror("pthread_create");
            free(thread_struct);
            continue;
        }
            
        SLIST_INSERT_HEAD(&head, thread_struct, entries);
        
        slist_data_t *node = SLIST_FIRST(&head);
        slist_data_t *temp = NULL; 
	//SLIST_FOREACH(node, &head, entries); 
	{ 
    	    //if (node->status) 
    	    {  // Thread has finished execution
        	//pthread_join(node->threadID, NULL);
        	//SLIST_REMOVE(&head, node, slist_data_s, entries);
        	//free(node);  // Free memory here
            }
	}
	
	while(node)	
	{
	    temp = SLIST_NEXT(node, entries);
	    
	    if(node->status)
	    {
	        pthread_join(node->threadID, NULL);
	        SLIST_REMOVE(&head, node, slist_data_s, entries);
	        free(node);
	    }
	    node = temp;  
	    
	}
        
        
        
        
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        
    }

    close(sockfd);
    remove(FILE_PATH);
    closelog();
    return 0;
}

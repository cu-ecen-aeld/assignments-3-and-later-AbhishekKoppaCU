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
//#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1  
#endif

#if USE_AESD_CHAR_DEVICE
#define FILE_PATH "/dev/aesdchar"
#else
#define FILE_PATH "/var/tmp/aesdsocketdata"
#endif

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
    //#ifndef USE_AESD_CHAR_DEVICE
        remove(FILE_PATH); // Remove only if using a regular file
    //#endif

    if (sockfd != -1) 
    {
        close(sockfd);
    }

    #ifndef USE_AESD_CHAR_DEVICE
    pthread_join(timestamp_thread, NULL);
    #endif
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

#ifndef USE_AESD_CHAR_DEVICE
void* timestamp_threadfunc(void* arg) 
{
    #ifdef USE_AESD_CHAR_DEVICE
    return NULL; // No timestamps needed when using the char device
    #endif
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
#endif

void* threadfunc(void* thread_param)
{
    slist_data_t *thread_struct = (slist_data_t *)thread_param;
    struct sockaddr_in *addr = thread_struct->socket_address;
    int new_fd = thread_struct->new_fd;
    int file_fd;

    syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr->sin_addr));

    // Buffer for receiving data dynamically
    size_t buffer_size = 1024; // Initial buffer size
    char *buffer = (char *)malloc(buffer_size);
    if (!buffer) {
        syslog(LOG_ERR, "Memory allocation failed");
        close(new_fd);
        return NULL;
    }

    size_t total_received = 0;
    ssize_t bytes_received;

    pthread_mutex_lock(&aesdsocket_mutex);
    

    while ((bytes_received = recv(new_fd, buffer + total_received, buffer_size - total_received - 1, 0)) > 0) 
    {
        total_received += bytes_received;
        buffer[total_received] = '\0';

        // If buffer is full, resize it
        if (total_received >= buffer_size - 1) {
            buffer_size *= 2; // Double the buffer size
            char *new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                syslog(LOG_ERR, "Memory reallocation failed");
                break;
            }
            buffer = new_buffer;
        }

        
        if (strchr(buffer, '\n')) {
            break;
        }
    }
    
    if(total_received > 0)
    {
    
    
    
    //#ifdef USE_AESD_CHAR_DEVICE
        //file_fd = open("/dev/aesdchar", O_WRONLY);
    //#else
        file_fd = open(FILE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0666);
    //#endif

    if (file_fd == -1) {
        syslog(LOG_ERR, "Failed to open file");
        close(new_fd);
        free(buffer);
        pthread_mutex_unlock(&aesdsocket_mutex);
        return NULL;
    }

    // Write the full received message to file
    if (write(file_fd, buffer, total_received) == -1) {
        syslog(LOG_ERR, "Failed to write to file");
    }

    fsync(file_fd);
    close(file_fd);
    pthread_mutex_unlock(&aesdsocket_mutex);
    }

    if(total_received > 0)
    {
    // Send back the full file content
    pthread_mutex_lock(&aesdsocket_mutex);
    #ifdef USE_AESD_CHAR_DEVICE
        file_fd = open("/dev/aesdchar", O_RDONLY);
    #else
        file_fd = open(FILE_PATH, O_RDONLY);
    #endif
    if (file_fd != -1) {
        while ((bytes_received = read(file_fd, buffer, buffer_size)) > 0) {
            send(new_fd, buffer, bytes_received, 0);
        }
        close(file_fd);
    }
    pthread_mutex_unlock(&aesdsocket_mutex);
    }

    syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(addr->sin_addr));
    close(new_fd);
    free(buffer);
    thread_struct->status = true;
    return NULL;
}


int main(int argc, char *argv[])
{
    int status, new_fd;
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
    
    
    #ifdef USE_AESD_CHAR_DEVICE
        syslog(LOG_INFO, "Using character device /dev/aesdchar");
    #else
        pthread_create(&timestamp_thread, NULL, timestamp_threadfunc, NULL);
    #endif

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

#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>   
#include <errno.h>   
#include <string.h>  

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <file path> <text string>\n", argv[0]);
        return 1;
    }
    char* writefile = argv[1];
    char* writestr = argv[2];
    int len = strlen(writestr);

    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) 
    {
        openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_ERR, "Error opening file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    ssize_t written = write(fd, writestr, len);
    if (written == -1) 
    {
        openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_ERR, "Error writing to file %s: %s", writefile, strerror(errno));
        closelog();
        close(fd);
        return 1;
    }

    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_DEBUG, "Writing \"%s\" to %s", writestr, writefile);
    closelog();

    close(fd);

    return 0;

}
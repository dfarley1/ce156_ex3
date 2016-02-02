/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Server code.  Run as "./myserver <port number>"
 */

#include "myunp.h"

int sendFileSize(int connfd, char *filename);

int main(int argc, char **argv)
{
    int listenfd, connfd, n;
    unsigned int port_num;
    char recvline[MAXLINE], output[MAXLINE];
    struct sockaddr_in servaddr;
    
    //Check arguments
    if (argc != 2) {
        printf("Usage: ./myserver <port number>\n\n");
        return 1;
    }
    port_num = strtoul(argv[1], NULL, 10);
    
    //Set up the socket
    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_num);
    
    Bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
    printf("  Server started on %s:%u...\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);
    
    Listen(listenfd, LISTENQ);
    
    for (;;) {
        bzero(recvline, MAXLINE);
        bzero(output, MAXLINE);
        
        //Wait for connection from client
        connfd = Accept(listenfd, NULL, NULL);
        if (connfd >= 0) {
            //Read data from client
            
            while (strstr(recvline, "\n\n") == NULL) {
                Read(connfd, recvline+strlen(recvline), MAXLINE-strlen(recvline)-1);
            }
            printf("recieved \"%s\"\n", recvline);
        } else {}
        
        char filename[MAXLINE];
        int start = 0, size = 0;
        bzero(filename, MAXLINE);
        
        //No idea why I can't do both at the same time, stupid sscanf...
        if (sscanf(recvline, "%[^\n]s", filename) != 1) {
            printf("ERROR: sscanf(\"%s\") failed!  Closing connection...\n", filename);
            Close(connfd);
            continue;
        }
        if (sscanf((recvline+strlen(filename)), "%d\n%d", &start, &size) != 2) {
            printf("ERROR: sscanf(%d   %d) failed!  Closing connection...\n", start, size);
            Close(connfd);
            continue;
        }
        
        printf("\"%s\", %d, %d\n", filename, start, size);
        
        if (start == -1 && size == -1) {
            sendFileSize(connfd, filename);
        } else {
            //serve part of file
        }
        
        
    }
    
    return 0;
}

int sendFileSize(int connfd, char *filename)
{
    char output[MAXLINE];
    struct stat st;
    bzero(output, MAXLINE);
    
    if (stat(filename, &st) != 0) {
        printf("sendFileSize(): stat() ERROR. Sending \"%s\" to client", strerror(errno));
        strcpy(output, strerror(errno));
        Write(connfd, output, strlen(output));
        Close(connfd);
        return -1;
    } else {
        sprintf(output, "%d", st.st_size);
        printf("Sending \"%s\" to client...", output);
        Write(connfd, output, strlen(output));
        Close(connfd);
        return 0;
    }
    
    return 0;
}

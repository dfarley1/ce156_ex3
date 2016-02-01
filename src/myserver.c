/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Server code.  Run as "./myserver <port number>"
 */

#include "myunp.h"

int main(int argc, char **argv)
{
    int listenfd, connfd, n;
    unsigned int port_num;
    char filename[MAXLINE], output[MAXLINE];
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
        bzero(filename, MAXLINE);
        bzero(output, MAXLINE);
        
        //Wait for connection from client
        connfd = Accept(listenfd, NULL, NULL);
        if (connfd >= 0) {
            //Read data from client
            
            while (strstr(filename, "\n\n") == NULL) {
                Read(connfd, filename+strlen(filename), MAXLINE-strlen(filename)-1);
            }
            printf("recieved \"%s\"\n", filename);
        }
        //if -1,-1 -> send size
        //else -> send data
        
    }
    
    return 0;
}

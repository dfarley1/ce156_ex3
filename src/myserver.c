/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Server code.  Run as "./myserver <port number>"
 */

#include "myunp.h"

int sendFileSize(int connfd, char *filename);
int sendFileChunk(int connfd, char *filename, int start, int size);

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
            sendFileChunk(connfd, filename, start, size);
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

int sendFileChunk(int connfd, char *filename, int start, int size)
{
    int retval = 0;
    char *output;
    struct stat st;
    bzero(output, size);
    
    if (stat(filename, &st) != 0) {
        //-1 header specifies error, data portion will contain strerror
        output = calloc(8 + strlen(strerror(errno)) + 1, sizeof(char));
        strcpy(output, "-1\n-1\n\n");
        strcpy(output+strlen(output), strerror(errno));
        
        printf("sendFileChunk(): stat() ERROR. Sending \"%s\" to client", output);
        retval = -1;
    } else {
        //if sizes don't match, send error
        if (st.st_size != size) {
            asprintf(&output, "-1\n-1\n\nFile sizes do not match!  reported size=%d\n  found size=%d", 
                    size, st.st_size);
            
            printf("sendFileChunk(): ERROR.  Sending \"%s\" to client", output);
            retval = -2;
        //else if the start point is beyond the start of the file, send error
        } else if (start >= size) {
            asprintf(&output, "-1\n-1\n\nStart position larger than file size!  start=%d\n  size=%d", 
                    start, size);
            
            printf("sendFileChunk(): ERROR.  Sending \"%s\" to client", output);
            retval = -3;
        //otherwise, time to open the file
        } else {
            asprintf(&output, "%d\n%d\n\n", start, size);
            output = realloc(output, strlen(output) + size + 1);
            bzero(output + strlen(output), size + 1);
            
            //put file chunk into output+strlen(output)
            FILE *fp = fopen("filename", "r");
            if (fp) {
                fseek(fp, start, SEEK_SET);
                int outsize = 0;
                do {
                    output[outsize++] = fgetc(fp);
                    if (feof(fp)) break;
                } while (1);
                fclose(fp);
            } else {
                output = realloc(output, 8 + strlen(strerror(errno)) + 1);
                bzero(output, 8 + strlen(strerror(errno)) + 1);
                strcpy(output, "-1\n-1\n\n");
                strcpy(output+strlen(output), strerror(errno));
                
                printf("sendFileChunk(): fopen() ERROR. Sending \"%s\" to client", output);
                retval = -4;
            }
        }
    }
    
    Write(connfd, output, strlen(output));
    Close(connfd);
    return retval;
}

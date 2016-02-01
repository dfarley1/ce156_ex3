/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Client.  Run as "./myclient <server-info.text> <num connections>"
 */

#include "myunp.h"

typedef struct {
    char **IPs;
    unsigned int *ports;
    int num;
    int linesAlloced;
} servers_t;

servers_t *servers;

void readServerList(FILE *serversFile);
void freeServerList();
int getFileSize();

int main(int argc, char **argv)
{
    int numConns = 0, fileSize;
    char filename[MAXLINE];// **servers, **server_ips, **server_ports, 
    FILE *serversFile;
    
    //arg checking
    //printf("checking args...\n");
    if (argc != 3) {
        printf("  Usage: myclient <server-info.text> <num connections>\n\n");
        exit(1);
    }
    
    if (sscanf(argv[2], "%d", &numConns) != 1) {
        printf("  Error: Invalid num connections\n\n");
        exit(1);
    }
    
    //printf("trying to fopen(%s)...\n", argv[1]);
    if ((serversFile = fopen(argv[1], "r")) == NULL) {
        printf("  Error opening server-info.text: %d: %s\n\n", errno, strerror(errno));
        exit(1);
    } 
    
    //Get the list of servers
    //printf("starting read....\n");
    readServerList(serversFile);
    fclose(serversFile);
    
    servers->num = (servers->num > numConns)?(numConns):(servers->num);
    
    printf("  num=%d\n", servers->num);
    int i;
    for (i = 0; i < servers->num; i++) {
        printf("  %s:%d\n", servers->IPs[i], servers->ports[i]);
    }
    
    fileSize = getFileSize();
    
    /*
    //try to connect to a server
    int i;
    for (i = 0; i < numConns; i++) {
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        
        //get port_num from servers list
        char *port_num_str = strchr(servers[i], ' ');
        port_num = strtoul(port_num_str, NULL, 10);
        servaddr.sin_port = htons(port_num);
        
        //Need to put IP address in its own string because
        // inet_pton() can't ignore the port
        printf("%d    ", port_num_str - servers[i]);
        char *ip_str = calloc(16, sizeof(char));
        strncpy(ip_str, servers[i], port_num_str - servers[i]);
        printf("\"%s\"\n", ip_str);
        if (inet_pton(AF_INET, ip_str, &servaddr.sin_addr) <= 0) {
            err_quit("inet_pton error for %s\n", ip_str);
        }
        free(ip_str);
        
        //try to connect, if not then try next server, if so 
        if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
            err_sys("connect error");
        }
    }
    */
    
    
    freeServerList();
    return 0;
}

//Read server IPs/ports into string array
//Modified from https://stackoverflow.com/a/19174415
void readServerList(FILE *serversFile)
{
    int max_line_len = 30;
    
    servers = calloc(1, sizeof(servers_t));
    servers->linesAlloced = 4;
    servers->num = 0;
    
    servers->IPs = (char **)calloc(servers->linesAlloced, sizeof(char *));
    if (servers == NULL) {
        printf("  Error allocating memory\n\n");
        exit(1);
    }

    //First part: Read raw lines into strings (servers->IPs for now)
    int i;
    for (i = 0; 1; i++) {
        //printf("i=%d\n", i);
        //Do we need to realloc?
        if (i >= servers->linesAlloced) {
            //if so, double the size and 
            //printf("current=%d, ", servers->linesAlloced);
            servers->linesAlloced *= 2;
            //printf("new=%d|| size=%d\n", servers->linesAlloced, sizeof(char *) * (servers->linesAlloced));
            servers->IPs = (char **)realloc(servers->IPs, sizeof(char *) * (servers->linesAlloced));
            if (servers == NULL) {
                printf("  Error re-allocating memory\n\n");
                exit(1);
            }
            int j;
            for (j = (servers->linesAlloced)/2; j < (servers->linesAlloced); j++) {
                servers->IPs[j] = NULL;
            }
        }
        
        //Allocate space for next server
        servers->IPs[i] = (char *)calloc(max_line_len, sizeof(char));
        if(servers->IPs[i] == NULL) {
            printf("  Error allocating server line\n\n");
            exit(1);
        }
        
        //printf("getting line...\n");
        if ((fgets(servers->IPs[i], max_line_len, serversFile)) == NULL) {
            servers->num = i;
            break;
        }
        //printf("got line: \"%s\"...\n", servers->IPs[i]);
        
        //strip CR, LF characters and insert \0
        int j;
        for (j = strlen(servers->IPs[i])-1; j <= 0 && (servers->IPs[i][j] == '\n' || servers->IPs[i][j] == '\r');  j--)
            ;
        //printf("len = %d\n", j);
        servers->IPs[i][j] = '\0';
        
    }
    
    //Second part: read ports into servers->ports and trim them from servers->IPs
    servers->ports = (unsigned int *)calloc(servers->num, sizeof(unsigned int));
    for (i = 0; i < servers->num; i++) {
        char *port_num_str = strchr(servers->IPs[i], ' ');
        //printf("%d    ", port_num_str - servers->IPs[i]);
        servers->ports[i] = strtoul(port_num_str, NULL, 10);
        *port_num_str = '\0';
    }
}

void freeServerList()
{
    int i;
    for (i = 0; i < servers->linesAlloced; i++) {
        free(servers->IPs[i]);
    }
    free(servers->IPs);
    free(servers->ports);
    free(servers);
}

int getFileSize()
{
    int fileSize, sockfd, n;
    char filename[MAXLINE], recvline[MAXLINE];
    struct sockaddr_in servaddr;
    
    bzero(filename, MAXLINE);
    bzero(recvline, MAXLINE);
    
    printf("Enter file name: ");
    fgets(filename, MAXLINE/2, stdin);  //should be enough room, right?
    
    //Try to connect to a server
    int i = 0;
    while (1) {
        //If we've tried all servers then just crash
        if (i >= servers->num) {
            err_sys("getFileSize():  ERROR: No servers reachable, tried %d!\n", i);
        }
        
        bzero(&servaddr, sizeof(servaddr));
        sockfd = Socket(AF_INET, SOCK_STREAM, 0);
        
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(servers->ports[i]);
        if (inet_pton(AF_INET, servers->IPs[i], &servaddr.sin_addr) <= 0) {
            err_quit("getFileSize():  ERROR: inet_pton error for %s\n", servers->IPs[i]);
        }
        
        //If connection is successful then break, if not, i++ and try next one
        if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
            i++;
            continue;
        } else {
            break;
        }
    }
    printf("getFileSize():  Connected to %s:%d\n", servers->IPs[i], servers->ports[i]);
    strcpy((filename+strlen(filename)),"-1\n-1\n\n"); //append dummy <start> and <size> parameters
    
    printf("getFileSize():  sending \"%s\"\n", filename);
    Write(sockfd, filename, strlen(filename));
    
    while ((n = Read(sockfd, recvline, MAXLINE-1)) > 0) {
        recvline[n] = 0;
        printf("recieved \"%s\"\n", recvline);
        if (sscanf(recvline, "%d", &fileSize) != 1) {
            err_sys("getFileSize():  ERROR: sscanf() failed to get a fileSize\n");
        }
    }
    
    printf("getFileSize():  Returning %d\n\n", fileSize);
    return fileSize;
}
    
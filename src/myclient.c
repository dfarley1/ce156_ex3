/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Client.  Run as "./myclient <server-info.text> <num connections>"
 */

#include "myunp.h"
#include "globals.h"

typedef struct {
    char **IPs;
    unsigned int *ports;
    int num;
    int linesAlloced;
} servers_t;

typedef struct {
    char *filename;
    int start;
    int size;
    int thread_id;
} tInfo_t;


servers_t *servers;

void readServerList(char **argv);
void freeServerList();
packet_type *getFileSize(char *filename);
void downloadFile(char *filename, int fileSize);
void *childDownloader (void *threadInfo);
static void recvfrom_alarm(int);
packet_type *get_chunk(packet_type *p_request, int t_id);

int main(int argc, char **argv)
{
    char filename[FILENAME_LENGTH + 1];
    packet_type *p_size;
    
    //arg checking
    //printf("checking args...\n");
    if (argc != 3) {
        printf("  Usage: myclient <server-info.text> <num connections>\n\n");
        exit(1);
    }
    readServerList(argv);
    
    //get a size_reply packet containing the filename and size
    p_size = getFileSize(filename);
    
    printf("main: got fileSize=%d!\n", p_size->size);
    strcpy(filename, p_size->filename);
    downloadFile(filename, p_size->size);
    free(p_size);
    
    freeServerList();
    return 0;
}

//Read server IPs/ports into string array
//Modified from https://stackoverflow.com/a/19174415
void readServerList(char **argv)
{
    int max_line_len = 30, numConns = 0;
    FILE* serversFile;
    
    if (sscanf(argv[2], "%d", &numConns) != 1) {
        printf("  Error: Invalid num connections\n\n");
        exit(2);
    }
    
    //Get the list of servers
    if ((serversFile = fopen(argv[1], "r")) == NULL) {
        printf("  Error opening server-info.text: %d: %s\n\n", errno, strerror(errno));
        exit(3);
    }
    
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
    fclose(serversFile);
    
    servers->num = (servers->num > numConns)?(numConns):(servers->num);
    
    /*
    printf("  num=%d\n", servers->num);
    for (i = 0; i < servers->num; i++) {
        printf("  %s:%d\n", servers->IPs[i], servers->ports[i]);
    }
    */
}

packet_type *getFileSize(char *filename)
{
    int sockfd, n;
    socklen_t len;
    struct sockaddr_in servaddr;
    packet_type *request_packet, *reply_packet;
    
    //Set sigalarm action
    struct sigaction sact = {
        .sa_handler = recvfrom_alarm,
        .sa_flags = 0,
    };
    if (sigaction(SIGALRM, &sact, NULL) == -1) {
        err_sys("getFileSize(): sigaction error: %s", strerror(errno));
    }
    
    //Loop until a valid size_reply packet is received.
    do {
        //get filename from user
        bzero(filename, FILENAME_LENGTH);
        printf("Enter file name: ");
        fgets(filename, FILENAME_LENGTH, stdin);
        filename[strcspn(filename, "\n")] = 0;
        
        //Try every server
        int i;
        for (i = 0; i < servers->num * NUM_ATTEMPTS; i++) {
            //Set up socket and server IP address
            bzero(&servaddr, sizeof (struct sockaddr_in));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(servers->ports[i%servers->num]);
            if (inet_pton(AF_INET, servers->IPs[i%servers->num], &servaddr.sin_addr) <= 0) {
                err_quit("getFileSize():  ERROR: inet_pton error for %s\n", servers->IPs[i%servers->num]);
            }
            sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
            printf("  Trying server %s:%u\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);
            
            //create the request packet and allocate space for reply
            request_packet = new_packet(size_request, 0, 0, filename, NULL);
            reply_packet = new_blank_packet();
            
            //send the packet!
            Sendto(sockfd, request_packet, sizeof(packet_type), 0, (SA *) &servaddr, sizeof(servaddr));
            printf("  Data sent, wait for reply...\n");
            len = sizeof(servaddr);
            
            //wait up to TIMEOUT seconds for reply
            alarm(TIMEOUT_PERIOD);
            n = recvfrom(sockfd, reply_packet, sizeof(packet_type) + MAX_DATA_SIZE - 1, 
                         0, (SA *) &servaddr, &len);
            free(request_packet); //No need for that anymore
            if (n < 0) {
                if (errno == EINTR) { //This server timed out, let's try another
                    printf("  Server timeout. Trying next...\n");
                    free(reply_packet);
                    continue; 
                } else { //There was some other error
                    err_sys("getFileSize(): recvfrom error: ", strerror(errno));
                }
            } else { //got a packet!
                alarm(0);
                break;
            }
        }
        //We exit the for() either when we run out of servers or when we get a packet
        if (i >= servers->num) {//we ran out
            err_sys("getFileSize():  ERROR: No servers reachable, tried %d!\n", i);
            
        } else {//we got a packet
            if (reply_packet->opcode == size_reply) {
                printf("  getFileSize(): Received a valid size_reply packet!\n");
                
            } else {
                if (reply_packet->opcode == error) {
                    int j;
                    for (j = 0; j < reply_packet->size; j++) {
                        printf("%c", (&(reply_packet->data))[j]);
                    }
                    printf(".(%d)  ", reply_packet->size);
                } else {
                    printf("  getFileSize(): Received an invalid packet\n");
                }
                free(reply_packet);
                reply_packet = NULL;
            }
        }
    } while (!reply_packet || (reply_packet->opcode != size_reply));
    
    return reply_packet;
}

void downloadFile(char *filename, int fileSize)
{
    //spawn children
    int rc;
    pthread_t tid[servers->num];
    void *vptr_return[servers->num];
    tInfo_t *tInfo[servers->num];
    
    int i;
    for (i = 0; i < servers->num; i++) {
        tInfo[i] = (tInfo_t *) malloc(sizeof(tInfo_t));
        tInfo[i]->filename = filename;
        tInfo[i]->thread_id = i;
        
        //The last thread needs to pick up the slack if fileSize doesn't divide evenly
        if (i == (servers->num - 1)) { 
            tInfo[i]->start = i * (fileSize / servers->num);
            tInfo[i]->size = (fileSize / servers->num) 
                           + (fileSize - (fileSize / servers->num) * servers->num);
        } else {
            tInfo[i]->start = i * (fileSize / servers->num);
            tInfo[i]->size = fileSize / servers->num;
        }
        printf("Creating thread %d: \"%s\"|%d|%d\n", 
                tInfo[i]->thread_id, tInfo[i]->filename, tInfo[i]->start, tInfo[i]->size);
                
        rc = pthread_create(&tid[i], NULL, childDownloader, (void *) tInfo[i]);
        if (rc) {
            printf("downloadFile(): pthread_create() ERROR: %s\n", strerror(rc));
            exit(5);
        }
    }
    
    //get string pointers from children
    for (i = 0; i < servers->num; i++) {
        pthread_join(tid[i], &vptr_return[i]);
        //free(vptr_return[i]);
    }
    
    //print all children to file (how should it be named?)
    char *newFileName = calloc(strlen(filename) + 3, sizeof(char));
    strcpy(newFileName, filename);
    strcpy(newFileName+strlen(newFileName), "_d");
    
    FILE *fp = fopen(newFileName, "w+");
    if (fp) {
        int i;
        for (i = 0; i < servers->num; i++) {
            char *chunk = (char *)vptr_return[i];
            int j = 0;
            do {
                fputc(chunk[j++], fp);
                if (j >= tInfo[i]->size) break;
            } while (1);
        }
    } else {
        printf("downloadFile(): fopen(%s) ERROR", newFileName);
        exit(6);
    }
    fclose(fp);
    
    free(newFileName);
    for (i = 0; i < servers->num; i++) {
        free(vptr_return[i]);
        free(tInfo[i]);
    }
}

void *childDownloader (void *threadInfo)
{
    tInfo_t *tInfo = (tInfo_t *) threadInfo;
    int sockfd, 
        n,
        chunk_start = tInfo->start,
        chunk_size = MAX_DATA_SIZE;
    char *data_string = calloc(tInfo->size, 1);
    socklen_t len;
    struct sockaddr_in servaddr;
    packet_type *p_request, *p_reply;
    
    //Since we're not gettin the whole thing at once we need to count out the chunks and request them individually
    //At the end of the loop we'll increment chunk_start by the size of the data received
    while (chunk_start < tInfo->start + tInfo->size) {
        //if the chunk_size (default MAX) is larger than what's left in this chunk
        //  then reduce it to what's left.  Basically min(MAX, what's left in the file)
        if (chunk_size > tInfo->start + tInfo->size - chunk_start) {
            chunk_size = tInfo->start + tInfo->size - chunk_start;
        }
        
        p_request = new_packet(chunk_request, chunk_size, chunk_start, tInfo->filename, NULL);
        printf("childDownloader(%d): requesting chunk %d:%d of %s\n", 
                tInfo->thread_id, p_request->start, p_request->size, p_request->filename);
        
        p_reply = get_chunk(p_request, tInfo->thread_id);
        free(p_request);
        if (!p_reply) {
            printf("childDownloader(%d): did not get a valid chunk, returning NULL to parent...\n", tInfo->thread_id);
            pthread_exit(NULL);
        }
        
        memcpy(data_string + (chunk_start - tInfo->start), &(p_reply->data), p_reply->size);
        
        printf("childDownloader(%d): Chunk (%d) received:\n", tInfo->thread_id, p_reply->size);
        int t;
        for (t = 0; t < p_reply->size; t++) {
            printf("%4d ", data_string[t]);
        }
        printf("\n----------\n");
        
        chunk_start += p_reply->size;
        free(p_reply);
    }
    
    printf("\n\n-------------------------\nchildDownloader(%d): Chunk (%d) received:\n", tInfo->thread_id, tInfo->size);
        int t;
        for (t = 0; t < tInfo->size; t++) {
            printf("%4d ", data_string[t]);
        }
        printf("\n--------------------------------------------\n\n");
    
    
    pthread_exit((void *) data_string);
}

packet_type *get_chunk(packet_type *p_request, int t_id)
{
    int sockfd, n;
    socklen_t len;
    struct sockaddr_in servaddr;
    packet_type *p_reply;
    
    //Set sigalarm action
    struct sigaction sact = {
        .sa_handler = recvfrom_alarm,
        .sa_flags = 0,
    };
    if (sigaction(SIGALRM, &sact, NULL) == -1) {
        err_sys("  get_chunk(): sigaction error: %s", strerror(errno));
    }
    
    //this loop tries every server NUM_ATTEMPTS times
    int i;
    for (i = t_id; i < servers->num * NUM_ATTEMPTS; i++) {
        
        //set up socket for ith server
        bzero(&servaddr, sizeof (struct sockaddr_in));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(servers->ports[i%servers->num]);
        if (inet_pton(AF_INET, servers->IPs[i%servers->num], &servaddr.sin_addr) <= 0) {
            err_quit("  get_chunk():  ERROR: inet_pton error for %s\n", servers->IPs[i%servers->num]);
        }
        sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
        //printf("  get_chunk(): Trying server %s:%u\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);
        
        p_reply = new_blank_packet();
        
        //send the packet!
        Sendto(sockfd, p_request, sizeof(packet_type), 0, (SA *) &servaddr, sizeof(servaddr));
        //printf("  get_chunk(): Data sent, wait for reply...\n");
        len = sizeof(servaddr);
        
        //wait up to TIMEOUT seconds for reply
        alarm(TIMEOUT_PERIOD);
        n = recvfrom(sockfd, p_reply, sizeof(packet_type) + p_request->size, 
                     0, (SA *) &servaddr, &len);
        if (n < 0) {
            if (errno == EINTR) { //This server timed out, let's try another
                printf("  get_chunk(): Server timeout. Trying next...\n");
                free(p_reply);
                continue; 
            } else { //There was some other error
                err_sys("  get_chunk(): recvfrom error: ", strerror(errno));
            }
        } else { //got a packet!
            //If it's a good one, we break
            if (p_reply->opcode == chunk_reply && p_reply->size == p_request->size) {
                alarm(0);
                break;
            //otherwise, keep trying
            } else {
                printf("  get_chunk(): Got a bad packet (opcode=%d, reply_size=%d, request_size=%d)\n", 
                        p_reply->opcode, p_reply->size, p_request->size);
                free(p_reply);
                continue;
            }
        }
    }
    //We exit the for() either when we run out of servers or when we get a good packet
    if (i >= servers->num) {//we ran out
        err_sys("  get_chunk():  ERROR: No servers reachable, tried %d!\n", i);
    } 
    //we got a good packet
    return p_reply;
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

static void recvfrom_alarm (int signo)
{
    return; //We just need to interrupt recvfrom
}

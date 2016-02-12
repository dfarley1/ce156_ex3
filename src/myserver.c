/*
 * Daniel Farley - dfarley@ucsc.edu
 * CE 156 - Programing Assignment 1
 * Server code.  Run as "./myserver <port number>"
 */

#include "myunp.h"
#include "globals.h"

int sockfd;

void sendFileSize(packet_type *p_recv, struct sockaddr_in *cliaddr, socklen_t *len);
void sendFileChunk(packet_type *p_recv, struct sockaddr_in *cliaddr, socklen_t *len);

int main(int argc, char **argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    
    int n;
    socklen_t len;
    unsigned int port_num;
    struct sockaddr_in servaddr, cliaddr;
    packet_type *p_recv = new_blank_packet();
    
    //Check arguments
    if (argc != 2) {
        printf("Usage: ./myserver <port number>\n\n");
        return 1;
    }
    port_num = strtoul(argv[1], NULL, 10);
    
    //Set up the socket
    sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_num);
    
    Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));
    printf("  Server started on %s:%u...\n", inet_ntoa(servaddr.sin_addr), servaddr.sin_port);
    
    for (;;) {
        len = sizeof(cliaddr);
        
        //Wait for and get a packet_type
        //TODO: Should I do a peek first and make sure it's not a massive packet?
        n = recvfrom(sockfd, 
                     p_recv, 
                     sizeof(packet_type) + MAX_DATA_SIZE - 1, 
                     0, 
                     (SA *) &cliaddr,
                     &len);
        if (n == -1) {
            printf("recvfrom error: %s\n", strerror(errno));
            //error: ignore it?
        } else if (n == 0) {
            printf("recvfrom error: n == 0\n");
            //"peer has performed an orderly shutdown"
            //What do?
        } else {
            switch (p_recv->opcode) {
                case size_request:
                    printf("Packet received: size_request -> sendFileSize()\n");
                    sendFileSize(p_recv, &cliaddr, &len);
                    break;
                case size_reply:
                    printf("Packet received: size_reply, ignoring...\n");
                    break;
                case chunk_request:
                    printf("Packet received: chunk_request -> sendFileChunk()\n");
                    sendFileChunk(p_recv, &cliaddr, &len);
                    break;
                case chunk_reply:
                    //ignore?
                    printf("Packet received: chunk_reply, ignoring...\n");
                    break;
                case error:
                    //ignore?
                    printf("Packet received: error, ignoring...\n");
                    break;
                default:
                    //ignore
                    printf("Packet received: other, ignoring...\n");
                    break;
            }
        }
        free(p_recv);
        p_recv = new_blank_packet();
    }
    
    return 0;
}

void sendFileSize(packet_type *p_recv, struct sockaddr_in *cliaddr, socklen_t *len)
{
    struct stat st;
    packet_type *reply_packet;
    
    if (stat(p_recv->filename, &st) != 0) {
        //uh oh, send error packet
        printf("sendFileSize(): stat() ERROR, sending \"%s\" to %s:%u...\n", 
               strerror(errno), inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        reply_packet = new_packet(error, strlen(strerror(errno)), 
                                               0, p_recv->filename, strerror(errno));
    } else {
        //got size, send size_reply
        printf("sendFileSize(): got size %d, sending to %s:%u...\n", 
               st.st_size, inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        reply_packet = new_packet(size_reply, st.st_size, 0, p_recv->filename, NULL);
    }
    Sendto(sockfd, reply_packet, sizeof(packet_type) + reply_packet->size, 0, (SA *) cliaddr, sizeof(*cliaddr));
    free(reply_packet);
}

void sendFileChunk(packet_type *p_recv, struct sockaddr_in *cliaddr, socklen_t *len)
{
    struct stat st;
    FILE *fp;
    packet_type *p_reply = new_blank_packet();
    
    
    /*
    int retval = 0;
    char *output;
    struct stat st;
    //bzero(output, size);
    
    char * header;
    asprintf(&header, "%d\n%d\n\n", start, size);
    
    if (stat(filename, &st) != 0) {
        //-1 header specifies error, data portion will contain strerror
        output = calloc(8 + strlen(strerror(errno)) + 1, sizeof(char));
        strcpy(output, "-1\n-1\n\n");
        strcpy(output+strlen(output), strerror(errno));
        
        printf("sendFileChunk(): stat() ERROR. Sending \"%s\" to client", output);
        retval = -1;
    } else {
        //if sizes don't match, send error
        if (st.st_size < size) {
            asprintf(&output, "-1\n-1\n\nFile sizes do not match!  reported size=%d\n  found size=%d", 
                    size, st.st_size);
            
            printf("sendFileChunk(): ERROR.  Sending \"%s\" to client", output);
            retval = -2;
        //else if the start point is beyond the start of the file, send error
        } else if (start >= st.st_size) {
            asprintf(&output, "-1\n-1\n\nStart position larger than file size!  start=%d\n  size=%d", 
                    start, size);
            
            printf("sendFileChunk(): ERROR.  Sending \"%s\" to client", output);
            retval = -3;
        //otherwise, time to open the file
        } else {
            asprintf(&output, "%d\n%d\n\n", start, size);
            output = realloc(output, strlen(output) + size + 2);
            bzero(output + strlen(output), size + 2);
            
            //put file chunk into output+strlen(output)
            FILE *fp = fopen(filename, "r");
            if (fp) {
                fseek(fp, start, SEEK_SET);
                char *chunkStart = output + strlen(output);
                int outsize = 0;
                do {
                    chunkStart[outsize++] = fgetc(fp);
                    if (outsize >= size) break;
                } while (1);
                fclose(fp);
                
                printf("sendFileChunk(): SUCCESS: Sending ----------\n", output);
                int t;
                for (t = 0; t < size; t++) {
                    printf("%4d ", chunkStart[t]);
                }
                printf("\n----------\n");
                
            } else {
                output = realloc(output, 8 + strlen(strerror(errno)) + 1);
                bzero(output, 8 + strlen(strerror(errno)) + 1);
                strcpy(output, "-1\n-1\n\n");
                strcpy(output+strlen(output), strerror(errno));
                
                printf("sendFileChunk(): fopen(%s) ERROR. Sending \"%s\" to client", filename, output);
                retval = -4;
            }
        }
    }
    
    printf("sendFileChunk(): SUCCESS: Sending ----------\n", output);
    int t;
    for (t = 0; t < strlen(header) + size; t++) {
        printf("%4d ", output[t]);
    }
    printf("\n----------\n");
    
    Write(connfd, output, strlen(header) + size);
    Close(connfd);
    free(output);
    return retval;
    */
}

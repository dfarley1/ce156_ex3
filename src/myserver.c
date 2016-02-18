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
            if (p_recv->opcode == size_request) {
                printf("Packet received: size_request -> sendFileSize()\n");
                sendFileSize(p_recv, &cliaddr, &len);
            } else if (p_recv->opcode == chunk_request) {
                printf("Packet received: chunk_request -> sendFileChunk()\n");
                sendFileChunk(p_recv, &cliaddr, &len);
            } else {
                printf("Packet received: %d, ignoring...\n", p_recv->opcode);
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
    packet_type *p_reply;
    
    if (stat(p_recv->filename, &st) != 0) {
        //uh oh, send error packet
        printf("sendFileSize(): stat() ERROR, sending \"%s\" to %s:%u...\n", 
               strerror(errno), inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        p_reply = new_packet(error, strlen(strerror(errno)), 
                                               0, p_recv->filename, strerror(errno));
        Sendto(sockfd, p_reply, sizeof(packet_type) + p_reply->size, 0, (SA *) cliaddr, sizeof(*cliaddr));
    } else {
        //got size, send size_reply
        printf("sendFileSize(): got size %d, sending to %s:%u...\n", 
               st.st_size, inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        p_reply = new_packet(size_reply, st.st_size, 0, p_recv->filename, NULL);
        Sendto(sockfd, p_reply, sizeof(packet_type), 0, (SA *) cliaddr, sizeof(*cliaddr));
    }
    
    free(p_reply);
}

void sendFileChunk(packet_type *p_recv, struct sockaddr_in *cliaddr, socklen_t *len)
{
    struct stat st;
    FILE *fp;
    packet_type *p_reply;
    
    //check for file
    if (stat(p_recv->filename, &st) != 0) {
        printf("sendFileSize(): stat() ERROR, sending \"%s\" to %s:%u...\n", 
               strerror(errno), inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        p_reply = new_packet(error, strlen(strerror(errno)), 0, p_recv->filename, strerror(errno));
    //check request bounds against file size
    } else if (p_recv->start + p_recv->size > st.st_size) {
        printf("sendFileSize(): out of bounds ERROR, sending \"%s\" to %s:%u...\n", 
               ERR_OUT_OF_BOUNDS, inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        p_reply = new_packet(error, strlen(ERR_OUT_OF_BOUNDS), 0, p_recv->filename, ERR_OUT_OF_BOUNDS);
    //looks like we're good
    } else if (p_recv->size > MAX_DATA_SIZE) {
        printf("sendFileSize(): illegal size value ERROR, sending \"%s\" to %s:%u...\n", 
               ERR_ILLEGAL_SIZE, inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
        p_reply = new_packet(error, strlen(ERR_ILLEGAL_SIZE), 0, p_recv->filename, ERR_ILLEGAL_SIZE);
    } else {
        fp = fopen(p_recv->filename, "r");
        if (fp) {
            fseek(fp, p_recv->start, SEEK_SET);
            int iter = 0;
            p_reply = new_packet(chunk_reply, p_recv->size, p_recv->start, p_recv->filename, NULL);
            
            do {
                (&(p_reply->data))[iter++] = fgetc(fp);
                if (iter >= p_recv->size) break;
            } while (1);
            fclose(fp);
            
            printf("sendFileChunk(): SUCCESS: Sending ----------\n");
            int t;
            for (t = 0; t < p_reply->size; t++) {
                printf("%4d ", (&(p_reply->data))[t]);
            }
            printf("\n----------\n");
            
        } else {
            printf("sendFileSize(): fopen() ERROR, sending \"%s\" to %s:%u...\n", 
                    strerror(errno), inet_ntoa(cliaddr->sin_addr), cliaddr->sin_port);
            p_reply = new_packet(error, strlen(strerror(errno)), 0, p_recv->filename, strerror(errno));
                                                   
        }
    }
    
    Sendto(sockfd, p_reply, sizeof(packet_type) + p_reply->size, 0, (SA *) cliaddr, sizeof(*cliaddr));
    free(p_reply);
    
    /*
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

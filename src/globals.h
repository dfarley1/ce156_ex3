#define FILENAME_LENGTH 100
#define MAX_DATA_SIZE 512

typedef enum {
    size_request = 100,
    size_reply,
    chunk_request,
    chunk_reply,
    error
} opcode_type;

typedef struct {
    opcode_type opcode;
    int size;
    int start;
    char filename[FILENAME_LENGTH + 1];
    char data;
} packet_type;


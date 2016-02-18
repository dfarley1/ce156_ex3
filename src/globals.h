#define FILENAME_LENGTH 100
#define MAX_DATA_SIZE 50
#define TIMEOUT_PERIOD 5
#define NUM_ATTEMPTS 5
#define ERR_OUT_OF_BOUNDS "Requested chunk out of bounds"
#define ERR_ILLEGAL_SIZE "Illegal size value"

typedef enum {
    error,
    size_request,
    size_reply,
    chunk_request,
    chunk_reply
} opcode_type;

typedef struct {
    opcode_type opcode;
    int size;
    int start;
    char filename[FILENAME_LENGTH + 1];
    char data;
} packet_type;

packet_type* new_blank_packet()
{
    packet_type *new_packet = calloc(sizeof(packet_type) + MAX_DATA_SIZE, 1);
    new_packet->opcode = error;
    new_packet->size = 0;
    new_packet->start = 0;
    return new_packet;
}

packet_type* new_packet(opcode_type _opcode, int _size, int _start, char *_filename, char *_data)
{
    packet_type *new_packet;
    if ((_opcode == error) || (_opcode == chunk_reply)) {
        new_packet = calloc(sizeof(packet_type) + _size, 1);
    } else {
        new_packet = calloc(sizeof(packet_type), 1);
    }
    if (_data) {
        memcpy(&(new_packet->data), _data, _size);
    }
    
    new_packet->opcode = _opcode;
    new_packet->size = _size;
    new_packet->start = _start;
    if (_filename) {
        strncpy(new_packet->filename, _filename, FILENAME_LENGTH);
    }
    
    return new_packet;
}


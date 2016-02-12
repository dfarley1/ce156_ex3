#define FILENAME_LENGTH 100
#define MAX_DATA_SIZE 512
#define TIMEOUT_PERIOD 5
#define NUM_ATTEMPTS 5

typedef enum {
    size_request = 100,
    size_reply,
    chunk_request,
    chunk_reply,
    error,
    none
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
    new_packet->opcode = none;
    new_packet->size = 0;
    new_packet->start = 0;
    return new_packet;
}

packet_type* new_packet(opcode_type _opcode, int _size, int _start, char *_filename, char *_data)
{
    packet_type *new_packet;
    if (_data && ((_opcode == error) || (_opcode == chunk_reply))) {
        new_packet = calloc(sizeof(packet_type) + _size, 1);
        memcpy(&(new_packet->data), _data, _size);
    } else {
        new_packet = calloc(sizeof(packet_type), 1);
    }
    new_packet->opcode = _opcode;
    new_packet->size = _size;
    new_packet->start = _start;
    if (_filename) {
        strncpy(new_packet->filename, _filename, FILENAME_LENGTH);
    }
    
    return new_packet;
}


// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayer linklayer;
    strcpy(linklayer.serialPort, serialPort);
    linklayer.baudRate = baudRate;
    linklayer.nRetransmissions = nTries;
    linklayer.timeout = timeout;
    linklayer.role = strcmp(role, "tx") == 0 ? LlTx : LlRx;

    if (llopen(linklayer) == -1) {
        perror("Could not establish connection\n");
        exit(1);
    }
}

unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {
    return 0;
}

void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {

}

unsigned char * getControlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size) {
    return 0;
}

unsigned char * getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize) {
    return 0;
}

unsigned char * getData(FILE* fd, long int fileLength) {
    return 0;
}

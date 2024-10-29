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
        printf("Could not establish connection\n");
        exit(1);
    }
}

unsigned char* parseControlPacket(unsigned char* packet, int size, unsigned long int *fileSize) {
    if (size < 3) {
        printf("Packet is too small to contain minimum required data\n");
        return NULL;
    }

    *fileSize = 0;

    unsigned char fileSizeBytes = packet[2];

    if (size < 3 + fileSizeBytes) {
        printf("Packet is too small to contain the declared file size field.\n");
        return NULL;
    }

    for (unsigned int i = 0; i < fileSizeBytes; i++) {
        *fileSize = (*fileSize << 8 | packet[3+i]);
    }


    if (size < 3 + fileSizeBytes + 2) {
        printf("Packet is too small to contain the file name length byte.\n");
        return NULL;
    }

    unsigned char fileNameBytes = packet[3 + fileSizeBytes + 1];

    if (size < 3 + fileSizeBytes + 2 + fileNameBytes) {
        printf("Packet is too small to contain the declared file name field.\n");
        return NULL;
    }

    unsigned char *name = (unsigned char*)malloc(fileNameBytes+1);
    memcpy(name, packet + 3 + fileSizeBytes + 2, fileNameBytes);
    name[fileNameBytes] = '\0';

    return name;
}

void parseDataPacket(const unsigned char* packet, const unsigned int packetSize, unsigned char* buffer) {
    if (packetSize < 5) {
        printf("Invalid packet size\n");
        return;
    }

    unsigned char controlField = packet[0];

    if (controlField != 2) {
        printf("Not a data packet\n");
        return;
    }

    unsigned char sequenceNumber = packet[1];

    unsigned short dataLength = (packet[2] << 8) | packet [3];

    if (packetSize < 4 + dataLength) {
        printf("Packet size does not match data length\n");
        return;
    }

    memcpy(buffer, packet + 4, dataLength);

    buffer[dataLength] = '\0';

    //debugging
    printf("Control Field: %u\n", controlField);
    printf("Sequence Number: %u\n", sequenceNumber);
    printf("Data Length: %u\n", dataLength);
    printf("Data: ");
    for (unsigned int i = 0; i < dataLength; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}

unsigned char * getControlPacket(const unsigned int c, const char* filename, long int length, unsigned int* size) {
    if (!filename || length < 0 || !size) return NULL;

    unsigned char fileSizeType = 0;
    unsigned char fileSizeLength = sizeof(length);

    unsigned char fileNameType = 1;
    unsigned char fileNameLength = strlen(filename);

    *size = 1 + 2 + fileSizeLength + 2 + fileNameLength;

    unsigned char *packet = (unsigned char*)malloc(*size);
    if (!packet) return NULL;

    unsigned int index = 0;
    packet[index++] = (unsigned char) c;
    packet[index++] = fileSizeType;
    for(int i = fileSizeLength - 1; i >= 0; i--) {
        packet[index++] = (length >> (i * 8)) & 0xFF;
    }

    packet[index++] = fileNameType;
    packet[index++] = fileNameLength;
    memcpy(packet + index, filename, fileNameLength);
 
    return packet;
}

unsigned char * getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize) {
    *packetSize = 4 + dataSize; 

    unsigned char *packet = (unsigned char*)malloc(*packetSize);
    if (packet == NULL) return NULL;    

    int index = 0;

    packet[index++] == 2;
    packet[index++] = sequence;
    packet[index++] = (dataSize >> 8) & 0xFF;
    packet[index++] = dataSize & 0xFF;

    memcpy(packet + index, data, dataSize); 

    return packet;
}

unsigned char *getData(FILE* fd, long int fileLength) {
    unsigned char *data = (unsigned char *)malloc(fileLength);
    if (!data) {
        perror("Memory allocation failed");
        return NULL;
    }

    size_t bytesRead = fread(data, 1, fileLength, fd);
    if (bytesRead != fileLength) {
        perror("File reading failed");
        free(data);
        return NULL;
    }

    return data;
}
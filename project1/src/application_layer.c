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
    
    switch (linklayer.role) {
    case LlTx:
        unsigned char buf[3] = {0x01, 0x02, 0x04}; //0x07        0x7e, 0x02, 0x7d, 0x7c/0x7f

        if (llwrite(buf, 3) == -1) {
            printf("Error while sending data to receiver\n");
            exit(2);
        }

        /*
        buf[0] = 0x7e;
        buf[2] = 0x7d;

        if (llwrite(buf, 3) == -1) {
            printf("Error while sending data to receiver\n");
            exit(2);
        }*/

        if (llclose(0) == -1) {
            printf("Error on closing connection\n");
            exit(4);
        }

        break;
    
    case LlRx:
        unsigned char packet[3];
        int result = llread(packet);
        printf("result = %d\n", result);
        if (result == -1) {
            printf("Error while reading data from transmitor\n");
            exit(3);
        }

        printf("Packet received:\n");
        for (int i = 0; i < 3; i++) {
            printf("0x%02X ", packet[i]);
        }
        printf("\n");

        /*
        packet[0] = 0x00;
        packet[1] = 0x00;
        packet[2] = 0x00;

        result = llread(packet);
        printf("result = %d\n", result);
        if (result == -1) {
            printf("Error while reading data from transmitor\n");
            exit(3);
        }

        printf("Packet received:\n");
        for (int i = 0; i < 3; i++) {
            printf("0x%02X ", packet[i]);
        }
        printf("\n");*/

        result = llread(packet);
        printf("result = %d\n", result);
        if (result == -1) {
            printf("Error while disconnecting from transmitor\n");
            exit(3);
        }

        break;
    
    default:
        break;
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

// Application layer protocol implementation

#include "application_layer.h"
#include <stdio.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayer linklayer;
    strcpy(linklayer.serialPort, serialPort);
    linklayer.baudRate = baudRate;
    linklayer.nRetransmissions = nTries;
    linklayer.timeout = timeout;
    linklayer.role = strcmp(role, "tx") ? LlRx : LlTx;

    if (llopen(linklayer) == -1) {
        fprintf(stderr, "Error: Could not establish connection\n");
        exit(1);
    }

    FILE *file;

    unsigned char *packet;
    unsigned long int receivedFileSize = 0;
    int packetSize;

    
    switch(linklayer.role) {
        case LlTx: 
            file = fopen(filename, "rb");
            if (file == NULL) {
                perror("Error opening file\n");
                exit(-1);
            }

            int prev = ftell(file);
            fseek(file,0L,SEEK_END);
            long int fileSize = ftell(file)-prev;
            fseek(file,prev,SEEK_SET);
            printf("Debug: File size = %ld bytes\n", fileSize); //debug


            unsigned int controlPacketSize;
            unsigned char *controlPacket = getControlPacket(2, filename, fileSize, &controlPacketSize);
            if (llwrite(controlPacket, controlPacketSize) == -1) {
                fprintf(stderr, "Error: Failed to send start control packet\n");
                free(controlPacket);
                fclose(file);
                exit(-1);
            }
            free(controlPacket);

            unsigned char sequence = 0;
            unsigned char* content = getData(file, fileSize);
            long int bytesLeft = fileSize;

            while (bytesLeft > 0) {
                int dataSize = (bytesLeft > MAX_PAYLOAD_SIZE) ? MAX_PAYLOAD_SIZE : bytesLeft;
                unsigned char *data = (unsigned char *)malloc(dataSize);

                memcpy(data, content, dataSize);
                int packetSize;
                unsigned char* packet = getDataPacket(sequence, data, dataSize, &packetSize);

                if (dataSize > MAX_PAYLOAD_SIZE) {
                    fprintf(stderr, "Error: Payload size exceeded maximum limit\n");
                    free(packet);
                    free(data);
                    fclose(file);
                    exit(-1);
                }

                if (llwrite(packet, packetSize) == -1) {
                    fprintf(stderr, "Error: Failed to send data packet\n");
                    free(packet);
                    free(data);
                    fclose(file);
                    exit(-1);
                }
                
                bytesLeft -= dataSize; 

                content += dataSize; 

                printf("Debug: Payload size sent = %d bytes, Frame size (with headers) = %d bytes, remaining file size = %ld bytes\n", dataSize, packetSize, bytesLeft);                
                // Clean up
                free(packet);
                free(data);

                // Update the sequence number
                sequence = (sequence + 1) % 256;

                printf("Debug: Remaining file size = %ld bytes\n", bytesLeft);
            }

            unsigned char *endControlPacket = getControlPacket(3, filename, 0, &controlPacketSize);
            if (llwrite(endControlPacket, controlPacketSize) == -1) {
                fprintf(stderr, "Error: Failed to send end control packet\n");                
                free(endControlPacket);
                fclose(file);
                exit(-1);
            }
            free(endControlPacket);
            
            llclose(0);
            break;

        case LlRx:
            packet = (unsigned char *)malloc(MAX_PAYLOAD_SIZE);
            receivedFileSize = 0;
            packetSize = llread(packet);

            if (packetSize < 0) {
                fprintf(stderr, "Error: Failed to read start control packet\n");
                free(packet);
                llclose(0);
                exit(1);
            }   

            unsigned char *filenameReceived = parseControlPacket(packet, packetSize, &receivedFileSize);
            if (filenameReceived == NULL) {
                fprintf(stderr, "Error: Could not parse start control packet\n");
                free(packet);
                llclose(0);
                exit(-1);
            }

            file = fopen((char*)filename, "wb+");
            if (file == NULL) {
                perror("Error creating file\n");
                free(packet);
                llclose(0);
                exit(-1);
            }

            while (1) {
                packetSize = 0;    
                while (packetSize <= 0) {
                    packetSize = llread(packet);
                }
                if(packet[0] != 3){
                    unsigned char *buffer = (unsigned char*)malloc(packetSize);
                    parseDataPacket(packet, packetSize, buffer);
                    fwrite(buffer, sizeof(unsigned char), packetSize-4, file);
                    free(buffer);
                }
                else break;
            }

            fclose(file);
            free(packet);
            llclose(0);
            break;

        default:
            fprintf(stderr, "Error: Unknown role\n");
            llclose(0);
            exit(1);
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
    /*
    printf("Control Field: %u\n", controlField);
    printf("Sequence Number: %u\n", sequenceNumber);
    printf("Data Length: %u\n", dataLength);
    printf("Data: ");
    for (unsigned int i = 0; i < dataLength; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n")
    */
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
    packet[index++] = 0; // file size type
    packet[index++] = fileSizeLength;

    for(int i = fileSizeLength - 1; i >= 0; i--) {
        packet[index++] = (length >> (i * 8)) & 0xFF;
    }

    packet[index++] = 1; // file name type
    packet[index++] = fileNameLength;
    memcpy(packet + index, filename, fileNameLength);
 
    return packet;
}

unsigned char * getDataPacket(unsigned char sequence, unsigned char *data, int dataSize, int *packetSize) {
    *packetSize = 4 + dataSize; 

    unsigned char *packet = (unsigned char*)malloc(*packetSize);
    if (packet == NULL) return NULL;    

    int index = 0;

    packet[index++] = 2; // control field for data packet
    packet[index++] = sequence;
    packet[index++] = (dataSize >> 8) & 0xFF; // high byte of data size
    packet[index++] = dataSize & 0xFF; // low byte of data size

    memcpy(packet + index, data, dataSize); 

    return packet;
}

unsigned char *getData(FILE* spfd, long int fileLength) {
    unsigned char *data = (unsigned char *)malloc(fileLength);
    if (!data) {
        fprintf(stderr, "Error allocating memory for file data\n");
        return NULL;
    }

    size_t bytesRead = fread(data, sizeof(unsigned char), fileLength, spfd);
    if (bytesRead != fileLength) {
        if (feof(spfd)) {
            printf("Debug: Reached end of file\n");
        } else if (ferror(spfd)) {
            printf("Debug: An error occurred while reading the file\n");
        }
    }

    return data;
}


//DEBUGG
/*switch (linklayer.role) {
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
        }

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
        printf("\n");

        result = llread(packet);
        printf("result = %d\n", result);
        if (result == -1) {
            printf("Error while disconnecting from transmitor\n");
            exit(3);
        }

        break;
    
    default:
        break;*/


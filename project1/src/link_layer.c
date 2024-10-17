// Link layer protocol implementation

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "link_layer.h"
#include "serial_port.h"

#define BAUDRATE B38400
//MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256

#define FLAG 0x7E
#define ADDRESS_SENT_SENDER 0x03
#define ADDRESS_ANSWER_RECEIVER 0x03
#define ADDRESS_SENT_RECEIVER 0X01
#define ADDRESS_ANSWER_SENDER 0X01
#define CONTROL_SET 0X03
#define CONTROL_UA 0X07

volatile int STOP = FALSE;

typedef enum {
    START_R,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK_R,
    STOP_RCV,
} ReceiverState;

typedef enum {
    START_S,
    FLAG_SDR,
    A_SDR,
    C_SDR,
    BCC_OK_S,
    STOP_SDR,
} SenderState;

int alarmEnabled = FALSE;
int responseReceived = FALSE;
int alarmCount = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    if (openSerialPort(connectionParameters.serialPort,
                       connectionParameters.baudRate) < 0)
    {
        return -1;
    }

    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    // TODO
    switch (connectionParameters.role)
    {
    case LlTx:
        printf("New termios structure set\n");

        // Create string to send
        unsigned char bufS[BUF_SIZE] = {0};

        bufS[0] = FLAG;
        bufS[1] = ADDRESS_SENT_SENDER;
        bufS[2] = CONTROL_SET;
        bufS[3] = ADDRESS_SENT_SENDER ^ CONTROL_SET;
        bufS[4] = FLAG;

        // In non-canonical mode, '\n' does not end the writing.
        // Test this condition by placing a '\n' in the middle of the buffer.
        // The whole buffer must be sent even with the '\n'.
        bufS[5] = '\n';

        (void)signal(SIGALRM, alarmHandler);

        unsigned char response[BUF_SIZE] = {0};
        int response_bytes = 0;
        SenderState senderState = START_S;

        while (alarmCount < 4 && !responseReceived) {
            if (alarmEnabled == FALSE)
            {
                int bytes = write(fd, bufS, BUF_SIZE);
                sleep(1);
                printf("%d bytes written\n", bytes);
                alarm(3); // Set alarm to be triggered in 3s
                
                alarmEnabled = TRUE;
            }

            response_bytes = read(fd, response, BUF_SIZE);

            if (response_bytes > 0) {
                for (int i = 0; i < response_bytes; i++) {
                    switch(senderState) {
                        case START_S:
                            printf("start\n");
                            if (response[i] == FLAG) {
                                senderState = FLAG_SDR;
                            }
                            else {
                                senderState = START_S;
                            }
                            break;
                        case FLAG_SDR:
                            printf("flag\n");
                            if (response[i] == ADDRESS_ANSWER_RECEIVER) {
                            
                                senderState = A_SDR;
                            }
                            else if (response[i] == FLAG) {
                            
                                senderState = FLAG_SDR;
                            }
                            else {
                                senderState = START_S;
                            }
                            break;
                        case A_SDR:
                            printf("A\n");
                            if (response[i] == CONTROL_UA) {
                                senderState = C_SDR;
                            }
                            else if (response[i] == FLAG) {
                            
                                senderState = FLAG_SDR;
                            }
                            else {
                                senderState = START_S;
                            }
                            break;
                        case C_SDR:
                            printf("C\n");
                            if (response[i] == (ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA)) {
                                senderState = BCC_OK_S;
                            }
                            else if (response[i] == CONTROL_UA) {
                                
                                senderState = FLAG_SDR;
                            }
                            else {
                                
                                senderState = START_S;
                            }
                            break;
                        case BCC_OK_S:
                            printf("BCC\n");
                            if (response[i] == FLAG) {
                                
                                senderState = STOP_SDR;
                            }
                            else {

                                senderState = START_S;
                            }
                            break;
                        case STOP_SDR:
                            printf("STOP\n");
                            printf("Received UA frame successfully.\n");
                            responseReceived = TRUE;
                            alarm(0);
                            break;
                        default:
                            senderState = START_S;
                            break;
                    }
                }
            }
            else {
                //printf("No frame received.\n");
            }
        }

        printf("Ending program\n");
        break;
    case LlRx:
        printf("New termios structure set\n");

        // Loop for input
        unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
        ReceiverState ReceiverState = START_R;
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            int bytes = read(fd, buf, BUF_SIZE);
            buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

            //printf(":%s:%d\n", buf, bytes); //prints frame received
            if (bytes > 0) {
                for (int i = 0; i < bytes; i++) {
                    if (STOP == TRUE) break; 
                    switch(ReceiverState) {
                        case START_R:
                        printf("start\n");
                            if (buf[i] == FLAG) {
                                ReceiverState = FLAG_RCV;
                            }
                            else {
                                ReceiverState = START_R;
                            }
                            break;
                        case FLAG_RCV:
                            printf("flag\n");
                            if (buf[i] == ADDRESS_SENT_SENDER) {
                            
                                ReceiverState = A_RCV;
                            }
                            else if (buf[i] == FLAG) {
                            
                                ReceiverState = FLAG_RCV;
                            }
                            else {
                                ReceiverState = START_R;
                            }
                            break;
                        case A_RCV:
                            printf("A\n");
                            if (buf[i] == CONTROL_SET) {
                                ReceiverState = C_RCV;
                            }
                            else if (buf[i] == FLAG) {
                            
                                ReceiverState = FLAG_RCV;
                            }
                            else {
                                ReceiverState = START_R;
                            }
                            break;
                        case C_RCV:
                            printf("C\n");
                            if (buf[i] == (ADDRESS_SENT_SENDER ^ CONTROL_SET)) {
                                ReceiverState = BCC_OK_R;
                            }
                            else if (buf[i] == CONTROL_SET) {
                                
                                ReceiverState = FLAG_RCV;
                            }
                            else {
                                
                                ReceiverState = START_R;
                            }
                            break;
                        case BCC_OK_R:
                            printf("BCC\n");
                            if (buf[i] == FLAG) {
                                
                                ReceiverState = STOP_RCV;
                            }
                            else { 

                                ReceiverState = START_R;
                            }
                            break;
                        case STOP_RCV:
                            printf("STOP\n");
                            unsigned char uaFrame[6] = {FLAG, ADDRESS_ANSWER_RECEIVER, CONTROL_UA, ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA, FLAG};
                            write(fd, uaFrame, 6);
                            printf("Sent UA frame\n");
                            sleep(1);
                            STOP = TRUE;
                            break;
                        default:
                            ReceiverState = START_R;
                            break;
                    }
                }
            }
        }
        break;
    default:
        break;
    }

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}

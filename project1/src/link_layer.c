// Link layer protocol implementation

#include "link_layer.h"

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
    int fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    if (fd < 0) return -1;

    // TODO
    switch (connectionParameters.role) {
        case LlTx: {
            printf("New termios structure set\n");

            (void)signal(SIGALRM, alarmHandler);

            // Create string to send
            unsigned char bufS[6] = {FLAG, ADDRESS_SENT_SENDER, CONTROL_SET, ADDRESS_SENT_SENDER ^ CONTROL_SET, FLAG, '\0'};

            unsigned char response[BUF_SIZE] = {0};
            int response_bytes = 0;
            SenderState senderState = START_S;

            while (alarmCount < ALARM_MAX_RETRIES && !responseReceived) {
                if (alarmEnabled == FALSE)
                {
                    int bytes = write(fd, bufS, 6);
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
        }

        case LlRx: {
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
                                unsigned char uaFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, CONTROL_UA, ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA, FLAG};
                                write(fd, uaFrame, 5);
                                printf("Sent UA frame\n");
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
        }

        default:
            break;
    }
return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(int fd, unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int fd, int showStatistics)
{
    // TODO

    int clstat = closeSerialPort();
    return clstat;
}

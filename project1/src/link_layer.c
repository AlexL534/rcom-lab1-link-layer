// Link layer protocol implementation

#include "link_layer.h"

volatile int STOP = FALSE;

unsigned char frameNumberT = 0;
unsigned char frameNumberR = 1;

int alarmEnabled = FALSE;
int responseReceived = FALSE;
int alarmCount = 0;
int retransmissions = 0;
int timeout = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}

unsigned char checkControl(int fd) {
    unsigned char byte;
    unsigned char c = 0;
    SenderState state = START_S;

    while (state != STOP_SDR && alarmEnabled == FALSE) {
        if (read(fd,&byte, 1) > 0) {
            switch (state) {
                case START_S:
                    if (byte == FLAG) state = FLAG_SDR;
                    break;

                case FLAG_SDR:
                    if (byte == ADDRESS_SENT_RECEIVER) state = A_SDR;
                    else if (byte == FLAG) state = FLAG_SDR;
                    else state = START_S;
                    break;
                
                case A_SDR:
                    if (byte == RR0 || byte == RR1 || byte == REJ0 || byte == REJ1 || byte == DISC) {
                        state = C_SDR;
                        c = byte;
                    }
                    else if (byte == FLAG) state = FLAG_SDR;
                    else state = START_S;
                    break;

                case C_SDR:
                    if (byte == (ADDRESS_SENT_RECEIVER ^ c)) state = BCC_OK_S;
                    else if (byte == FLAG) state = FLAG_SDR;
                    else state = START_S;
                    break;

                case BCC_OK_S:
                    if (byte == FLAG) state = STOP_SDR;
                    else state = START_S;
                    break;

                default:
                    break;
            }
        }
    }
    return c;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int fd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    if (fd < 0) return -1;

    retransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    // TODO
    switch (connectionParameters.role) {
        case LlTx: {
            printf("\nNew termios structure set\n");

            (void)signal(SIGALRM, alarmHandler);

            // Create string to send
            unsigned char bufS[5] = {FLAG, ADDRESS_SENT_TRANSMITTER, CONTROL_SET, ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET, FLAG};

            unsigned char response[BUF_SIZE] = {0};
            int response_bytes = 0;
            SenderState senderState = START_S;

            while (alarmCount < ALARM_MAX_RETRIES && !responseReceived) {
                if (alarmEnabled == FALSE)
                {
                    int bytes = write(fd, bufS, 5);
                    sleep(1); 
                    printf("%d bytes written\n", bytes);
                    alarm(3); // Set alarm to be triggered in 3s
                    
                    alarmEnabled = TRUE;
                }

                response_bytes = read(fd, response, BUF_SIZE);

                if (response_bytes > 0) {
                    for (int i = 0; i <= response_bytes; i++) {
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
                                if (buf[i] == ADDRESS_SENT_TRANSMITTER) {
                                
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
                                if (buf[i] == (ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET)) {
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
    int inf_frame_size = 6 + bufSize;
    unsigned char *frame = (unsigned char *) malloc(inf_frame_size);

    frame[0] = FLAG;
    frame[1] = ADDRESS_SENT_TRANSMITTER;
    frame[2] = C_N(frameNumberT);
    frame[3] = ADDRESS_SENT_TRANSMITTER ^ C_N(frameNumberT);

    memcpy(frame+4,buf, bufSize);
    unsigned char BCC2 = 0;
    for (unsigned int i = 0; i < bufSize; i++) {
        BCC2 ^= buf[i]; // doing XOR of each byte with BCC2
    }

    int j = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG || buf[i] == ESC) {
            unsigned char *temp = realloc(frame, ++inf_frame_size);
            if (temp != NULL) {
                frame = temp;
                frame[j++] = ESC; //byte stuffing
            }
            else {
                free(frame);
                return -1;
            }
        }
        frame[j++] = buf[i];
    }

    // Ensure space for BCC2 and FLAG.
    unsigned char *temp = realloc(frame, inf_frame_size + 2);
    if (temp == NULL) {
        free(frame);
        return -1; // Memory allocation failed.
    }
    frame = temp;

    frame[j++] = BCC2;
    frame[j++] = FLAG;

    int current_transmission = 0;
    int rejected = 0;
    int accepted = 0;

    while (current_transmission < retransmissions) {
        alarmEnabled = FALSE;
        alarm(timeout);
        rejected = 0;
        accepted = 0;

        while (!alarmEnabled && !rejected && !accepted) {
            write(fd, frame, j);
            unsigned char command = checkControl(fd);

            if (command == REJ0 || command == REJ1) {
                rejected = 1;
            }

            else if (command == RR0 || command == RR1) {
                accepted = 1;
                frameNumberT = (frameNumberT + 1) % 2;
            }
            
        }

        if (accepted) break;
        current_transmission++;
    }

    free(frame);
    if (accepted) return inf_frame_size;
    else {
        llclose(fd, 1); //o 2º argumento não é 1, só meti para encher
        return -1;
    }

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

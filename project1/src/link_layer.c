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

unsigned char checkControl() {
    unsigned char byte;
    unsigned char c = 0;
    SenderState state = START_S;

    while (state != STOP_SDR && alarmEnabled == FALSE) {
        if (readByteSerialPort(&byte)) {
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

            SenderState senderState = START_S;

            while (alarmCount < ALARM_MAX_RETRIES && !responseReceived) {
                if (alarmEnabled == FALSE)
                {
                    int bytes = writeBytesSerialPort(bufS,5);
                    sleep(1); 
                    printf("%d bytes written\n", bytes);
                    alarm(3); // Set alarm to be triggered in 3s
                    
                    alarmEnabled = TRUE;
                }

                unsigned char response_byte = readByteSerialPort(&response_byte);

                for (int i = 0; i <= response_byte; i++) {
                    switch(senderState) {
                        case START_S:
                            printf("start\n");
                            if (response_byte == FLAG) {
                                senderState = FLAG_SDR;
                            }
                            else {
                                senderState = START_S;
                            }
                            break;
                        case FLAG_SDR:
                            printf("flag\n");
                            if (response_byte == ADDRESS_ANSWER_RECEIVER) {
                            
                                senderState = A_SDR;
                            }
                            else if (response_byte == FLAG) {
                            
                                senderState = FLAG_SDR;
                            }
                            else {
                                senderState = START_S;
                            }
                            break;
                        case A_SDR:
                            printf("A\n");
                            if (response_byte == CONTROL_UA) {
                                senderState = C_SDR;
                            }
                            else if (response_byte == FLAG) {
                            
                                senderState = FLAG_SDR;
                            }
                            else {
                                senderState = START_S;
                            }
                            break;
                        case C_SDR:
                            printf("C\n");
                            if (response_byte == (ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA)) {
                                senderState = BCC_OK_S;
                            }
                            else if (response_byte == CONTROL_UA) {
                                
                                senderState = FLAG_SDR;
                            }
                            else {
                                
                                senderState = START_S;
                            }
                            break;
                        case BCC_OK_S:
                            printf("BCC\n");
                            if (response_byte == FLAG) {
                                
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
                int byte2 = readByteSerialPort(buf);

                //printf(":%s:%d\n", buf, bytes); //prints frame received
                if (STOP == TRUE) break; 
                switch(ReceiverState) {
                    case START_R:
                    printf("start\n");
                        if (byte2 == FLAG) {
                            ReceiverState = FLAG_RCV;
                        }
                        else {
                            ReceiverState = START_R;
                        }
                        break;
                    case FLAG_RCV:
                        printf("flag\n");
                        if (byte2 == ADDRESS_SENT_TRANSMITTER) {
                        
                            ReceiverState = A_RCV;
                        }
                        else if (byte2 == FLAG) {
                        
                            ReceiverState = FLAG_RCV;
                        }
                        else {
                            ReceiverState = START_R;
                        }
                        break;
                    case A_RCV:
                        printf("A\n");
                        if (byte2 == CONTROL_SET) {
                            ReceiverState = C_RCV;
                        }
                        else if (byte2 == FLAG) {
                        
                            ReceiverState = FLAG_RCV;
                        }
                        else {
                            ReceiverState = START_R;
                        }
                        break;
                    case C_RCV:
                        printf("C\n");
                        if (byte2 == (ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET)) {
                            ReceiverState = BCC_OK_R;
                        }
                        else if (byte2 == CONTROL_SET) {
                            
                            ReceiverState = FLAG_RCV;
                        }
                        else {
                            
                            ReceiverState = START_R;
                        }
                        break;
                    case BCC_OK_R:
                        printf("BCC\n");
                        if (byte2 == FLAG) {
                            
                            ReceiverState = STOP_RCV;
                        }
                        else { 

                            ReceiverState = START_R;
                        }
                        break;
                    case STOP_RCV:
                        printf("STOP\n");
                        unsigned char uaFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, CONTROL_UA, ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA, FLAG};
                        writeBytesSerialPort(uaFrame, 5);
                        printf("Sent UA frame\n");
                        STOP = TRUE;
                        break;
                    default:
                        ReceiverState = START_R;
                        break;
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
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO
    int inf_frame_size = 6 + bufSize;
    unsigned char *stuffed_frame = (unsigned char *)malloc(2 * inf_frame_size);

    stuffed_frame[0] = FLAG;
    stuffed_frame[1] = ADDRESS_SENT_TRANSMITTER;
    stuffed_frame[2] = C_N(frameNumberT);
    stuffed_frame[3] = ADDRESS_SENT_TRANSMITTER ^ C_N(frameNumberT);

    unsigned char BCC2 = 0;
    for (unsigned int i = 0; i < bufSize; i++) {
        BCC2 ^= buf[i]; // doing XOR of each byte with BCC2
    }

    int j = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG) {
            stuffed_frame[j++] = ESC;
            stuffed_frame[j++] = FLAG ^ 0X20;
        }
        else if (buf[i] == ESC) {
            stuffed_frame[j++] = ESC;
            stuffed_frame[j++] = ESC ^ 0x20;
        }
        else {
            stuffed_frame[j++] = buf[i];
        }
    }

    if (BCC2 == FLAG) {
        stuffed_frame[j++] = ESC;
        stuffed_frame[j++] = FLAG ^ 0x20;
    } 
    else if (BCC2 == ESC) {
        stuffed_frame[j++] = ESC;
        stuffed_frame[j++] = ESC ^ 0x20;
    } 
    else {
        stuffed_frame[j++] = BCC2;
    }

    stuffed_frame[j++] = FLAG;
    
    inf_frame_size = j;

    int current_transmission = 0;
    int rejected = 0;
    int accepted = 0;

    while (current_transmission < retransmissions) {
        alarmEnabled = FALSE;
        alarm(timeout);
        rejected = 0;
        accepted = 0;

        while (!alarmEnabled && !rejected && !accepted) {
            writeBytesSerialPort(stuffed_frame, j);
            unsigned char command = checkControl();

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

    free(stuffed_frame);
    if (accepted) return inf_frame_size;
    else {
        llclose(0); //o 2º argumento não é 1, só meti para encher
        return -1;
    }

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte; 
    unsigned char c = 0;
    int x = 0;
    unsigned char supervisionFrame[5];
    ReceiverState state = START_R;

    while (state != STOP_RCV) {
        if (readByteSerialPort(&byte)) {
            switch (state) {
            case START_R:
                if (byte == FLAG) state = FLAG_RCV;
                break;

            case FLAG_RCV:
                if (byte == ADDRESS_SENT_TRANSMITTER) state = A_RCV;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START_R;
                break;

            case A_RCV:
                if (byte == C_N(0) || byte == C_N(1)) {
                    state = C_RCV;
                    c = byte;
                } 
                else if (byte == FLAG) state = FLAG_RCV;
                else if (byte == DISC) {
                    supervisionFrame[0] = FLAG;
                    supervisionFrame[1] = ADDRESS_SENT_RECEIVER;
                    supervisionFrame[2] = DISC;
                    supervisionFrame[3] = ADDRESS_SENT_RECEIVER ^ DISC;
                    supervisionFrame[4] = FLAG;
                    writeBytesSerialPort(supervisionFrame, 5);
                    return 0;
                }
                else state = START_R;
                break;
            
            case C_RCV:
                if (byte == (ADDRESS_SENT_TRANSMITTER ^ c)) state = READ_DATA;
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START_R;
                break;

            case READ_DATA:
                if (byte == ESC) state = ESC_FOUND;
                else if (byte == FLAG) {
                    unsigned char bcc2 = packet[--x];

                    unsigned char acc = 0;
                    for (unsigned int i = 0; i < x; i++) {
                        acc ^= packet[i];
                    }

                    if (bcc2 == acc) {
                        state = STOP_RCV;
                        unsigned char cResponse = frameNumberR ? RR1 : RR0;
                        unsigned char supervisionFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};
                        writeBytesSerialPort(supervisionFrame, 5);
                        frameNumberR = (frameNumberR + 1) % 2;
                        return x;
                    }
                    else {
                        printf("Error retransmission\n");
                        unsigned char cResponse = frameNumberR ? REJ1 : REJ0;
                        unsigned char supervisionFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse};
                        writeBytesSerialPort(supervisionFrame, 5);
                        return -1;
                    }
                }
                else {
                    packet[x++] = byte;
                }
                break;

            case ESC_FOUND:
                if (byte == (FLAG ^ 0x20)) {
                    packet[x++] = FLAG;
                }
                else if (byte == (ESC ^ 0x20)) {
                    packet[x++] = ESC;
                }
                else {
                    state = START_R;
                }
                state = READ_DATA;
                break;

            default:
                break;
            }
        }
    }

    return -1;
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

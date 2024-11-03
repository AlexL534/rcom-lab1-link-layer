// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;

unsigned char frameNumberT = 0;
unsigned char frameNumberR = 0;

int isTx = FALSE;

int alarmEnabled = FALSE;
int responseReceived = FALSE;
int alarmCount = 0;
int retransmissions = 0;
int timeout = 0;

//statistics
int framesSent = 0;
int framesReceived = 0;
int retransmissionsNumber = 0;
int timeouts = 0;
int framesRejected = 0;
clock_t startTime;
int duplicateFrames = 0;

// Alarm function handler
void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    timeouts++;
    if (alarmCount > 0) {
        printf("\nAlarm #%d\n\n", alarmCount);
    }
}

unsigned char checkControl() {
    unsigned char byte;
    unsigned char c = 0;
    SenderState state = START_S;

    while (state != STOP_SDR && alarmEnabled == TRUE) {
        if (readByteSerialPort(&byte)) {
            switch (state) {
                case START_S:
                    if (byte == FLAG) state = FLAG_SDR;
                    break;

                case FLAG_SDR:
                    if (byte == ADDRESS_ANSWER_RECEIVER) state = A_SDR;
                    else if (byte == FLAG) state = FLAG_SDR;
                    else state = START_S;
                    break;
                
                case A_SDR:
                    if (byte == RR0 || byte == RR1 || byte == REJ0 || byte == REJ1) {
                        state = C_SDR;
                        c = byte;
                    }
                    else if (byte == FLAG) state = FLAG_SDR;
                    else state = START_S;
                    break;

                case C_SDR:
                    if (byte == (ADDRESS_ANSWER_RECEIVER ^ c)) state = BCC_OK_S;
                    else if (byte == FLAG) state = FLAG_SDR;
                    else state = START_S;
                    break;

                case BCC_OK_S:
                    if (byte == FLAG) {
                        state = STOP_SDR;
                        return c;
                    }
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
int llopen(LinkLayer connectionParameters) {

    startTime = clock();

    int spfd = openSerialPort(connectionParameters.serialPort, connectionParameters.baudRate);
    if (spfd < 0) return -1;

    retransmissions = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    switch (connectionParameters.role) {
        case LlTx: {
            isTx = TRUE;
            printf("\nNew termios structure set\n\n");

            (void)signal(SIGALRM, alarmHandler);

            // Create string to send
            unsigned char bufS[FRAME_SIZE] = {FLAG, ADDRESS_SENT_TRANSMITTER, CONTROL_SET, ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET, FLAG};

            SenderState senderState = START_S;
            alarmCount = 0;
            alarmEnabled = FALSE;

            while (alarmCount < retransmissions && senderState != STOP_SDR) {
                if (!alarmEnabled) {
                    int bytes = writeBytesSerialPort(bufS,FRAME_SIZE);
                    // bufS[2] = CONTROL_SET;                            //SO PARA TESTE    
                    if (bytes < 0) {
                        perror("Failed to write bytes to serial port");
                        return -1;
                    }
                    sleep(1); //important
                    printf("%d bytes written\n", bytes);
                    alarm(timeout);
                    
                    alarmEnabled = TRUE;
                }

                unsigned char response_byte;

                if (readByteSerialPort(&response_byte) == -1) {
                    perror("Failed to read byte from serial port");
                    continue;
                }

                switch(senderState) {
                    case START_S:
                        //printf("start\n");
                        if (response_byte == FLAG) senderState = FLAG_SDR;
                        break;
                    case FLAG_SDR:
                        //printf("flag\n");
                        if (response_byte == ADDRESS_ANSWER_RECEIVER) senderState = A_SDR;
                        else if (response_byte != FLAG) senderState = START_S;
                        break;
                    case A_SDR:
                        //printf("A\n");
                        if (response_byte == CONTROL_UA) senderState = C_SDR;
                        else if (response_byte == FLAG) senderState = FLAG_SDR;
                        else senderState = START_S;
                        break;
                    case C_SDR:
                        //printf("C\n");
                        if (response_byte == (ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA)) senderState = BCC_OK_S;
                        else if (response_byte == FLAG) senderState = FLAG_SDR;
                        else senderState = START_S;
                        break;
                    case BCC_OK_S:
                        //printf("BCC\n");
                        if (response_byte == FLAG) {
                            senderState = STOP_SDR;
                            alarm(0);
                        }
                        else senderState = START_S;
                        break;
                    default:
                        senderState = START_S;
                        break;
                }
            }

            alarm(0);

            if (senderState == STOP_SDR) {
                //printf("STOP\n");
                printf("\nReceived UA frame successfully\n\nConnection sucessfull!\n\n");
                return 1;
            }

            else {
                printf("\nNo response from receiver\n\nCanceling operation...\n\n");
                return -1;
            }
            break;
        }

        case LlRx: {
            printf("\nNew termios structure set\n");

            // Loop for input
            unsigned char byte2;
            ReceiverState ReceiverState = START_R;
            while (ReceiverState != STOP_RCV) {
                if (readByteSerialPort(&byte2)) {
                    //printf(":%s:%d\n", buf, bytes); //prints frame received
                    switch(ReceiverState) {
                        case START_R:
                        //printf("start\n");
                            if (byte2 == FLAG) ReceiverState = FLAG_RCV;
                            else ReceiverState = START_R;
                            break;
                        case FLAG_RCV:
                            //printf("flag\n");
                            if (byte2 == ADDRESS_SENT_TRANSMITTER) ReceiverState = A_RCV;
                            else if (byte2 == !FLAG) ReceiverState = START_R;
                            break;
                        case A_RCV:
                            //printf("A\n");
                            if (byte2 == CONTROL_SET) ReceiverState = C_RCV;
                            else if (byte2 == FLAG) 
                                ReceiverState = FLAG_RCV;
                            else ReceiverState = START_R;
                            break;
                        case C_RCV:
                            //printf("C\n");
                            if (byte2 == (ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET)) {
                                ReceiverState = BCC_OK_R;
                            }
                            else if (byte2 == FLAG) ReceiverState = FLAG_RCV; 
                            else ReceiverState = START_R;
                            break;
                        case BCC_OK_R:
                            //printf("BCC\n");
                            if (byte2 == FLAG) ReceiverState = STOP_RCV;
                            else ReceiverState = START_R;
                            break;
                        default:
                            ReceiverState = START_R;
                            break;
                    }
                }
            }
            //printf("STOP\n");
            unsigned char uaFrame[FRAME_SIZE] = {FLAG, ADDRESS_ANSWER_RECEIVER, CONTROL_UA, ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA, FLAG};
            if (writeBytesSerialPort(uaFrame, FRAME_SIZE) < 0) {
                perror("Failed to send UA frame");
                return -1; 
            }
            printf("\nSent UA frame\n\n");
            return 1;
            break;
        }

        default:
            break;
    }
    return spfd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    int inf_frame_size = 6 + bufSize;
    unsigned char *stuffed_frame = (unsigned char *)malloc(2 * inf_frame_size);

    if (stuffed_frame == NULL) {
        perror("Memory allocation failed");
        return -1;
    }   

    stuffed_frame[0] = FLAG;
    stuffed_frame[1] = ADDRESS_SENT_TRANSMITTER;
    stuffed_frame[2] = C_N(frameNumberT);
    stuffed_frame[3] = ADDRESS_SENT_TRANSMITTER ^ C_N(frameNumberT);

    unsigned char BCC2 = 0;
    for (unsigned int i = 0; i < bufSize; i++) {
        BCC2 ^= buf[i]; // doing XOR of each byte with BCC2
    }

    printf("Frame BCC2 = 0x%02X\n", BCC2); // Debugging BCC2 value

    int j = 4;
    for (int i = 0; i < bufSize; i++) {
        if (buf[i] == FLAG) {
            stuffed_frame[j++] = ESC;
            stuffed_frame[j++] = FLAG ^ 0X20;
        } else if (buf[i] == ESC) {
            stuffed_frame[j++] = ESC;
            stuffed_frame[j++] = ESC ^ 0x20;
        } else {
            stuffed_frame[j++] = buf[i];
        }
    }

    if (BCC2 == FLAG) {
        stuffed_frame[j++] = ESC;
        stuffed_frame[j++] = FLAG ^ 0x20;
    } else if (BCC2 == ESC) {
        stuffed_frame[j++] = ESC;
        stuffed_frame[j++] = ESC ^ 0x20;
    } else {
        stuffed_frame[j++] = BCC2;
    }

    stuffed_frame[j++] = FLAG;

    inf_frame_size = j;

    int current_transmission = 0;
    int rejected = 0;
    int accepted = 0;
    alarmEnabled = FALSE;
    alarmCount = 0;

    while (current_transmission <= retransmissions) {
        rejected = 0;
        accepted = 0;

        if (!alarmEnabled) {
            current_transmission++;
            if (current_transmission > retransmissions + 1) break;
            alarmEnabled = TRUE;
            int bytesW = writeBytesSerialPort(stuffed_frame, j);
            framesSent++;
            if (bytesW < 0) {
                perror("Failed to write bytes to serial port");
                free(stuffed_frame);
                return -1; // Handle error
            }
            alarm(timeout);
            printf("%d bytes written\n\n", bytesW); // Debugging: bytes written to serial port
        }

        unsigned char command = checkControl();
        printf("Receiver response = 0x%02X\n", command); // Debugging: command received

        if (command == REJ0 || command == REJ1) {
            rejected = 1;
        } else if ((command == RR0 && frameNumberT == 1) || (command == RR1 && frameNumberT == 0)) {
            accepted = 1;
            frameNumberT = (frameNumberT + 1) % 2;
            printf("New Frame number = 0x%02X\n\n", frameNumberT); // Debugging: frame number
        }

        if (accepted) {
            alarm(0);
            break;
        } else if (rejected) {
            alarm(0);
            alarmEnabled = FALSE;
            alarmCount = -1;
            current_transmission = 0;
            printf("Frame was rejected. Resending data bytes.\n"); // Debugging: bytes rewritten on rejection
        }
    }

    alarm(0);
    free(stuffed_frame);

    if (accepted) {
        printf("Frame delivered with success!\n\n");
        return inf_frame_size;
    } else {
        printf("Frame could not be delivered..\n\n");
        return -1;
    }

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    unsigned char byte; 
    unsigned char c = 0;
    int x = 0;
    ReceiverState state = START_R;

    while (state != STOP_RCV) {
        if (readByteSerialPort(&byte)) {
            //printf("0x%02X ", byte);
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
                    if (byte == C_N(0) || byte == C_N(1)/* || byte == DISC*/) {
                        state = C_RCV;
                        c = byte;
                    } 
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START_R;
                    break;

                case C_RCV:
                    if (byte == (ADDRESS_SENT_TRANSMITTER ^ c)) {
                        state = READ_DATA;
                        if ((c == C_N(0) && frameNumberR == 1) || (c == C_N(1) && frameNumberR == 0)) {
                            state = STOP_RCV;
                            unsigned char cResponse = frameNumberR == 0 ? RR0 : RR1;
                            unsigned char supervisionFrame[FRAME_SIZE] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};

                            int bytesW = writeBytesSerialPort(supervisionFrame, FRAME_SIZE);
                            printf("\nReceived duplicated data frame\n%d positive response bytes written\n", bytesW);
                            duplicateFrames++;
                            return 0;
                        }
                        //else if (c == DISC) state = DISC_RCV;
                    } else if (byte == FLAG) state = FLAG_RCV;
                    else state = START_R;
                    break;

                /*case DISC_RCV:
                    if (byte == FLAG) {
                        closeReceiver();
                    }
                    else {
                        state = START_R;
                    }
                    */
                case READ_DATA:
                    if (byte == ESC) state = ESC_FOUND;
                    else if (byte == FLAG) {
                        unsigned char bcc2 = packet[--x];
                        printf("\nFrame BCC2 = 0x%02X\n", bcc2);

                        unsigned char acc = 0;
                        for (unsigned int i = 0; i < x; i++) {
                            acc ^= packet[i];
                            //printf("0x%02X ", packet[i]); // Debugging: packet content
                        }

                        printf("\nCalculated BCC2 = 0x%02X\n", acc);

                        if (bcc2 == acc) {
                            framesReceived++;
                            state = STOP_RCV;
                            unsigned char cResponse = frameNumberR == 0 ? RR1 : RR0;
                            unsigned char supervisionFrame[FRAME_SIZE] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};

                            int bytesW = writeBytesSerialPort(supervisionFrame, FRAME_SIZE);
                            printf("\nGot new frame!\n%d positive response bytes written\n", bytesW);
                            frameNumberR = (frameNumberR + 1) % 2;
                            return x;
                        } else {
                            if ((c == C_N(0) && frameNumberR == 1) || (c == C_N(1) && frameNumberR == 0)) {
                                state = STOP_RCV;
                                unsigned char cResponse = frameNumberR == 0 ? RR0 : RR1;
                                unsigned char supervisionFrame[FRAME_SIZE] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};

                                int bytesW = writeBytesSerialPort(supervisionFrame, FRAME_SIZE);
                                printf("\n%d Error in frame data but is a duplicated frame.\n positive response bytes written\n", bytesW);
                                free(packet);
                                packet = NULL;
                                packet = (unsigned char *)malloc(PAYLOAD_SIZE_500);
                                return 0;
                            } else {
                                printf("\nError in data, asking for retransmission\n");
                                unsigned char cResponse = frameNumberR == 0 ? REJ0 : REJ1;
                                unsigned char supervisionFrame[FRAME_SIZE] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse};
                                int bytesW = writeBytesSerialPort(supervisionFrame, FRAME_SIZE);
                                framesRejected++;
                                printf("\n%d negative response bytes written\n", bytesW);
                                /*state = START_R;
                                x = 0;*/
                                free(packet);
                                packet = NULL;
                                packet = (unsigned char *)malloc(PAYLOAD_SIZE_500);
                                return -1;
                            }
                        }
                    } else {
                        packet[x++] = byte;
                    }
                    break;

                case ESC_FOUND:
                    packet[x++] = byteDestuff(byte);
                    state = READ_DATA;
                    break;

                case STOP_RCV:
                    break;            

                default:
                    state = START_R;
                    break;
            }
        }
    }

    return -1;
}

unsigned char byteDestuff(unsigned char byte) {
    if (byte == (FLAG ^ 0x20)) {
        return FLAG;
    }
    else if (byte == (ESC ^ 0x20)) {
        return ESC;
    }
    return byte;
}

int closeReceiver() {

    unsigned char supervisionFrame[FRAME_SIZE] = {FLAG, ADDRESS_SENT_RECEIVER, DISC, ADDRESS_SENT_RECEIVER ^ DISC, FLAG};

    ReceiverState receiverState = START_R;

    alarmEnabled = FALSE;
    alarmCount = 0;
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount <= retransmissions && receiverState != STOP_RCV) {

        if (!alarmEnabled) {
            int bytes = writeBytesSerialPort(supervisionFrame, FRAME_SIZE);
            printf("\n%d DISC bytes written to transmitter\n\n", bytes);
            alarm(timeout);
            
            alarmEnabled = TRUE;
        }

        unsigned char response_byte;

        if (readByteSerialPort(&response_byte)) {
            //printf("0x%02X ", response_byte);
            switch(receiverState) {
                case START_R:
                    //printf("start\n");
                    if (response_byte == FLAG) receiverState = FLAG_RCV;
                    else receiverState = START_R;
                    break;
                case FLAG_RCV:
                    //printf("flag\n");
                    if (response_byte == ADDRESS_ANSWER_TRANSMITTER) receiverState = A_RCV;
                    else if (response_byte == FLAG) receiverState = FLAG_RCV;
                    else receiverState = START_R;
                    break;
                case A_RCV:
                    //printf("A\n");
                    if (response_byte == CONTROL_UA) receiverState = C_RCV;
                    else if (response_byte == FLAG) receiverState = FLAG_RCV;
                    else receiverState = START_R;
                    break;
                case C_RCV:
                    //printf("C\n");
                    if (response_byte == (ADDRESS_ANSWER_TRANSMITTER ^ CONTROL_UA)) receiverState = BCC_OK_R;
                    else if (response_byte == FLAG) receiverState = FLAG_RCV;
                    else receiverState = START_R;
                    break;
                case BCC_OK_R:
                    //printf("BCC\n");
                    if (response_byte == FLAG) {
                        alarm(0);
                        receiverState = STOP_RCV;
                    }
                    else receiverState = START_R;
                    break;
                case STOP_RCV:
                    break;
                default:
                    receiverState = START_S;
                    break;
            }
        }
    }

    alarm(0);

    if (receiverState == STOP_RCV) {
        printf("Received UA frame successfully\n\n");
        return 1;
    }

    else {
        printf("Did not receive UA from transmitter\n\n");
        return -1;
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    unsigned char byte; 
    ReceiverState state = START_R;

    switch (isTx) {

    case TRUE:

        (void)signal(SIGALRM, alarmHandler);

        unsigned char bufS[5] = {FLAG, ADDRESS_SENT_TRANSMITTER, DISC, ADDRESS_SENT_TRANSMITTER ^ DISC, FLAG};

        SenderState senderState = START_S;

        alarmCount = 0;

        alarmEnabled = FALSE;

        while (alarmCount <= retransmissions && senderState != STOP_SDR) {
            if (!alarmEnabled) {
                int bytes = writeBytesSerialPort(bufS,FRAME_SIZE);
                printf("\n%d DISC command bytes written to receiver\n\n", bytes);
                alarm(timeout);
                alarmEnabled = TRUE;
            }

            unsigned char response_byte;

            if (readByteSerialPort(&response_byte)) {
                //printf("Received byte: 0x%02X\n", response_byte);
                switch(senderState) {
                    case START_S:
                        //printf("start\n");
                        if (response_byte == FLAG)  senderState = FLAG_SDR;
                        else senderState = START_S;
                        break;
                    case FLAG_SDR:
                        //printf("flag\n");
                        if (response_byte == ADDRESS_SENT_RECEIVER) senderState = A_SDR;
                        else if (response_byte == FLAG) senderState = FLAG_SDR;
                        else senderState = START_S;
                        break;
                    case A_SDR:
                        //printf("A\n");
                        if (response_byte == DISC) senderState = C_SDR;
                        else if (response_byte == FLAG) senderState = FLAG_SDR;
                        else senderState = START_S;
                        break;
                    case C_SDR:
                        //printf("C\n");
                        if (response_byte == (ADDRESS_SENT_RECEIVER ^ DISC)) senderState = BCC_OK_S;
                        else if (response_byte == FLAG) senderState = FLAG_SDR;
                        else senderState = START_S;
                        break;
                    case BCC_OK_S:
                        //printf("BCC\n");
                        if (response_byte == FLAG) {
                            alarm(0);
                            senderState = STOP_SDR;
                            printf("Received DISC acknowledgment\n\n");
                        }
                        else senderState = START_S;
                        break;
                    case STOP_SDR:
                        break;
                    default:
                        senderState = START_S;
                        break;
                }
            } 
        }

        alarm(0);

        if (senderState == STOP_SDR) {
            printf("Read DISC frame successfully\n\n");
            unsigned char uaFrame[FRAME_SIZE] = {FLAG, ADDRESS_ANSWER_TRANSMITTER, CONTROL_UA, ADDRESS_ANSWER_TRANSMITTER ^ CONTROL_UA, FLAG};
            writeBytesSerialPort(uaFrame, FRAME_SIZE);
            printf("Sent UA frame\n\nDisconnect completed!\n\n");
        }
        else printf("Did not receive DISC command from receiver (retry limit reached)\n\n");

        clock_t endTime = clock();
        double elapsedTime = (double)(endTime - startTime) / CLOCKS_PER_SEC;

        if (showStatistics) {
            printf("Communication Statistics:\n");
            printf("Data Frames Sent: %d\n", framesSent);
            printf("Number of duplicate frames received: %d\n", duplicateFrames);
            printf("Number of timeouts: %d\n", timeouts);
            printf("Total execution time: %.2f seconds\n", elapsedTime);
        }
        
        break;

    case FALSE: 

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
                    if (byte == DISC) state = C_RCV;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START_R;
                    break;
                
                case C_RCV:
                    if (byte == (ADDRESS_SENT_TRANSMITTER ^ DISC)) {
                        state = DISC_RCV;
                    }
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START_R;
                    break;

                case DISC_RCV:
                    if (byte == FLAG) {
                        printf("\nDISC command received from transmitter\n\n");
                        state = STOP_RCV;
                    }
                    else {
                        state = START_R;
                    }
                    break;

                case STOP_RCV:
                    break;            

                default:
                state = START_R;
                    break;
                }
            }
        }

        if (state == STOP_RCV) {
            printf("Sending DISC to transmitter\n");
            if (closeReceiver() == -1) {
                printf("Error on disconecting\n\n");
                return -1;
            }
        }

        
        if (showStatistics) {
            printf("Communication Statistics:\n");
            printf("Data Frames Received Sucessfully: %d\n", framesReceived);
            printf("Frames rejected: %d\n", framesRejected);
        }

        break;
    default:
        break;
    }


    int clstat = closeSerialPort();
    return clstat;
}

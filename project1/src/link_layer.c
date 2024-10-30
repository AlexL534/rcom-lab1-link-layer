// Link layer protocol implementation

#include "link_layer.h"

volatile int STOP = FALSE;

unsigned char frameNumberT = 0;
unsigned char frameNumberR = 0;

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
                    if (byte == RR0 || byte == RR1 || byte == REJ0 || byte == REJ1 || byte == DISC) {
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
            alarmCount = 0;
            alarmEnabled = FALSE;

            while (alarmCount <= retransmissions && senderState != STOP_SDR) {
                if (alarmEnabled == FALSE)
                {
                    int bytes = writeBytesSerialPort(bufS,5);
                    // bufS[2] = CONTROL_SET;                            //SO PARA TESTE
                    sleep(1); 
                    printf("%d bytes written\n", bytes);
                    alarm(timeout); // Set alarm to be triggered in 3s
                    
                    alarmEnabled = TRUE;
                }

                unsigned char response_byte;

                if (readByteSerialPort(&response_byte)) {
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
                            else if (response_byte == FLAG) {
                                
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
                                alarm(0);
                            }
                            else {

                                senderState = START_S;
                            }
                            break;
                        default:
                            senderState = START_S;
                            break;
                    }
                }
            }

            alarm(0);

            if (senderState == STOP_SDR) {
                printf("STOP\n");
                printf("Received UA frame successfully.\n");
                return 1;
            }

            else {
                printf("No response from receiver\n");
                printf("Canceling operation..\n");
                return -1;
            }
            break;
        }

        case LlRx: {
            printf("New termios structure set\n");

            // Loop for input
            unsigned char byte2;
            ReceiverState ReceiverState = START_R;
            while (ReceiverState != STOP_RCV) {
                if (readByteSerialPort(&byte2)) {
                    //printf(":%s:%d\n", buf, bytes); //prints frame received
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
                            break;
                        default:
                            ReceiverState = START_R;
                            break;
                    }
                }
            }
            printf("STOP\n");
            unsigned char uaFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, CONTROL_UA, ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA, FLAG};
            writeBytesSerialPort(uaFrame, 5);
            printf("Sent UA frame\n");
            return 1;
            break;
        }

        default:
            break;
    }
return -1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
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

    //BCC2 = 0x06;                                // TESTAR
    printf("BCC2 = 0x%02X\n", BCC2);            // PARA TESTAR BCC

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
    
    printf("After Stuff I frame:\n");
    for (int i = 0; i < j; i++) {
        printf("0x%02X ", stuffed_frame[i]);
    }
    printf("\n");

    //return -1; //                           PARA TESTE

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
            sleep(1);
            printf("%d bytes written\n", bytesW);
            alarm(timeout);
        }

        unsigned char command = checkControl();
        printf("command = 0x%02X\n", command); //              TESTAR COMMAND

        if ((command == REJ0 && frameNumberT == 0) || (command == REJ1 && frameNumberT == 1)) {
            rejected = 1;
        }

        else if ((command == RR0 && frameNumberT == 1) || (command == RR1 && frameNumberT == 0)) {
            accepted = 1;
            frameNumberT = (frameNumberT + 1) % 2;
            printf("\nFrame number = 0x%02X\n", frameNumberT); //        PARA TESTE
            
        }

        if (accepted) {
            alarm(0);
            break;
        }
        else if (rejected) {
            current_transmission++;
            //stuffed_frame[j-2] = 0x07; //                                   PARA TESTE
            int bytesW = writeBytesSerialPort(stuffed_frame, j);
            sleep(1);
            printf("%d bytes rewritten\n", bytesW);
        }
    }

    alarm(0);

    free(stuffed_frame);
    if (accepted) {
        printf("Frame delivered with success\n");
        //printf("Frame Size = %d\n", inf_frame_size); //             PARA TESTE
        return inf_frame_size;
    }
    else {
        printf("Frame could not be delivered\n");
        return -1;
    }

    return -1;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char byte; 
    unsigned char c = 0;
    int x = 0;
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
                if (byte == C_N(0) || byte == C_N(1) || byte == DISC) {
                    state = C_RCV;
                    c = byte;
                }
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START_R;
                break;
            
            case C_RCV:
                if (byte == (ADDRESS_SENT_TRANSMITTER ^ c)) {
                    state = c == DISC ? DISC_RCV : READ_DATA;
                    if ((c == C_N(0) && frameNumberR == 1) || (c == C_N(1) && frameNumberR == 0)) {
                        state = STOP_RCV;
                        unsigned char cResponse = frameNumberR == 0 ? RR0 : RR1;
                        unsigned char supervisionFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};
                        
                        for (int i = 0; i < 5; i++) {
                            printf("0x%02X ", supervisionFrame[i]);
                        }
                        printf("\nFrame number = 0x%02X\n", frameNumberR);      //TESTE
                        
                        int bytesW = writeBytesSerialPort(supervisionFrame, 5);
                        sleep(1);
                        printf("%d Duplicate frame, positive response bytes written\n", bytesW);
                        return 0;
                    }
                }
                else if (byte == FLAG) state = FLAG_RCV;
                else state = START_R;
                break;

            case DISC_RCV:
                printf("Disc received\n");
                if (byte == FLAG) {
                    printf("Sending DISC to transmitter\n");
                    if (closeReceiver() == 1) return 5;
                    else return -1;
                }
                else {
                    state = START_R;
                }
                break;

            case READ_DATA:
                if (byte == ESC) state = ESC_FOUND;
                else if (byte == FLAG) {
                    unsigned char bcc2 = packet[--x];
                    printf("BCC2 = 0x%02X\n", bcc2);

                    unsigned char acc = 0;
                    for (unsigned int i = 0; i < x; i++) {
                        acc ^= packet[i];
                        printf("0x%02X ", packet[i]);
                    }

                    printf("\nACC2 = 0x%02X\n", acc);

                    if (bcc2 == acc) {
                        state = STOP_RCV;
                        unsigned char cResponse = frameNumberR == 0 ? RR1 : RR0;
                        unsigned char supervisionFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};
                        
                        for (int i = 0; i < 5; i++) {
                            printf("0x%02X ", supervisionFrame[i]);
                        }
                        printf("\nFrame number = 0x%02X\n", frameNumberR);      //TESTE
                        
                        int bytesW = writeBytesSerialPort(supervisionFrame, 5);
                        sleep(1);
                        printf("%d positive response bytes written\n", bytesW);
                        frameNumberR = (frameNumberR + 1) % 2;
                        printf("\nFrame number = 0x%02X\n", frameNumberR);      //TESTE
                        return x;
                    }
                    else {
                        if ((c == C_N(0) && frameNumberR == 1) || (c == C_N(1) && frameNumberR == 0)) {
                            state = STOP_RCV;
                            unsigned char cResponse = frameNumberR == 0 ? RR0 : RR1;
                            unsigned char supervisionFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse, FLAG};
                            
                            for (int i = 0; i < 5; i++) {
                                printf("0x%02X ", supervisionFrame[i]);
                            }
                            printf("\nFrame number = 0x%02X\n", frameNumberR);      //TESTE
                            
                            int bytesW = writeBytesSerialPort(supervisionFrame, 5);
                            sleep(1);
                            printf("%d Error in data but duplicate frame, positive response bytes written\n", bytesW);
                            return 0;
                        }
                        else {
                            printf("Error in data, asking for retransmission\n");
                            unsigned char cResponse = frameNumberR == 0 ? REJ0 : REJ1;
                            unsigned char supervisionFrame[5] = {FLAG, ADDRESS_ANSWER_RECEIVER, cResponse, ADDRESS_ANSWER_RECEIVER ^ cResponse};
                            int bytesW = writeBytesSerialPort(supervisionFrame, 5);
                            sleep(1);
                            printf("%d negative response bytes written\n", bytesW);
                            state = START_R;
                            x = 0;
                        }
                    }
                }
                else {
                    packet[x++] = byte;
                }
                break;

            case ESC_FOUND:
                /*if (byte == (FLAG ^ 0x20)) {
                    packet[x++] = FLAG;
                }
                else if (byte == (ESC ^ 0x20)) {
                    packet[x++] = ESC;
                }*/
                /*else {
                    state = START_R;
                }*/
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

    unsigned char supervisionFrame[5];
    supervisionFrame[0] = FLAG;
    supervisionFrame[1] = ADDRESS_SENT_RECEIVER;
    supervisionFrame[2] = DISC;
    supervisionFrame[3] = ADDRESS_SENT_RECEIVER ^ DISC;
    supervisionFrame[4] = FLAG;

    ReceiverState receiverState = START_R;

    alarmEnabled = FALSE;
    alarmCount = 0;
    (void)signal(SIGALRM, alarmHandler);

    while (alarmCount <= retransmissions && receiverState != STOP_RCV) {

        if (alarmEnabled == FALSE) {
            int bytes = writeBytesSerialPort(supervisionFrame,5);
            sleep(1); 
            printf("%d DISC bytes written to transmitor\n", bytes);
            alarm(timeout); // Set alarm to be triggered in 3s
            
            alarmEnabled = TRUE;
        }

        unsigned char response_byte;

        if (readByteSerialPort(&response_byte)) {
            printf("0x%02X ", response_byte);
            switch(receiverState) {
                case START_R:
                    printf("start\n");
                    if (response_byte == FLAG) {
                        receiverState = FLAG_RCV;
                    }
                    else {
                        receiverState = START_R;
                    }
                    break;
                case FLAG_RCV:
                    printf("flag\n");
                    if (response_byte == ADDRESS_ANSWER_TRANSMITTER) {
                    
                        receiverState = A_RCV;
                    }
                    else if (response_byte == FLAG) {
                    
                        receiverState = FLAG_RCV;
                    }
                    else {
                        receiverState = START_R;
                    }
                    break;
                case A_RCV:
                    printf("A\n");
                    if (response_byte == CONTROL_UA) {
                        receiverState = C_RCV;
                    }
                    else if (response_byte == FLAG) {
                    
                        receiverState = FLAG_RCV;
                    }
                    else {
                        receiverState = START_R;
                    }
                    break;
                case C_RCV:
                    printf("C\n");
                    if (response_byte == (ADDRESS_ANSWER_TRANSMITTER ^ CONTROL_UA)) {
                        receiverState = BCC_OK_R;
                    }
                    else if (response_byte == FLAG) {
                        
                        receiverState = FLAG_RCV;
                    }
                    else {
                        
                        receiverState = START_R;
                    }
                    break;
                case BCC_OK_R:
                    printf("BCC\n");
                    if (response_byte == FLAG) {
                        receiverState = STOP_RCV;
                    }
                    else {

                        receiverState = START_R;
                    }
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
        printf("STOP\n");
        printf("Received UA frame successfully.\n");
        return 1;
    }

    else {
        printf("Did not receiver UA from transmitor\nError in disconecting\n");
        return -1;
    }

    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    (void)signal(SIGALRM, alarmHandler);

    unsigned char bufS[5] = {FLAG, ADDRESS_SENT_TRANSMITTER, DISC, ADDRESS_SENT_TRANSMITTER ^ DISC, FLAG};

    SenderState senderState = START_S;

    alarmCount = 0;
    printf("AQUI 1\n");

    alarmEnabled = FALSE;

    while (alarmCount <= retransmissions && senderState != STOP_SDR) {
        if (alarmEnabled == FALSE)
        {
            int bytes = writeBytesSerialPort(bufS,5);
            //sleep(1);
            printf("%d bytes written\n", bytes);
            alarm(timeout); // Set alarm to be triggered in 3s
            
            alarmEnabled = TRUE;
        }

        unsigned char response_byte = readByteSerialPort(&response_byte);

        if (response_byte > 0) {
            printf("0x%02X ", response_byte);
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
                    if (response_byte == ADDRESS_SENT_RECEIVER) {
                    
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
                    if (response_byte == DISC) {
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
                    if (response_byte == (ADDRESS_SENT_RECEIVER ^ DISC)) {
                        senderState = BCC_OK_S;
                    }
                    else if (response_byte == FLAG) {
                        
                        senderState = FLAG_SDR;
                    }
                    else {
                        
                        senderState = START_S;
                    }
                    break;
                case BCC_OK_S:
                    printf("BCC\n");
                    if (response_byte == FLAG) {
                        
                        alarm(0);
                        
                        senderState = STOP_SDR;
                    }
                    else {

                        senderState = START_S;
                    }
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
        printf("STOP\n");
        printf("Read DISC frame successfully.\n");
        unsigned char uaFrame[5] = {FLAG, ADDRESS_ANSWER_TRANSMITTER, CONTROL_UA, ADDRESS_ANSWER_TRANSMITTER ^ CONTROL_UA, FLAG};
        writeBytesSerialPort(uaFrame, 5);
        sleep(1);
        printf("Sent UA frame\n");
    }

    printf("Ending program\n");

    int clstat = closeSerialPort();
    return clstat;
}

// Link layer header.
// NOTE: This file must not be changed.

#ifndef _LINK_LAYER_H_
#define _LINK_LAYER_H_

#include "serial_port.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#define BAUDRATE B38400
//MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define FRAME_SIZE 5

#define FLAG 0x7E
#define ADDRESS_SENT_TRANSMITTER 0x03
#define ADDRESS_ANSWER_RECEIVER 0x03
#define ADDRESS_SENT_RECEIVER 0X01
#define ADDRESS_ANSWER_TRANSMITTER 0X01
#define CONTROL_SET 0X03
#define CONTROL_UA 0X07
#define C_N(Ns) ((Ns) << 6)
#define ESC 0x7D
#define RR0 0xAA
#define RR1 0xAB
#define REJ0 0X54
#define REJ1 0X55
#define DISC 0X0B

#define ALARM_MAX_RETRIES 4

typedef enum {
    START_R,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK_R,
    STOP_RCV,
    READ_DATA,
    ESC_FOUND,
} ReceiverState;

typedef enum {
    START_S,
    FLAG_SDR,
    A_SDR,
    C_SDR,
    BCC_OK_S,
    STOP_SDR,
} SenderState;

typedef enum
{
    LlTx,
    LlRx,
} LinkLayerRole;

typedef struct
{
    char serialPort[50];
    LinkLayerRole role;
    int baudRate;
    int nRetransmissions;
    int timeout;
} LinkLayer;



// SIZE of maximum acceptable payload.
// Maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// MISC
#define FALSE 0
#define TRUE 1

// Open a connection using the "port" parameters defined in struct linkLayer.
// Return "1" on success or "-1" on error.
int llopen(LinkLayer connectionParameters);

// Send data in buf with size bufSize.
// Return number of chars written, or "-1" on error.
int llwrite(const unsigned char *buf, int bufSize);

// Receive data in packet.
// Return number of chars read, or "-1" on error.
int llread(unsigned char *packet);

// Close previously opened connection.
// if showStatistics == TRUE, link layer should print statistics in the console on close.
// Return "1" on success or "-1" on error.
int llclose(int showStatistics);

unsigned char checkControl();

#endif // _LINK_LAYER_H_

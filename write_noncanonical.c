
// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

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

#define BUF_SIZE 256

volatile int STOP = FALSE;

typedef enum {
    START,
    FLAG_SDR,
    A_SDR,
    C_SDR,
    BCC_OK,
    STOP_SDR,
} SenderState;

int alarmEnabled = FALSE;
int responseReceived = FALSE;
int alarmCount = 0;

unsigned char frameNumberT = 0;

// Alarm function handler
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
}


int main(int argc, char *argv[]) {
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    (void)signal(SIGALRM, alarmHandler);

    unsigned char buf[6] = {FLAG, ADDRESS_SENT_TRANSMITTER, CONTROL_SET, ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET, FLAG, '\0'};

    unsigned char response[BUF_SIZE] = {0};
    int response_bytes = 0;
    SenderState state = START;

    while (alarmCount < ALARM_MAX_RETRIES && !responseReceived) {
        if (alarmEnabled == FALSE)
        {
            int bytes = write(fd, buf, 6);
            sleep(1); //this sleep is important
            printf("%d bytes written\n", bytes);
            alarm(3); // Set alarm to be triggered in 3s
            
            alarmEnabled = TRUE;
        }

        response_bytes = read(fd, response, BUF_SIZE);

        if (response_bytes > 0) {
            for (int i = 0; i <= response_bytes; i++) {
                switch(state) {
                    case START:
                        printf("start\n");
                        if (response[i] == FLAG) {
                            state = FLAG_SDR;
                        }
                        else {
                            state = START;
                        }
                        break;
                    case FLAG_SDR:
                        printf("flag\n");
                        if (response[i] == ADDRESS_ANSWER_RECEIVER) {
                          
                            state = A_SDR;
                        }
                        else if (response[i] == FLAG) {
                           
                            state = FLAG_SDR;
                        }
                        else {
                            state = START;
                        }
                        break;
                    case A_SDR:
                        printf("A\n");
                        if (response[i] == CONTROL_UA) {
                            state = C_SDR;
                        }
                        else if (response[i] == FLAG) {
                          
                            state = FLAG_SDR;
                        }
                        else {
                            state = START;
                        }
                        break;
                    case C_SDR:
                        printf("C\n");
                        if (response[i] == (ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA)) {
                            state = BCC_OK;
                        }
                        else if (response[i] == CONTROL_UA) {
                            
                            state = FLAG_SDR;
                        }
                        else {
                              
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        printf("BCC\n");
                        if (response[i] == FLAG) {
                             
                            state = STOP_SDR;
                        }
                        else {

                            state = START;
                        }
                        break;
                    case STOP_SDR:
                        printf("STOP\n");
                        printf("Received UA frame successfully.\n");
                        responseReceived = TRUE;
                        alarm(0);
                        break;
                    default:
                        state = START;
                        break;
                }
            }
        }
        else {
            //printf("No frame received.\n");
        }
    }

    //Start of Stop and Wait !!!!!!!!
    unsigned char buf2[256];
    int bufsize = 256; //random value, bufsize is needed because it is one of the arguments of llwrite
    int inf_frame_size = 6 + bufsize;
    unsigned char *frame = (unsigned char *) malloc(inf_frame_size);

    frame[0] = FLAG;
    frame[1] = ADDRESS_SENT_TRANSMITTER;
    frame[2] = C_N(frameNumberT);
    frame[3] = ADDRESS_SENT_TRANSMITTER ^ C_N(frameNumberT);

    memcpy(frame+4,buf2, bufsize);
    unsigned char BCC2 = 0;
    for (unsigned int i = 0; i < bufsize; i++) {
        BCC2 ^= buf2[i]; // doing XOR of each byte with BCC2
    }
    
    if (responseReceived) {
        int j = 4;
        for (int i = 0; i < bufsize; i++) {
            if (buf2[i] == FLAG || buf2[i] == ESC) {
                frame = realloc(frame, inf_frame_size++);
                frame[j++] = ESC; // Stuff with ESC byte
            }
            frame[j++] = buf2[i];
        }
        frame[j++] = BCC2;
        frame[j++] = FLAG;
    }

    printf("Ending program\n");

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}

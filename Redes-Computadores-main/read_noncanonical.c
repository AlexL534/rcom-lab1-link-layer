
    // Read from serial port in non-canonical mode
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

    #define BUF_SIZE 256

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

    volatile int STOP = FALSE;

    typedef enum {
        START,
        FLAG_RCV,
        A_RCV,
        C_RCV,
        BCC_OK,
        STOP_RCV,
    } ReceiverState;

    unsigned char frameNumberR = 1;

    //this function probably needs to be changed
    void byteDestuff(unsigned char *input, int inputSize, unsigned char *output, int *outputSize) {
    int j = 0;
    for (int i = 0; i < inputSize; i++) {
        if (input[i] == ESC) {
            i++; // Move to the next byte
            output[j++] = input[i] ^ 0x20; // Undo the XOR to get the original byte
        } else {
            output[j++] = input[i];
        }
    }
    *outputSize = j; // Set the size of the output
}

    int main(int argc, char *argv[])
    {
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

        // Open serial port device for reading and writing and not as controlling tty
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

        // Loop for input
        unsigned char buf[BUF_SIZE + 1] = {0}; // +1: Save space for the final '\0' char
        ReceiverState state = START;
        while (STOP == FALSE)
        {
            // Returns after 5 chars have been input
            int bytes = read(fd, buf, BUF_SIZE);
            buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

            //printf(":%s:%d\n", buf, bytes); //prints frame received
            if (bytes > 0) {
                for (int i = 0; i < bytes; i++) {
                    if (STOP == TRUE) break; 
                    switch(state) {
                        case START:
                        printf("start\n");
                            if (buf[i] == FLAG) {
                                state = FLAG_RCV;
                            }
                            else {
                                state = START;
                            }
                            break;
                        case FLAG_RCV:
                            printf("flag\n");
                            if (buf[i] == ADDRESS_SENT_TRANSMITTER) {
                            
                                state = A_RCV;
                            }
                            else if (buf[i] == FLAG) {
                            
                                state = FLAG_RCV;
                            }
                            else {
                                state = START;
                            }
                            break;
                        case A_RCV:
                            printf("A\n");
                            if (buf[i] == CONTROL_SET) {
                                state = C_RCV;
                            }
                            else if (buf[i] == FLAG) {
                            
                                state = FLAG_RCV;
                            }
                            else {
                                state = START;
                            }
                            break;
                        case C_RCV:
                            printf("C\n");
                            if (buf[i] == (ADDRESS_SENT_TRANSMITTER ^ CONTROL_SET)) {
                                state = BCC_OK;
                            }
                            else if (buf[i] == CONTROL_SET) {
                                
                                state = FLAG_RCV;
                            }
                            else {
                                
                                state = START;
                            }
                            break;
                        case BCC_OK:
                            printf("BCC\n");
                            if (buf[i] == FLAG) {
                                
                                state = STOP_RCV;
                            }
                            else { 

                                state = START;
                            }
                            break;
                        case STOP_RCV:
                            printf("STOP\n");
                            unsigned char uaFrame[6] = {FLAG, ADDRESS_ANSWER_RECEIVER, CONTROL_UA, ADDRESS_ANSWER_RECEIVER ^ CONTROL_UA, FLAG, '\0'};
                            write(fd, uaFrame, 6);
                            printf("Sent UA frame\n");
                            STOP = TRUE;
                            break;
                        default:
                            state = START;
                            break;
                    }
                }
            }
        }

        //Start of Stop and Wait !!!!!!!!


        // The while() cycle should be changed in order to respect the specifications
        // of the protocol indicated in the Lab guide

        // Restore the old port settings
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
        {
            perror("tcsetattr");
            exit(-1);
        }

        close(fd);

        return 0;
    }

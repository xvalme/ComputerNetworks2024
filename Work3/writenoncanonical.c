/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

#define DATA_BUFFER_SIZE 1024

typedef enum {
    Start_State,
    Flag_RCV_State,
    A_RCV_State,
    C_RCV_State,
    BCC_OK_State,
    Stop_State,
    DATA_RCV_State,
    BCC2_OK_State
} StateMachine;

int handshake(int fd) {
    fprintf(stderr, "[INFO] Initializing Handshake \n");

	const char flag = 0x5c;
	const char address = 0x03;
	const char control = 0x08;
	const char control_ua = 0x06;

	const char bcc1 = address^control;
	const char bcc1_ua = address^control_ua;
	
    char buffer[5]= {flag, address, control, bcc1, flag};

    int res = write(fd,buffer,5);

    if (res == 5) {
        fprintf(stderr, "[INFO] Sent SET packet\n");
    } else {
        fprintf(stderr, "[ERR] Error sending SET packet\n");
        return -1;
    }

    char buf[6];
	
    StateMachine state = Start_State;
    while (STOP==FALSE || state != Stop_State) {       /* loop for input */
        res = read(fd,buf,1);   /* returns after 5 chars have been input */
        buf[res]=0;               /* so we can printf... */
        // fprintf(stderr, "[INFO] Received byte: %x, %d\n", buf[0], state);
        char received_correct = 1;
    
        switch (state) {
        case Start_State:
            if (buf[0] == flag) {
                state = Flag_RCV_State;
            } else {
                state = Start_State;
            }
            break;

        case Flag_RCV_State:
            if (buf[0] == address) {
                state = A_RCV_State;
            } else if (buf[0] == flag) {
                state = Flag_RCV_State;
            } else {
                state = Start_State;
            }
            break;

        case A_RCV_State:
            if (buf[0] == control_ua) {
                state = C_RCV_State;
            } else if (buf[0] == flag) {
                state = Flag_RCV_State;
            } else {
                state = Start_State;
            }
            break;

        case C_RCV_State:
            if (buf[0] == bcc1_ua) {
                state = BCC_OK_State;
            } else if (buf[0] == flag) {
                state = Flag_RCV_State;
            } else {
                state = Start_State;
            }
            break;

        case BCC_OK_State:
            if (buf[0] == flag) {
                state = Stop_State;
                fprintf(stderr, "[INFO] Connection established\n");
                return 0;
            } else {
                state = Start_State;
            }
            break;
        }	
    }
    return 0;
}

int receive_data(int fd) {
    const char flag = 0x5c;
	const char address = 0x03;
	const char control = 0x08;
	const char control_ua = 0x06;

	const char bcc1 = address^control;  
	const char bcc1_ua = address^control_ua;

    char buf[1];
    StateMachine state = Start_State;
    int consecutive_flags = 0;
    char received_data[DATA_BUFFER_SIZE];
    int data_index = 0;

	
    while (STOP==FALSE || state != Stop_State) {       /* loop for input */
        int res = read(fd,buf,1);   /* returns after 5 chars have been input */
        if (res < 0) {
            fprintf(stderr, "[ERR] Error reading from serial port\n");
            return -1;
        }
        // char received_correct = 1;
        if (buf[0] == flag) {
            consecutive_flags++;
        } else {
            consecutive_flags = 0;
        }

        if (consecutive_flags == 2) {
            // two consecutive flags, end of data
            fprintf(stderr, "[INFO] Received end of data\n");
            break;
        }
    
        switch (state) {
            case Start_State:
                if (buf[0] == flag) {
                    state = Flag_RCV_State;
                }
                break;

            case Flag_RCV_State:
                if (buf[0] == address) {
                    state = A_RCV_State;
                } else {
                    state = Start_State;
                }
                break;

            case A_RCV_State:
                if (buf[0] == control_ua) {
                    state = C_RCV_State;
                } else {
                    state = Start_State;
                }
                break;

            case C_RCV_State:
                if (buf[0] == bcc1_ua) {
                    state = BCC_OK_State;
                } else {
                    state = Start_State;
                }
                break;

            case BCC_OK_State:
                if (buf[0] == flag) {
                    state = DATA_RCV_State;
                } else {
                    state = Start_State;
                }
                break;

            case DATA_RCV_State:
                // Save received data
                if (buf[0] != flag) {
                    if (data_index < DATA_BUFFER_SIZE) {
                        received_data[data_index++] = buf[0];
                    } else {
                        fprintf(stderr, "[ERR] Received data too large\n");
                        return -1;
                    }
                } else {
                    state = BCC2_OK_State;
                }
                break;

            case BCC2_OK_State:
                if (buf[0] == flag) {
                    state = Stop_State;
                    // Print received data
                    printf("[INFO] Received data: %s\n", received_data);
                    return 0;
                } else {
                    state = Start_State;
                }
                break;

            default:
                break;
        }
    }
    return 0;
}

int send_data(int fd, const char* data, int data_length) {
    const char flag = 0x5c;
    const char address = 0x03;
    const char control = 0x08;

    char buffer[data_length + 5]; // Data length + 1 address byte + 1 control byte + 2 flag bytes
    buffer[0] = flag; // Start flag
    buffer[1] = address;
    buffer[2] = control;
    for (int i = 0; i < data_length; ++i) {
        buffer[i + 3] = data[i];
    }
    char bcc = address ^ control;
    for (int i = 0; i < data_length; ++i) {
        bcc ^= data[i];
    }
    buffer[data_length + 3] = bcc;
    buffer[data_length + 4] = flag; // End flag

    int res = write(fd, buffer, data_length + 5);
    if (res == data_length + 5) {
        fprintf(stderr, "[INFO] Sent data successfully\n");
        return 0;
    } else {
        fprintf(stderr, "[ERR] Error sending data\n");
        return -1;
    }
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS10", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS10\n");
        exit(1);
    }


    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */



    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */


    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");


    if (handshake(fd) != 0) {
        fprintf(stderr, "[ERR] Error in handshake\n");
        exit(-1);
    }

    const char data[] = "Hello, world!";
    if (send_data(fd, data, strlen(data)) != 0) {
        fprintf(stderr, "[ERR] Error in sending data\n");
        exit(-1);
    }

    if (receive_data(fd) != 0) {
        fprintf(stderr, "[ERR] Error in receiving data\n");
        exit(-1);
    }

    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }


    close(fd);
    return 0;
}

/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define SUCCESS 1
#define FAILURE -1


volatile int STOP=FALSE;

/* toggle the control bit value */
#define TOGGLE_CTRL(var) ((var) == 0 ? (var) = 1 : ((var) = 0))

#define DATA_BUFFER_SIZE 1024
#define DEBUG_ALL 1

int ctrl = 0;

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

typedef enum {
    Send0_State,
    ACK0_State,
    Send1_State,
    ACK1_State,
    Stop_SM_State
} senderSM;


enum ControlField{
    control_rr_field,
    control_rej_field,
    control_ua_field,
    control_disc_field
};



typedef struct linkLayer{
 char serialPort[50];
 int role; //defines the role of the program: 0==Transmitter,1=Receiver
 int baudRate;
 int numTries;
 int timeOut;
} linkLayer;
//ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1
//SIZE of maximum acceptable payload; maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000
//CONNECTION deafault values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

int fd;

int handshake(int fd) {
    fprintf(stderr, "[INFO] Initializing Handshake \n");

	const char flag = 0x5c;
	const char address = 0x01;
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
	const char address = 0x01;
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
int receive(int fd) {
    const char flag = 0x5c;
	const char address = 0x03;
	const char address_sender = 0x01;
	const char control_rr_1 = 0b00010001;
	const char control_rr_0 = 0b00000001;
    const char control_rej_1 = 0b00010101;
    const char control_rej_0 = 0b00000101;
	char control;
	const char control_ua = 0x06;
    const char control_disc = 0x0A;

    char current_ctrl = 0x80;

    bool rr = false;
    bool rej = false;
    bool disc = false;
    bool ua = false;

    int control_field;

    int ctrl_get = ctrl;

    char buf[1];
    StateMachine state = Start_State;
    int consecutive_flags = 0;
    char received_data[DATA_BUFFER_SIZE];
    int data_index = 0;

    fprintf(stderr, "[INFO] Initializing receiving \n");

    while (STOP==FALSE || state != Stop_State) {       /* loop for input */
        // fprintf(stderr, "[INFO] Initializing RR \n");
        int res = read(fd,buf,1);   /* returns after 5 chars have been input */
        // fprintf(stderr, "[INFO] Received byte: %x, %d\n", buf[0], state);
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
                // if (DEBUG_ALL) fprintf(stderr, "Buffer: %04x\n", buf[0]);
                if (buf[0] == control_rr_1) {
                    control = control_rr_1;
                    control_field = control_rr_field;
                    ctrl_get = 1;
                    rr = true;
                    state = C_RCV_State;
                } else if (buf[0] == control_rr_0) {
                    control = control_rr_0;
                    ctrl_get = 0;
                    control_field = control_rr_field;
                    rr = true;
                    state = C_RCV_State;
                } else if (buf[0] == control_rej_1) {
                    control = control_rej_1;
                    ctrl_get = 1;
                    rej = true;
                    control_field = control_rej_field;
                    state = C_RCV_State;
                } else if (buf[0] == control_rej_0) {
                    control = control_rej_0;
                    ctrl_get = 0;
                    rej = true;
                    control_field = control_rej_field;
                    state = C_RCV_State;
                } else if (buf[0] == control_ua) {
                    control = control_ua;
                    ua = true;
                    control_field = control_ua_field;
                    state = C_RCV_State;
                } else if (buf[0] == control_disc) {
                    control = control_disc;
                    disc = true;
                    control_field = control_disc_field;
                    state = C_RCV_State;
                } else if (buf[0] == flag) {
                    state = Flag_RCV_State;
                } else {
                    exit(-1);  
                    state = Start_State;
                }
                break;

            case C_RCV_State:
                if (buf[0] == (address^control))  {
                    state = BCC_OK_State;
                        fprintf(stderr, "[INFO] AAA\n");

                } else {
                    state = Start_State;
                }
                break;

            case BCC_OK_State:
                if (buf[0] == flag) {
                    if (ctrl_get != ctrl) return -1;

                    TOGGLE_CTRL(ctrl);
                    char buffer[5]= {flag, address_sender, current_ctrl, address_sender^current_ctrl, flag};
                    //int res = write(fd,buffer,5);
                    switch (control_field)
                    {
                    case control_ua_field:
                        /* code */
                        return control_ua_field;
                        break;
                    case control_disc_field:
                    
                        /* code */
                        return control_disc_field;
                        break;
                    case control_rr_field:
                        /* code */
                        fprintf(stderr, "[INFO] Received RR\n");
                        return control_rr_field;
                        break;
                    case control_rej_field:
                        fprintf(stderr, "[INFO] Received REJ\n");
                        /* code */
                        return control_rej_field;
                        break;
                    
                    default:
                        break;
                    }
                } else {
                    state = Start_State;
                }
                break;
            default:
                break;
        }
    }
    return -1;
}
char* byte_stuffing(const char* data, int data_length) {
    const char ESC = 0x1B;
    const char flag = 0x5c;
    char* new_data = (char*)malloc(sizeof(char) * (data_length * 2 + 1)); // Allocate memory for new_data

    if (new_data == NULL) {
        printf("Memory allocation failed");
        exit(1); // Exit if memory allocation fails
    }

    int i = 0;
    while (i <= data_length) { 
        if (data[i] == flag) {
            new_data[i] = ESC;
            i++;
            new_data[i] = 0x7C;
        } else if (data[i] == ESC) {
            new_data[i] = ESC;
            i++;
            new_data[i] = 0x7D;
        }else {
            new_data[i] = data[i];
        }
        i++;
    }
    new_data[i] = '\0'; // Null-terminate the new_data string
    return new_data;
}

int send_data(int fd, const char* data, int data_length, int ctrl) {
	char test_packet[data_length+6];
    const char flag = 0x5c;
    const char address = 0x01;
    
    char control;
	if (ctrl){
		control = 0xc0;
		}
    else {
		control = 0x80;
		}
    
    /* fill the packet */
    test_packet[0] = flag;
    test_packet[1] = address;
    test_packet[2] = control;
    test_packet[3] = address^control;

    char *new_data = byte_stuffing(data, data_length);

    int j = 0;
    while (new_data[j] != '\0') {
        test_packet[j+4] = new_data[j];
        j++;
    }

    // for (int i = 0; i < data_length; i++) {
    //     test_packet[i+4] = data[i];
    // }

    char bcc2 = ' ';
    for (int i = 4; i < data_length+4; i++) {
        bcc2 ^= test_packet[i];
    }

    test_packet[data_length+4] = bcc2;
    test_packet[data_length+5] = flag;

    for (int i = 0; i < sizeof(test_packet); i++) {
        if (DEBUG_ALL) fprintf(stderr, "%x ", test_packet[i]);
    }

    int res = write(fd,test_packet, sizeof(test_packet));
    
    if (res == sizeof(test_packet)) {
        fprintf(stderr, "[INFO] Sent dummy data packet\n");
    } else {
        fprintf(stderr, "[ERR] Error sending dummy data packet\n");
        return -1;
    }

    return res;
}
int disconnect(int fd, int ctrl) {
	char test_packet[5];
    const char flag = 0x5c;
    const char address = 0x01;
    const char control_ua = 0x06;
    
    char control_disc = 0x0A;
    
    /* fill the packet */
    test_packet[0] = flag;
    test_packet[1] = address;
    test_packet[2] = control_disc;
    test_packet[3] = address^control_disc;
    test_packet[4] = flag;

    for (int i = 0; i < sizeof(test_packet); i++) {
        if (DEBUG_ALL) fprintf(stderr, "%x ", test_packet[i]);
    }

    fprintf(stderr, "[INFO] Sending DISC\n");
    int res = write(fd,test_packet, sizeof(test_packet));
    
    if (res == sizeof(test_packet)) {
        fprintf(stderr, "[INFO] Sent DISC\n");
    } else {
        fprintf(stderr, "[ERR] Error sending DISC\n");
        return FAILURE;
    }
    
    // recive disc
    int control_code = receive(fd);


    if (control_code == control_disc_field) {
        fprintf(stderr, "[INFO] Received DISC\n");
    } else {
        fprintf(stderr, "[ERR] Error receiving DISC %d\n", control_code);
        return FAILURE;
    }

    // send ua
    char buffer[5]= {flag, address, control_ua, address^control_ua, flag};
    res = write(fd,buffer,5);


    return SUCCESS;
}

int send_msg(int fd, const char* msg) {
    int retr;

    // timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_DEFAULT;
    timeout.tv_usec = 0;
    fd_set readfds;
    int ready;

    int res = -1;


    senderSM state = Send0_State;
    while (state != Stop_SM_State) {
        // fprintf(stderr, "[DEBUGGGG] State: %d, msg %s\n", state, msg);

        switch (state)
        {
        case Send0_State:
            fprintf(stderr, "[INFO] Sending data with ctrl %d\n", ctrl);
            res = send_data(fd, msg, strlen(msg), ctrl);
            state = ACK0_State;
            break;
        case ACK0_State:
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            ready = select(fd+1, &readfds, NULL, NULL, &timeout);
            if (ready == 0) {
                fprintf(stderr, "[ERR] Timeout waiting for RR\n");
                sleep(1);
                state = Send0_State;
                break;
            }
            if (ready == -1) {
                fprintf(stderr, "[ERR] Error waiting for RR\n");
                return -1;
            }
            retr = receive(fd);


            if (DEBUG_ALL) fprintf(stderr, "[DEBUG] Received RR: %d\n", retr);

            if (retr == control_rej_field) {
                state = Send0_State;
            } else if (retr == control_rr_field) {
                state = Stop_SM_State;
                return res;
            } else {
                // some error
                state = Send0_State;
            }
            break;

        case Send1_State:
            fprintf(stderr, "[INFO] Sending data with ctrl %d\n", ctrl);
            res = send_data(fd, msg, strlen(msg), ctrl);
            state = ACK1_State;
            break;
        case ACK1_State:
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);
            ready = select(fd+1, &readfds, NULL, NULL, &timeout);
            if (ready == 0) {
                fprintf(stderr, "[ERR] Timeout waiting for response\n");
                sleep(1);
                state = Send0_State;
                break;
            }
            if (ready == -1) {
                fprintf(stderr, "[ERR] Error waiting for response\n");
                return -1;
            }

            if (retr == control_rej_field) {
                state = Send1_State;
            } else if (retr == control_rr_field) {
                state = Stop_SM_State;
                return res;
            } else {
                // some error
                state = Send1_State;
            }
        case Stop_SM_State:
            if (DEBUG_ALL) fprintf(stderr, "[INFO] Stopping sender state machine\n");
            return res;
            break;
        
        default:
            break;
        }
    }
    return res;
}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;

    /*if ( (argc < 2) ||
         ((strcmp("/dev/ttyS10", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS10\n");
        exit(1);
    }*/


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

    char data[] = "Hello, world!";
    char data1[] = "How are you?";
    char data2[] = "THis is a test message.";
    char data3[] = "Upsi.";
    char data4[] = "Bye!";
    char data5[] = "THis is a test message that is even longer.";

    fprintf(stderr, "[INFOOO] Sending data\n");
    send_msg(fd, data);
    fprintf(stderr, "[INFO] Sent message 1\n");
    fprintf(stderr, "[INFO] Sent message 1\n");
    send_msg(fd, data1);
    fprintf(stderr, "[INFO] Sent message 2\n");
    send_msg(fd, data2);
    fprintf(stderr, "[INFO] Sent message 3\n");
    send_msg(fd, data3);
    fprintf(stderr, "[INFO] Sent message 4\n");
    send_msg(fd, data4);
    fprintf(stderr, "[INFO] Sent message 5\n");
    send_msg(fd, data5);
    fprintf(stderr, "[INFO] Sent message 6\n");

    disconnect(fd, ctrl);

    // if (send_data(fd, data, strlen(data), 1) != 0) {
    //     fprintf(stderr, "[ERR] Error in sending data\n");
    //     exit(-1);
    // }

    // while (receive(fd) != 0) 
    // {
    //     fprintf(stderr, "[ERR] Error in receiving RR\n");
    //     if (send_data(fd, data, strlen(data), 1) != 0) {
    //         fprintf(stderr, "[ERR] Error in sending data\n");
    //         exit(-1);
    //     }
    // }
    // fprintf(stderr, "[INFO] Data sent successfully\n");


    // char data2[] = "This is a second message.";
    // if (send_data(fd, data2, strlen(data2), 0) != 0) {
    //     fprintf(stderr, "[ERR] Error in sending data\n");
    //     exit(-1);
    // }

    // while (receive(fd) != 0) 
    // {
    //     fprintf(stderr, "[ERR] Error in receiving RR\n");
    //     if (send_data(fd, data2, strlen(data2), 0) != 0) {
    //         fprintf(stderr, "[ERR] Error in sending data\n");
    //         exit(-1);
    //     }
    // }
    fprintf(stderr, "[INFO] Data sent successfully\n");

    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }


    close(fd);
    return 0;
}

/**
 * @brief protocol API to establish connection
 * 
 * @param connectionParameters structure containing 
 *        configuration parameters
 * @return int 1: success; -1: failure/ error
 */
int llopen(linkLayer connectionParameters) {
    int fd;
    struct termios oldtio,newtio;

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(connectionParameters.serialPort); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = connectionParameters.timeOut;   /* inter-character timer unused */
    newtio.c_cc[VMIN] = 5;   /* blocking read until 5 chars received */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    if (connectionParameters.role == TRANSMITTER) {
        if (handshake(fd) != 0) {
            fprintf(stderr, "[ERR] Error in handshake\n");
            exit(-1);
        }
    } else {
        if (receive_data(fd) != 0) {
            fprintf(stderr, "[ERR] Error in receiving data\n");
            exit(-1);
        }
    }

    return fd;
}

/**
 * @brief protocol API to write data
 * 
 * @param buffer array of characters to transmit
 * @param length length of the character array
 * @return int Number of written characters; Negative value in case of failure/ error
 */
int llwrite(unsigned char * buffer, int length) {
    int number_of_bytes_written = send_msg(fd, buffer);
    if (number_of_bytes_written <= 0) {
        fprintf(stderr, "[ERR] Error in sending data\n");
        return FAILURE;
    }
    return number_of_bytes_written;
}

/**
 * @brief 
 * 
 * @param connectionParameters  data link parameters stored in linkLayer structure
 * @param showStatistics true or false to show statistics collected by link layer for performance evaluation
 * @return int Positive value in case of failure/ error; Negative value in case of failure/ error
 */
int llclose(linkLayer connectionParameters, int showStatistics) {
    if (disconnect(fd, ctrl) != SUCCESS) {
        fprintf(stderr, "[ERR] Error in disconnecting\n");
        return FAILURE;
    }

    if (showStatistics) {
        // show statistics
    }
    
    return SUCCESS;
}
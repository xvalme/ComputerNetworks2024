//Here lies the code for the whole application

/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdbool.h>

#define FALSE 0
#define TRUE 1
#define SUCCESS 1
#define FAILURE -1

#define DEBUG_ALL 1

volatile int STOP=FALSE;

//ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1

#define MAX_PAYLOAD_SIZE 100000
#define DATA_BUFFER_SIZE (MAX_PAYLOAD_SIZE * 2 + 10) 

//CONNECTION deafault values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define TOGGLE_CTRL(var) ((var) == 0 ? (var) = 1 : ((var) = 0))

typedef struct {
    int current_ctrl;
} StateMachine;

typedef struct linkLayer{

    char serialPort[50]; //
    int role;  //
    int baudRate; // 
    int numTries; 
    int timeOut;
    
} linkLayer;

typedef enum {
    Start_State,
    Flag_RCV_State,
    A_RCV_State,
    C_RCV_State,
    BCC_OK_State,
    Stop_State,
    DATA_RCV_State,
    BCC2_OK_State
} s_StateMachine;

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

int fd;
int s_ctrl = 0;
volatile int s_STOP=FALSE;


struct termios oldtio,newtio;
struct termios s_oldtio,s_newtio;
StateMachine state_machine;

int r_ByteStuffing(char* str, int strlen) {

    char buffer [DATA_BUFFER_SIZE];
    int counter = 0;

    for (int i = 0;i++; i<strlen-1){

        if ((str[i] == 0x1b) && (str[i+1] == 0x7c)) {
            //Flag
            buffer[counter] = 0x5c;
            continue;
        }

        if ((str[i] == 0x1b) && str[i+1] == 0x7D){
            //Esc
            buffer[counter] = 0x1b;
            continue;
        }

        buffer[counter] = str[i];
        counter++;

    }

    memcpy(str, buffer, counter);

    return counter;

}

void r_initializeStateMachine() {
    state_machine.current_ctrl = 0;
}

int r_handshake(int fd) {
    char buf[6];
    int temporary_size = 6;
    char temporary[temporary_size];
    int res;

    char SET_correct [5] = {0x5c, 0x01, 0x08, 0, 0x5c};
    char UA [5] = {0x5c, 0x01, 0x06, 0, 0x5c};
    UA[3] = UA[1]^UA[2];

    char STATE = 'S';

    printf( "[r_handshake] Waiting for reception of SET packet.\n");

    while(TRUE){

        if (STOP == FALSE) {
            res = read(fd,buf,1);   
            buf[res]=0;       

            if (res) {
                STOP = TRUE;
                continue;
            } 
        }

        if (STOP == TRUE){

            //printf( "[INFO] Received byte: %x\n", buf[0]);
            switch (STATE) {
                
                case 'S':
                    {
                    if (buf[0] == SET_correct[0]){
                        STATE = 'F';
                        temporary[0] = buf[0];
                    }
                    break;
                    }
                case 'F':
                    {
                    if (buf[0] == SET_correct[0]){
                        STATE = 'F';
                    }
                    else if (buf[0] == SET_correct[1]){
                        STATE = 'A';
                        temporary[1] = buf[0];
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }

                    break;
                    }
                case 'A':
                    if (buf[0] == SET_correct[2]){
                        STATE = 'C';
                        temporary[2] = buf[0];
                    }
                    else if (buf[0] == temporary[1]) {
                        STATE = 'F';
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }

                    break;
                case 'C':
                    {
                    char xor = temporary[1]^temporary[2];

                    if (buf[0] == xor) {
                        STATE = 'B';
                        temporary[3] = buf[0];
                    }
                    else if (buf[0] == temporary[1]) {
                        STATE = 'F';
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }
                    break;
                    }
                case 'B':
                    if (buf[0] == SET_correct[4]) {
                        STATE = 'Z';
                        temporary[4] = buf[0];

                        printf("[SET] Received correct SET\n");

                        char UA [5] = {0x5c, 0x01, 0x06, "", 0x5c};

                        UA[3] = UA[1]^UA[2];

                        int res = write(fd, UA, 5);
                        int count = 2; 

                        while (res != 5) {
                            res = write(fd, UA, 5);
                            count--;
                            if (!count) {
                                return -1;
                            }
                        }
                        printf("[UA] Sent UA\n");

                        return 1;
                    }

                    else {
                        STATE = 'F';
                        memset (temporary, 0, temporary_size );
                    }

                    break;

                default:
                    printf( "%c\n", STATE);            
            }

            STOP = FALSE;
            continue;
        }
    }
}

int r_receive_data_packet_(int fd, int current_ctrl_int, char *data_buffer) {

    int res;
    char buf[DATA_BUFFER_SIZE];
    char temporary[DATA_BUFFER_SIZE];
    char data[DATA_BUFFER_SIZE];
    int STATE = 0;

    char FLAG = 0x5c;
    char Add_S = 0x01;
    char Add_R = 0x03;
    
    char Ctrl_up = 0xc0;
    char Ctrl_down = 0x80;

    char current_ctrl;
    
    int number_bytes_received = 0;
    char moving_xor = ' ';

    while (TRUE) {

        if (STOP == FALSE) {
            
            if (current_ctrl_int == 0) {
                current_ctrl = Ctrl_down;
            } else {
                current_ctrl = Ctrl_up;
            }

            res = read(fd, buf, 1);   

            if (res < 0) {
                return -1;
            }
            
            buf[res] = 0;       

            if (DEBUG_ALL) printf("Received byte: %x\n", buf[0]);

            if (res) {
                STOP = TRUE;
                continue;
            } 

        }

        if (STOP == TRUE) {

            switch (STATE) {
                case 0:
                    if (DEBUG_ALL) printf("In STATE 0\n");
                    if (buf[0] == FLAG) {
                        STATE = 1;
                        temporary[0] = buf[0];
                    }
                    break;

                case 1:
                    if (DEBUG_ALL) printf("In STATE 1\n");
                    if (buf[0] == Add_S) {
                        STATE = 2;
                        temporary[1] = buf[0];
                    }
                    else if (buf[0] == FLAG) {
                        STATE = 1;
                    }
                    else {
                        STATE = 0;
                        memset(temporary, 0, DATA_BUFFER_SIZE);
                        moving_xor = ' ';
                    }
                    break;

                case 2:
                    if (DEBUG_ALL) printf("In STATE 2\n");
                    if (DEBUG_ALL) printf( "Current ctrl: %x and received ctrl: %x\n", current_ctrl, buf[0]);
                    if (buf[0] == 0b00001010){
                        //DISC packet
                        STATE = 10;
                        break;
                    }
                    
                    if (buf[0] == current_ctrl) {
                        STATE = 3;
                        temporary[2] = buf[0];
                    }
                    else if (buf[0] == Add_S) {
                        STATE = 2;
                    }
                    else {
                        STATE = 0;
                        memset(temporary, 0, DATA_BUFFER_SIZE);
                        moving_xor = ' ';
                        return -1;
                    }
                    break;

                case 3:
                    if (DEBUG_ALL) printf("In STATE 3:");
                    
                    if (DEBUG_ALL) printf("|%x|%x|\n", buf[0], (temporary[1] ^ temporary[2]));

                    if (buf[0] == (temporary[1] ^ temporary[2])) {
                        STATE = 4;
                        temporary[3] = buf[0];
                    }
                    else if (buf[0] == current_ctrl) {
                        STATE = 3;
                    }
                    else {
                        STATE = 0;
                        memset(temporary, 0, DATA_BUFFER_SIZE);
                        moving_xor = ' ';
                    }
                    break;
                
                case 4:
                    if (DEBUG_ALL) printf("In STATE 4 |Moving XOR: %x| Received %x\n", moving_xor, buf[0]);
                    
                    if (buf[0] == FLAG) {
                        //Check if the last byte is the XOR of the previous bytes

                        if (DEBUG_ALL) printf("Received FLAG\n");

                        if (moving_xor == temporary[4]) {
                            printf("[I] Received data correctly.\n");
                            number_bytes_received--;
                            //data[number_bytes_received] = '\0';
                            /*for (int i = 0; i < number_bytes_received; i++) {
                                if (DEBUG_ALL) printf("%c", data[i]);
                            }
                            if (DEBUG_ALL) printf("\n");*/
                            memcpy(data_buffer, data, number_bytes_received);
                            return number_bytes_received;
                        }
                        else{
                            if (DEBUG_ALL) printf("Resetting state machine.\n");
                            STATE = 0;
                            
                            memset(temporary, 0, DATA_BUFFER_SIZE);
                            memset(data, 0, DATA_BUFFER_SIZE);
                            memset(buf, 0, DATA_BUFFER_SIZE);
                            moving_xor = ' ';
                            number_bytes_received = 0;

                            if (DEBUG_ALL) printf("State machine reseted.\n");
                            break;
                        }
                    }

                    data[number_bytes_received] = buf[0];
                    number_bytes_received++;

                    if (buf[0] != moving_xor) {
                    }
                    else {
                        if (DEBUG_ALL) printf( "Ready to receive flag\n");
                        temporary[4] = buf[0];
                        break;
                    }

                    moving_xor = data[number_bytes_received-1] ^ moving_xor;

                    break;

                case 10:

                    if (buf[0] == temporary[1]^temporary[2]) {
                        STATE = 11;
                    }

                    else {
                        STATE = 0;
                        memset(temporary, 0, DATA_BUFFER_SIZE);
                    }

                    break;

                case 11:

                    if (buf[0] == FLAG) {
                        return -2;
                    }

                    else {
                        STATE = 0;
                        break;
                    }

            }
            STOP = FALSE;
        }
    }
}

int r_send_rr(int fd, int ctrl){
    char rr [5] = {0x5c, 0x03, 0, 0, 0x5c};

    if (ctrl){
        rr[2] = 0b00010001;
    }
    else {
        rr[2] = 0b00000001;
    }

    rr[3] = rr[1]^rr[2];

    int res = write(fd, rr, 5);

    if (res){
        printf( "[RR] with value %d Sent.\n", ctrl);
        return 1;
    }
    else {
        return -1;
    }

}

int r_send_rej(int fd, int ctrl) {

    char rr [5] = {0x5c, 0x03, 0, 0, 0x5c};

    if (ctrl){
        rr[2] = 0b00010101;
    }
    else {
        rr[2] = 0b00000101;
    }

    rr[3] = rr[1]^rr[2];

    int res = write(fd, rr, 5);

    if (res){
        printf( "[REJ] with value %d Sent.\n", ctrl);
        return 1;
    }
    else {
        return -1;
    }
}

int r_verify_if_ua_received(int fd){
    char buf[6];
    int temporary_size = 6;
    char temporary[temporary_size];
    int res;

    char SET_correct [5] = {0x5c, 0x01, 0x06, 0, 0x5c};

    char STATE = 'S';

    printf( "[r_disconnect] Waiting for reception of UA packet.\n");

    while(TRUE){

        if (STOP == FALSE) {
            res = read(fd,buf,1);   
            buf[res]=0;       

            if (res) {
                STOP = TRUE;
                continue;
            } 
        }

        if (STOP == TRUE){

            //printf( "[INFO] Received byte: %x\n", buf[0]);
            switch (STATE) {
                
                case 'S':
                    {
                    if (buf[0] == SET_correct[0]){
                        STATE = 'F';
                        temporary[0] = buf[0];
                    }
                    break;
                    }
                case 'F':
                    {
                    if (buf[0] == SET_correct[0]){
                        STATE = 'F';
                    }
                    else if (buf[0] == SET_correct[1]){
                        STATE = 'A';
                        temporary[1] = buf[0];
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }

                    break;
                    }
                case 'A':
                    if (buf[0] == SET_correct[2]){
                        STATE = 'C';
                        temporary[2] = buf[0];
                    }
                    else if (buf[0] == temporary[1]) {
                        STATE = 'F';
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }

                    break;
                case 'C':
                    {
                    char xor = temporary[1]^temporary[2];

                    if (buf[0] == xor) {
                        STATE = 'B';
                        temporary[3] = buf[0];
                    }
                    else if (buf[0] == temporary[1]) {
                        STATE = 'F';
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }
                    break;
                    }
                case 'B':
                    if (buf[0] == SET_correct[4]) {
                        STATE = 'Z';
                        temporary[4] = buf[0];

                        printf("[DISC] Received correct UA. r_disconnecting.\n");

                        return 1;
                    }

                    else {
                        STATE = 'F';
                        memset (temporary, 0, temporary_size );
                    }

                    break;

                default:
                    printf( "%c\n", STATE);            
            }

            STOP = FALSE;
            continue;
        }
    }
}

int r_disconnect(int fd){
    char buf[6];
    int temporary_size = 6;
    char temporary[temporary_size];
    int res;

    char SET_correct [5] = {0x5c, 0x01, 0x0A, 0, 0x5c};
    char UA [5] = {0x5c, 0x01, 0x0A, 0, 0x5c};
    UA[3] = UA[1]^UA[2];

    char STATE = 'S';

    printf( "[r_disconnect] Waiting for reception of DISC packet.\n");

    while(TRUE){

        if (STOP == FALSE) {
            res = read(fd,buf,1);   
            buf[res]=0;       

            if (res) {
                STOP = TRUE;
                continue;
            } 
        }

        if (STOP == TRUE){

            //printf( "[INFO] Received byte: %x\n", buf[0]);
            switch (STATE) {
                
                case 'S':
                    {
                    if (buf[0] == SET_correct[0]){
                        STATE = 'F';
                        temporary[0] = buf[0];
                    }
                    break;
                    }
                case 'F':
                    {
                    if (buf[0] == SET_correct[0]){
                        STATE = 'F';
                    }
                    else if (buf[0] == SET_correct[1]){
                        STATE = 'A';
                        temporary[1] = buf[0];
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }

                    break;
                    }
                case 'A':
                    if (buf[0] == SET_correct[2]){
                        STATE = 'C';
                        temporary[2] = buf[0];
                    }
                    else if (buf[0] == temporary[1]) {
                        STATE = 'F';
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }

                    break;
                case 'C':
                    {
                    char xor = temporary[1]^temporary[2];

                    if (buf[0] == xor) {
                        STATE = 'B';
                        temporary[3] = buf[0];
                    }
                    else if (buf[0] == temporary[1]) {
                        STATE = 'F';
                    }
                    else {
                        STATE = 'S';
                        memset (temporary, 0, temporary_size );
                    }
                    break;
                    }
                case 'B':
                    if (buf[0] == SET_correct[4]) {
                        STATE = 'Z';
                        temporary[4] = buf[0];

                        printf("[DISC] Received correct DISC\n");

                        char UA [5] = {0x5c, 0x03, 0x0A, "", 0x5c};

                        UA[3] = UA[1]^UA[2];

                        int res = write(fd, UA, 5);
                        int count = 2; 

                        while (res != 5) {
                            res = write(fd, UA, 5);
                            count--;
                            if (!count) {
                                return -1;
                            }
                        }
                        printf("[DISC] Sent DISC back. Waiting for UA paccket.\n");

                        if (r_verify_if_ua_received(fd)) {
                            return 1;
                        }
                        else {
                            return -1;
                        }
                    }

                    else {
                        STATE = 'F';
                        memset (temporary, 0, temporary_size );
                    }

                    break;

                default:
                    printf( "%c\n", STATE);            
            }

            STOP = FALSE;
            continue;
        }
    }
}

int r_receive_data(int fd, char *data_buffer){

    int res;

    printf( ":--------------\n");

    res = r_receive_data_packet_(fd, state_machine.current_ctrl, data_buffer);

    if (res) {
        //Answer back with RR packet

        r_send_rr(fd, state_machine.current_ctrl);

        //Bytestuffing 
        r_ByteStuffing(data_buffer, DATA_BUFFER_SIZE);

        printf( "%s\n", data_buffer);

        printf( "--------------\n");

        state_machine.current_ctrl = !state_machine.current_ctrl;

        return res;

    }

    if (res == -2) {
            r_disconnect(fd);
        }
    if (res == -3) {
        // Asnwer back with REJ packet 

        r_send_rej(fd, state_machine.current_ctrl);

        return 1;
    }
    

}

int r_llopen(linkLayer connectionParameters) {

    if (connectionParameters.role == TRANSMITTER) {
       return -1;
    }

    //Generate fd

    char buf[255];

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );
    if (fd < 0) {return -1; }

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */


    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    // Our code


    r_initializeStateMachine();

    int ret = r_handshake(fd);

    if (ret == 1) {

        return 1;
    }
    else {
        return -1;
    }

}

int r_llclose(linkLayer connectionParameters, int showStatistics){

    int res = r_disconnect(fd);

    //TODO Missing statistics

    tcsetattr(fd,TCSANOW,&oldtio);

    close(fd);

    if (res == 1) {
        return 1;
    }
    else {
        return -1;
    }

}

int llopen(linkLayer connectionParameters) {
    
    if (connectionParameters.role == TRANSMITTER) {
        return s_llopen(connectionParameters);
    }
    else {
        return r_llopen(connectionParameters);
    }
    
}

int llclose (linkLayer connectionParameters, int showStatistics) {
    
    if (connectionParameters.role == TRANSMITTER) {
        return s_llclose(connectionParameters, showStatistics);
    }
    else {
        return r_llclose(connectionParameters, showStatistics);
    }
    
}

int llread(unsigned char * buffer){

    int res = r_receive_data(fd, buffer);

    return res;

}

/*int main(int argc, char** argv)
{

    linkLayer connectionParameters;
    connectionParameters.baudRate = BAUDRATE_DEFAULT;
    connectionParameters.numTries = MAX_RETRANSMISSIONS_DEFAULT;
    connectionParameters.timeOut = TIMEOUT_DEFAULT;
    connectionParameters.role = RECEIVER;
    memcpy(connectionParameters.serialPort, "/dev/pty4", 10);

    llopen(connectionParameters);

    unsigned char buffer[DATA_BUFFER_SIZE];

    llread(buffer);

    llclose(connectionParameters, 0);

    return 0;
}*/

int time_out = TIMEOUT_DEFAULT;
int s_fd;

int s_handshake(int s_fd) {
    printf( "[INFO] Initializing s_handshake \n");

	const char flag = 0x5c;
	const char address = 0x01;
	const char control = 0x08;
	const char control_ua = 0x06;

	const char bcc1 = address^control;
	const char bcc1_ua = address^control_ua;
	
    char buffer[5]= {flag, address, control, bcc1, flag};

    int res = write(s_fd,buffer,5);

    if (res == 5) {
        printf( "[INFO] Sent SET packet\n");
    } else {
        printf( "[ERR] Error sending SET packet\n");
        return -1;
    }

    char buf[6];
	
    s_StateMachine state = Start_State;
    while (s_STOP==FALSE || state != Stop_State) {       /* loop for input */
        res = read(s_fd,buf,1);   /* returns after 5 chars have been input */
        buf[res]=0;               /* so we can printf... */
        // printf( "[INFO] s_received byte: %x, %d\n", buf[0], state);
        char s_received_correct = 1;
    
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
                printf( "[INFO] Connection established\n");
                return 0;
            } else {
                state = Start_State;
            }
            break;
        }	
    }
    return 0;
}

int s_receive_data(int s_fd) {
    const char flag = 0x5c;
	const char address = 0x01;
	const char control = 0x08;
	const char control_ua = 0x06;

	const char bcc1 = address^control;  
	const char bcc1_ua = address^control_ua;

    char buf[1];
    s_StateMachine state = Start_State;
    int consecutive_flags = 0;
    char s_received_data[DATA_BUFFER_SIZE];
    int data_index = 0;

	
    while (s_STOP==FALSE || state != Stop_State) {       /* loop for input */
        int res = read(s_fd,buf,1);   /* returns after 5 chars have been input */
        if (res < 0) {
            printf( "[ERR] Error reading from serial port\n");
            return -1;
        }
        // char s_received_correct = 1;
        if (buf[0] == flag) {
            consecutive_flags++;
        } else {
            consecutive_flags = 0;
        }

        if (consecutive_flags == 2) {
            // two consecutive flags, end of data
            printf( "[INFO] s_received end of data\n");
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
                // Save s_received data
                if (buf[0] != flag) {
                    if (data_index < DATA_BUFFER_SIZE) {
                        s_received_data[data_index++] = buf[0];
                    } else {
                        printf( "[ERR] s_received data too large\n");
                        return -1;
                    }
                } else {
                    state = BCC2_OK_State;
                }
                break;

            case BCC2_OK_State:
                if (buf[0] == flag) {
                    state = Stop_State;
                    // Print s_received data
                    printf("[INFO] s_received data: %s\n", s_received_data);
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
int s_receive(int s_fd) {
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

    int ctrl_get = s_ctrl;

    char buf[1];
    s_StateMachine state = Start_State;
    int consecutive_flags = 0;
    char s_received_data[DATA_BUFFER_SIZE];
    int data_index = 0;

    printf( "[INFO] Initializing receiving \n");

    while (s_STOP==FALSE || state != Stop_State) {       /* loop for input */
        // printf( "[INFO] Initializing RR \n");
        int res = read(s_fd,buf,1);   /* returns after 5 chars have been input */
        // printf( "[INFO] s_received byte: %x, %d\n", buf[0], state);
        if (res < 0) {
            printf( "[ERR] Error reading from serial port\n");
            return -1;
        }
        // char s_received_correct = 1;
        if (buf[0] == flag) {
            consecutive_flags++;
        } else {
            consecutive_flags = 0;
        }

        if (consecutive_flags == 2) {
            // two consecutive flags, end of data
            printf( "[INFO] s_received end of data\n");
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
                // if (DEBUG_ALL) printf( "Buffer: %04x\n", buf[0]);
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
                        printf( "[INFO] AAA\n");

                } else {
                    state = Start_State;
                }
                break;

            case BCC_OK_State:
                if (buf[0] == flag) {
                    if (ctrl_get != s_ctrl) return -1;

                    TOGGLE_CTRL(s_ctrl);
                    char buffer[5]= {flag, address_sender, current_ctrl, address_sender^current_ctrl, flag};
                    //int res = write(s_fd,buffer,5);
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
                        printf( "[INFO] s_received RR\n");
                        return control_rr_field;
                        break;
                    case control_rej_field:
                        printf( "[INFO] s_received REJ\n");
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
char* s_byte_stuffing(const char* data, int data_length) {
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

    return new_data;
}

int s_send_data(int s_fd, const char* data, int data_length, int s_ctrl) {
	char test_packet[data_length+6];
    const char flag = 0x5c;
    const char address = 0x01;
    
    char control;
	if (s_ctrl){
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

    char *new_data = s_byte_stuffing(data, data_length);

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
        if (DEBUG_ALL) printf( "%x ", test_packet[i]);
    }

    int res = write(s_fd,test_packet, sizeof(test_packet));
    
    if (res == sizeof(test_packet)) {
        printf( "[INFO] Sent dummy data packet\n");
    } else {
        printf( "[ERR] Error sending dummy data packet\n");
        return -1;
    }

    return res;
}
int s_disconnect(int s_fd, int s_ctrl) {
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
        if (DEBUG_ALL) printf( "%x ", test_packet[i]);
    }

    printf( "[INFO] Sending DISC\n");
    int res = write(s_fd,test_packet, sizeof(test_packet));
    
    if (res == sizeof(test_packet)) {
        printf( "[INFO] Sent DISC\n");
    } else {
        printf( "[ERR] Error sending DISC\n");
        return FAILURE;
    }
    
    // recive disc
    int control_code = s_receive(s_fd);


    if (control_code == control_disc_field) {
        printf( "[INFO] s_received DISC\n");
    } else {
        printf( "[ERR] Error receiving DISC %d\n", control_code);
        return FAILURE;
    }

    // send ua
    char buffer[5]= {flag, address, control_ua, address^control_ua, flag};
    res = write(s_fd,buffer,5);


    return SUCCESS;
}

int s_send_msg(int s_fd, const char* msg, int len) {
    int retr;

    // timeout
    struct timeval timeout;
    timeout.tv_sec = time_out;
    timeout.tv_usec = 0;
    fd_set readfds;
    int ready;

    int res = -1;


    senderSM state = Send0_State;
    while (state != Stop_SM_State) {
        // printf( "[DEBUGGGG] State: %d, msg %s\n", state, msg);

        switch (state)
        {
        case Send0_State:
            printf( "[INFO] Sending data with s_ctrl %d\n", s_ctrl);
            res = s_send_data(s_fd, msg, len, s_ctrl);
            state = ACK0_State;
            break;
        case ACK0_State:
            FD_ZERO(&readfds);
            FD_SET(s_fd, &readfds);
            ready = select(s_fd+1, &readfds, NULL, NULL, &timeout);
            if (ready == 0) {
                printf( "[ERR] Timeout waiting for RR\n");
                sleep(1);
                state = Send0_State;
                break;
            }
            if (ready == -1) {
                printf( "[ERR] Error waiting for RR\n");
                return -1;
            }
            retr = s_receive(s_fd);


            if (DEBUG_ALL) printf( "[DEBUG] s_received RR: %d\n", retr);

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
            printf( "[INFO] Sending data with s_ctrl %d\n", s_ctrl);
            res = s_send_data(s_fd, msg, len, s_ctrl);
            state = ACK1_State;
            break;
        case ACK1_State:
            FD_ZERO(&readfds);
            FD_SET(s_fd, &readfds);
            ready = select(s_fd+1, &readfds, NULL, NULL, &timeout);
            if (ready == 0) {
                printf( "[ERR] Timeout waiting for response\n");
                sleep(1);
                state = Send0_State;
                break;
            }
            if (ready == -1) {
                printf( "[ERR] Error waiting for response\n");
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
            if (DEBUG_ALL) printf( "[INFO] Stopping sender state machine\n");
            return res;
            break;
        
        default:
            break;
        }
    }
    return res;
}

/**
 * @brief protocol API to establish connection
 * 
 * @param connectionParameters structure containing 
 *        configuration parameters
 * @return int 1: success; -1: failure/ error
 */
int s_llopen(linkLayer connectionParameters) {

    s_fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );
    if (s_fd < 0) { perror(connectionParameters.serialPort); exit(-1); }

    if ( tcgetattr(s_fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN] = 5;   /* blocking read until 5 chars s_received */

    /* set timeout */
    time_out = connectionParameters.timeOut;

    tcflush(s_fd, TCIOFLUSH);

    if (tcsetattr(s_fd,TCSANOW,&newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    if (connectionParameters.role == TRANSMITTER) {
        if (s_handshake(s_fd) != 0) {
            printf( "[ERR] Error in s_handshake\n");
            exit(-1);
        }
    } else {
        if (s_receive_data(s_fd) != 0) {
            printf( "[ERR] Error in receiving data\n");
            exit(-1);
        }
    }

    return s_fd;
}

/**
 * @brief protocol API to write data
 * 
 * @param buffer array of characters to transmit
 * @param length length of the character array
 * @return int Number of written characters; Negative value in case of failure/ error
 */
int llwrite(unsigned char * buffer, int length) {
    int number_of_bytes_written = s_send_msg(s_fd, buffer, length);
    if (number_of_bytes_written <= 0) {
        printf( "[ERR] Error in sending data\n");
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
int s_llclose(linkLayer connectionParameters, int showStatistics) {
    if (s_disconnect(s_fd, s_ctrl) != SUCCESS) {
        printf( "[ERR] Error in s_disconnecting\n");
        return FAILURE;
    }

    if (showStatistics) {
        // show statistics
        printf( "[INFO] Showing statistics, (comming soon...)\n");
    }

   if ( tcsetattr(s_fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }
    close(s_fd);
    
    return SUCCESS;
}

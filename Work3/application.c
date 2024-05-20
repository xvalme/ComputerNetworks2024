//Here lies the code for the whole application

/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define FALSE 0
#define TRUE 1
#define SUCCESS 1
#define FAILURE -1

#define DEBUG_ALL 0

volatile int STOP=FALSE;

//ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1

#define MAX_PAYLOAD_SIZE 1000
#define DATA_BUFFER_SIZE (MAX_PAYLOAD_SIZE * 2 + 10) 

//CONNECTION deafault values
#define BAUDRATE_DEFAULT B38400
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

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

int fd;

struct termios oldtio,newtio;
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

    buffer[counter] = "\0";

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

    fprintf(stderr, "[r_handshake] Waiting for reception of SET packet.\n");

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

            //fprintf(stderr, "[INFO] Received byte: %x\n", buf[0]);
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
                    fprintf(stderr, "%c\n", STATE);            
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
                    if (DEBUG_ALL) fprintf(stderr, "Current ctrl: %x and received ctrl: %x\n", current_ctrl, buf[0]);
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
                    if (DEBUG_ALL) printf("In STATE 3\n");
                    
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
                            data[number_bytes_received] = '\0';
                            for (int i = 0; i < number_bytes_received; i++) {
                                if (DEBUG_ALL) printf("%c", data[i]);
                            }
                            if (DEBUG_ALL) printf("\n");
                            memcpy(data_buffer, data, number_bytes_received);
                            return number_bytes_received;
                        }
                        else{
                            if (DEBUG_ALL) printf("Resetting state machine.");
                            STATE = 0;
                            memset(temporary, 0, DATA_BUFFER_SIZE);
                            memset(data, 0, DATA_BUFFER_SIZE);
                            moving_xor = ' ';
                            number_bytes_received = 0;
                            break;
                        }
                    }


                    data[number_bytes_received] = buf[0];
                    number_bytes_received++;

                    if (buf[0] != moving_xor) {
                    }
                    else {
                        if (DEBUG_ALL) fprintf(stderr, "Ready to receive flag\n");
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
        fprintf(stderr, "[RR] with value %d Sent.\n", ctrl);
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
        fprintf(stderr, "[REJ] with value %d Sent.\n", ctrl);
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

    fprintf(stderr, "[r_disconnect] Waiting for reception of UA packet.\n");

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

            //fprintf(stderr, "[INFO] Received byte: %x\n", buf[0]);
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
                    fprintf(stderr, "%c\n", STATE);            
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

    fprintf(stderr, "[r_disconnect] Waiting for reception of DISC packet.\n");

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

            //fprintf(stderr, "[INFO] Received byte: %x\n", buf[0]);
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
                    fprintf(stderr, "%c\n", STATE);            
            }

            STOP = FALSE;
            continue;
        }
    }
}

int r_receive_data(int fd, char *data_buffer){

    int res;

    fprintf(stderr, "--------------\n");

    memset(data_buffer, 0, DATA_BUFFER_SIZE);


    while(TRUE){
        res = r_receive_data_packet_(fd, state_machine.current_ctrl, data_buffer);
        if (res) {
            //Answer back with RR packet

            r_send_rr(fd, state_machine.current_ctrl);

            //Bytestuffing 
            r_ByteStuffing(data_buffer, DATA_BUFFER_SIZE);

            fprintf(stderr, "%s\n", data_buffer);

            fprintf(stderr, "--------------\n");

            state_machine.current_ctrl = !state_machine.current_ctrl;

            return res;

        }

        if (res == -2) {
                r_disconnect(fd);
            }
        else {
            // Asnwer back with REJ packet 

            r_send_rej(fd, state_machine.current_ctrl);

            return 1;
        }
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

int s_llclose(linkLayer connectionParameters, int showStatistics){

    

}

int s_llopen(linkLayer connectionParameters) {

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

int main(int argc, char** argv)
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
}

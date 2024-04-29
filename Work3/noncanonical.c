/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1


#define DATA_BUFFER_SIZE 1024
#define DEBUG_ALL 0

volatile int STOP=FALSE;

typedef struct {
    int current_ctrl;
} StateMachine;

StateMachine state_machine;

int ByteStuffing(char* str, int strlen) {

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

void initializeStateMachine() {
    state_machine.current_ctrl = 1;
}

int handshake(int fd) {
    char buf[6];
    int temporary_size = 6;
    char temporary[temporary_size];
    int res;

    char SET_correct [5] = {0x5c, 0x01, 0x08, 0, 0x5c};
    char UA [5] = {0x5c, 0x01, 0x06, 0, 0x5c};
    UA[3] = UA[1]^UA[2];

    char STATE = 'S';

    fprintf(stderr, "[Handshake] Waiting for reception of SET packet.\n");

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

int receive_data_packet_(int fd, int current_ctrl_int, char *data_buffer) {

    int res;
    char buf[DATA_BUFFER_SIZE];
    char temporary[DATA_BUFFER_SIZE];
    char data[DATA_BUFFER_SIZE];
    int STATE = 0;

    char FLAG When= 0x5c;
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
                            printf("[I] Received data correctly: ");
                            number_bytes_received--;
                            data[number_bytes_received] = '\0';
                            for (int i = 0; i < number_bytes_received; i++) {
                                if (DEBUG_ALL) printf("%c", data[i]);
                            }
                            printf("\n");
                            memcpy(data_buffer, data, number_bytes_received);
                            return 1;
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
                        return 2;
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

int send_rr(int fd, int ctrl){
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

int send_rej(int fd, int ctrl) {

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

int disconect(int fd){

    //Send DISC back

}

int receive_data(int fd){

    char data_buffer[DATA_BUFFER_SIZE];

    int res;

    while(TRUE){
        res = receive_data_packet_(fd, state_machine.current_ctrl, data_buffer);
        if (res == 1) {
            //Answer back with RR packet

            send_rr(fd, state_machine.current_ctrl);

            //Bytestuffing 
            ByteStuffing(data_buffer, DATA_BUFFER_SIZE);

            fprintf(stderr, "%s\n", data_buffer);

            state_machine.current_ctrl = !state_machine.current_ctrl;

            return 1;

        }

        if (res == 2) {
            disconnect(fd);
        }
        else {
            // Asnwer back with REJ packet 

            send_rej(fd, state_machine.current_ctrl);

            return 1;
        }
    }

}


int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

    /*Comment to use virtual ports
    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS0", argv[1])!=0) &&
          (strcmp("/dev/ttyS1", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }
    */

    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd < 0) { perror(argv[1]); exit(-1); }

    if (tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
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


    initializeStateMachine();
    handshake(fd);
    receive_data(fd);
    receive_data(fd);

    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}


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

volatile int STOP=FALSE;

#define DATA_BUFFER_SIZE 1024

int handshake(int fd) {
    char buf[6];
    int temporary_size = 6;
    char temporary[temporary_size];
    int res;

    char SET_correct [5] = {0x5c, 0x03, 0x08, 0, 0x5c};
    char UA [5] = {0x5c, 0x03, 0x06, 0, 0x5c};
    UA[3] = UA[1]^UA[2];

    char STATE = 'S';

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

                        printf("[INFO] Received correct SET\n");

                        char UA [5] = {0x5c, 0x03, 0x06, "", 0x5c};

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
                        printf("[INFO] Sent UA\n");

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

int receive_data(int fd) {

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

    char current_ctrl_int = 1;
    char current_ctrl = Ctrl_down;
    
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

            printf("Received byte: %x\n", buf[0]);

            if (res) {
                STOP = TRUE;
                continue;
            } 
        }

        if (STOP == TRUE) {
            switch (STATE) {
                case 0:
                    printf("In STATE 0\n");
                    if (buf[0] == FLAG) {
                        STATE = 1;
                        temporary[0] = buf[0];
                    }
                    break;

                case 1:
                    printf("In STATE 1\n");
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
                    }
                    break;

                case 2:
                    printf("In STATE 2\n");
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
                    }
                    break;

                case 3:
                    printf("In STATE 3\n");
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
                    }
                    break;
                
                case 4:
                    printf("In STATE 4\n");
                    if (buf[0] == FLAG) {
                        STATE = 0;
                        memset(temporary, 0, DATA_BUFFER_SIZE);
                    }
                    else if (buf[0] == moving_xor) {
                        STATE = 41;
                        temporary[4] = buf[0];
                    }
                    else {
                        data[number_bytes_received] = buf[0];
                        number_bytes_received++;
                        moving_xor = data[number_bytes_received-1] ^ moving_xor;
                        printf("Moving XOR: %x\n", moving_xor);
                    }
                    break;

                case 41:
                    printf("In STATE 41\n");

                    if (buf[0] == FLAG) {
                        printf("Received data correctly: ");
                        for (int i = 0; i < number_bytes_received; i++) {
                            printf("%c", data[i]);
                        }
                        printf("\n");
                        current_ctrl_int = !current_ctrl_int;
                        return 1;
                    }
                    else {
                        printf("Failed to receive data\n");
                        return -1;
                    }
                    break;
            }
            STOP = FALSE;
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

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until 5 chars received */

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

    handshake(fd);
    receive_data(fd);

    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}

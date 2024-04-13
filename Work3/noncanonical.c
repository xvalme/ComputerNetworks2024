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

int receive_handshake(int fd) {
    char buf[6];

    while(STOP == FALSE){
        int res = read(fd,buf,5);   /* returns after 5 chars have been input */
        buf[res]=0;               /* so we can printf... */
    }

    fprintf("[INFO] Received SET packet\n");

    char correct_answer [5] = {0x5c, 0x03, 0x08, "", 0x5c};
    correct_answer[3]  = buf[1]^buf[2];

	if (strncmp(correct_answer, buf, 5) == 0) {
	    
        fprintf("[SUC] SET packet not corrupted\n");

	    char UA [5] = {0x5c, 0x03, 0x06, "", 0x5c};

	    UA[3] = UA[1]^UA[2];

	    res = write(fd, UA, 5);
		
        printf("[INFO] Sent UA packet\n");
        printf("[INFO] Connection established\n");

        return 0;
	}  	    

    else {
        fprintf("[ERR] SET packet corrupted\n");
        return -1;
    }

}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];

    if ( (argc < 2) ||
         ((strcmp("/dev/ttyS0", argv[1])!=0) &&
          (strcmp("/dev/ttyS1", argv[1])!=0) )) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

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

    receive_handshake(fd);
}

    sleep(1);
    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}

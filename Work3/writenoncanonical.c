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

int handshake(int fd) {
    fprintf("[INFO] Initializing Handshake \n");

	const char flag = 0x5c;
	const char address = 0x03;
	const char control = 0x08;
	const char bcc1 = address^control;
	
    char buffer[5]= {flag, address, control, bcc1, flag};

    res = write(fd,buffer,5);

    if (res == 5) {
        fprintf("[INFO] Sent SET packet\n");
    } else {
        fprintf("[ERR] Error sending SET packet\n");
        return -1;
    }

    char buf[5];

    while (STOP==FALSE) {       /* loop for input */
        int res = read(fd,buf,5);   /* returns after 5 chars have been input */
        buf[res]=0;               /* so we can printf... */

        char received_correct = 1;
        if (memcmp(buf, &flag, 1)==0){
			//printf("Flag correct\n");
		}else{
			received_correct=0;
		}
		
		if (memcmp(buf+1, &address, 1)==0){
			//printf("Address correct\n");
		}else{
			received_correct=0;
		}
		
		if (memcmp(buf+2, &control, 1)==0){
			//printf("Control is SET\n");
		}else{
			received_correct=0;
		}
		
		char xor_shouldbe = *(buf+1) ^ *(buf+2);
		if (memcmp(buf+3, &xor_shouldbe, 1)==0){
			//printf("BCC is correct\n");
		}else{
			received_correct=0;
		}
		
		if (memcmp(buf+4, &flag, 1)==0){
			//printf("END flag is correct\n");
		}else{
			received_correct=0;
		}
		
		if (received_correct){
			fprintf("[INFO] Received UA packet\n");
			
		}
        else{
            fprintf("[ERR] Corrupted UA packet\n");
            return -1;
        }
		
    }

    fprintf("[INFO] Connection established\n");
    return 0;


}

int main(int argc, char** argv)
{
    int fd,c, res;
    struct termios oldtio,newtio;
    char buf[255];
    int i, sum = 0, speed = 0;

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

    handshake(fd);

    sleep(1);
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }


    close(fd);
    return 0;
}

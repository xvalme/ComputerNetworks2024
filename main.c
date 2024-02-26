#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAX_CHAR_SIZE 1024
#define FTP_DATA_PORT 21
#define FTP_COMMAND_PORT 20

//For testing:
// $ gcc main.c -o main.exe &&  ./main.exe ftp://ftp.up.pt/pub/

/*
The authors (Filip and Valentino) certify that this work is a creation of theirs
The estabilishment of the connection was built based on the Beej's Guide to Network 
Programming as well as the available files on moodle.
*/

int main(int argc, char *argv[]) {
    printf("\033[0;32mFTP DOWNLOAD APPLICATION BY Filip and Valentino. FEUP 2024");
    printf("\033[0;37m\n");

    //Parsing of the URL

    char url [MAX_CHAR_SIZE] ;
    char user [MAX_CHAR_SIZE];
    char password [MAX_CHAR_SIZE];
    char host [MAX_CHAR_SIZE];
    char url_path [MAX_CHAR_SIZE];

    char temp [MAX_CHAR_SIZE];

    int user_connection = 0;

    if (argc != 2){
        printf("Invaling arguments. Format: download_ftp URL");
        exit(-1);
    }

    //Copy argument

    if (strlen(argv[1]) > MAX_CHAR_SIZE) {
        exit(-1);
    }

    strcpy(url, argv[1]);

    if (strncmp (url, "ftp://", 6) != 0){
        printf("Not a FTP URL. Format: ftp://[<user>:<password>@]<host>/<url-path>");
        exit(-1);
    }

    strcpy(url, url + 6);

    // Now we check if there is a @

    char *pos = strchr(url, '@');

    if (pos) {
        //There is a username and password
        //We copy to a buffer the str before since strtok is destructive

        strcpy(temp, url);
        char *token = strtok(temp, ":");
        
        if (!token) {
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> ");
            exit(-1);
        }

        strcpy(user, token);

        char *rest = strtok(NULL, "");
        token = strtok( rest , "@");

        if (!rest) {
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> ");
            exit(-1);
        }

        strcpy(password, token);
        
        rest = strtok(NULL, "");

        if (!rest) {
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> ");
            exit(-1);
        }

        strcpy(url, rest);

        if (!strlen(user) || !strlen(password) || !strlen(url)){
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> ");
            exit(-1);
        }

        user_connection = 1;

    }
    
    //Now there is only the url left to parse

    strcpy(temp, url);
    char *token = strtok (temp, "/");

    if (!token) {
        printf("Invalid URL. There is no path. Format: ftp://[<user>:<password>@]<host>/<url-path>");
        exit(-1);
    }

    strcpy(host, token);
    token = strtok(NULL,"");

    if (!token) {
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> ");
            exit(-1);
    }

    strcpy(url_path, token);

    if (!strlen(host) || !strlen(url_path)){
        printf("Invalid URL host and/or path. Format ftp://[<user>:<password>@]<host>/<url-path>");
        exit(-1);
    }

    //Now everything was parsed from the input. Lets get 1st the IP address of the host.

    struct hostent *h = gethostbyname(host);

    if (h == NULL) {
        herror("gethostbyname");
        exit(-1);
    }

    printf("URL Resolved: %s", inet_ntoa(*((struct in_addr *)h->h_addr)));

    //Now we try to open 2 TCP connection, one for Control and other for data.

    //Creating a stuct for the address:
    struct sockaddr_in address_ctrl;
    memset(&address_ctrl,0, sizeof(address_ctrl)); //Zeroing the struct

    address_ctrl.sin_family = AF_INET; //IPv4
    address_ctrl.sin_addr.s_addr = inet_addr(host); 
    address_ctrl.sin_port = htons(FTP_COMMAND_PORT);

    struct sockaddr_in address_data;
    memset(&address_data,0, sizeof(address_data)); //Zeroing the struct

    address_data.sin_family = AF_INET; //IPv4
    address_data.sin_addr.s_addr = inet_addr(host); 
    address_data.sin_port = htons(FTP_COMMAND_PORT);

    //Opening the sockets
    int socket_id_ctrl = socket(AF_INET, SOCK_STREAM, 0);
    int socket_id_data = socket(AF_INET, SOCK_STREAM, 0);


    if (!socket_id_ctrl || !socket_id_data) {
        printf("Kernel does not want you to have a socket :(");
        exit(-1);
    }

    //Now we use the socket to connect to server
    int connection_ctrl = connect(socket_id_ctrl, (struct sockaddr *)&address_ctrl, sizeof(address_ctrl));
    int connection_data = connect(socket_id_data, (struct sockaddr *)&address_data, sizeof(address_data));

    if (!connection_ctrl || !connection_data){
        printf("Host unreachable");
        exit(-1); 
    }

    printf("\nConnetion estabilished. Sockets for Control|Data: %d|%d.", socket_id_ctrl, socket_id_data);

    //Now the rest...

    return 0;
}

/*
struct hostent {
    char *h_name;        // Official name of the host.
    char **h_aliases;    // A NULL-terminated array of alternate names for the host.
    int h_addrtype;      // The type of address being returned; usually AF_INET.
    int h_length;        // The length of the address in bytes.
    char **h_addr_list;  // A zero-terminated array of network addresses for the host.
                         // Host addresses are in Network Byte Order.
};

#define h_addr h_addr_list[0] // The first address in h_addr_list.
*/

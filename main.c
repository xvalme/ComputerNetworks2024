#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#define MAX_CHAR_SIZE 4096
#define FTP_DATA_PORT 21
#define FTP_COMMAND_PORT 20

//For testing:
// make
// ./main ftp://ftp.up.pt/pub/debian/README.html

/*
The authors (Filip and Valentino) certify that this work is a creation of theirs.
The estabilishment of the connection was built based on the Beej's Guide to Network 
Programming as well as the available files on moodle.
*/

/**
 * @brief Get the server addr object
 * 
 * @param ip_address 
 * @param data_port 
 * @return struct sockaddr_in 
 */
struct sockaddr_in get_server_addr(const char* ip_address, int data_port) {
    struct sockaddr_in server_addr;
    bzero((char*)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_address);  // 32 bit Internet address network byte ordered
    server_addr.sin_port = htons(data_port);  

    return server_addr;
}

/**
 * @brief check recived code for errors
 * 
 * @param code 
 * @return int 1 if error
 */
int check_return_code(char code) {
    switch (code)
    {
    case '4':
    case '5':
        return 1;
        break;

    case '1':
    case '2':
    case '3':
        return 0;
        break;
    default:
        break;
    }
}

/**
 * @brief send command to ftp server
 * 
 * @param sockfd socket file descriptor
 * @param command command to send
 */
char* send_command(int sockfd, const char *command) {
    char *buffer = (char *)malloc(MAX_CHAR_SIZE); // save response from server
    if (buffer == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    int bytes_sent = write(sockfd, command, strlen(command));
    if (bytes_sent < 0) {
        perror("write()");
        free(buffer); 
        exit(EXIT_FAILURE);
    }

    printf("Sent %d bytes: %s\n", bytes_sent, command);

    int bytes_received = recv(sockfd, buffer, MAX_CHAR_SIZE - 1, 0);

    if (bytes_received < 0) {
        perror("recv()");
        free(buffer); 
        exit(EXIT_FAILURE);
    }

    buffer[bytes_received] = '\0'; // Ensure null-termination

    printf("Received %d bytes: %s \n", bytes_received, buffer);

    return buffer;
}

/**
 * @brief Initialize connection and get first response
 * 
 * @param sockfd 
 */
void initialize_connection(int sockfd) {
    char *buffer[MAX_CHAR_SIZE];
    int bytes_received;
    char *command = "Hello server\r\n";

    int bytes_sent = write(sockfd, command, strlen(command));
    if (bytes_sent < 0) {
        perror("write()");
        exit(EXIT_FAILURE);
    }

    printf("Sent %d bytes: %s\n", bytes_sent, command);

    // Wait till message
    while(!strstr(buffer, "530 Please login with USER and PASS")) {
        bytes_received = recv(sockfd, buffer, MAX_CHAR_SIZE - 1, 0); // Receive message
        buffer[bytes_received] = '\0'; // Null-termination
        if (bytes_received < 0) {
            perror("recv()");
            exit(EXIT_FAILURE);
        }
    }

}

/**
 * @brief Get the new port object from
 *     < 227 Entering Passive Mode (x1,x2,x3,x4,x5,x6)
 * 
 * @param msg recived message
 * @return int 
 */
void get_new_port(const char *response, char *ip_address, int *port_number) {
    // Finding '(' and ')'
    const char *openParen = strchr(response, '(');
    const char *closeParen = strchr(response, ')');

    if (openParen == NULL || closeParen == NULL) {
        fprintf(stderr, "parsing failed, wrong format!\n");
        exit(-1);
    }

    char numbers[MAX_CHAR_SIZE];
    strncpy(numbers, openParen + 1, closeParen - openParen - 1);
    numbers[closeParen - openParen - 1] = '\0'; // add at the end

    // Extract numbers
    int x1, x2, x3, x4, x5, x6;
        sscanf(numbers, "%d,%d,%d,%d,%d,%d", &x1, &x2, &x3, &x4, &x5, &x6);
    
    // Calculate IP address
    sprintf(ip_address, "%d.%d.%d.%d", x1, x2, x3, x4);
        
    // Calculate port number
    *port_number = x5 * 256 + x6;
}

/**
 * @brief listen for the data from server
 * 
 * @param sockfd 
 * @param file 
 */
void receive_data(int sockfd, FILE *file) {
    char *buffer = (char *)malloc(MAX_CHAR_SIZE);
    int bytes_received;
    int counter = 0;

    // Receive data
    while ((bytes_received = recv(sockfd, buffer, MAX_CHAR_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        counter+=bytes_received;
    }

    if (bytes_received == -1) {
        perror("Error receiving data");
    }

    printf("Received %d bytes of file.\n", counter);
}

/**
 * @brief Close connection to the server
 * 
 * @param socked_id_ctrl 
 * @param socked_id_data 
 * @return int 
 */
int close_connection(int socked_id_ctrl, int socked_id_data) {
    send_command(socked_id_ctrl, "quit\r\n");

    // Close socket
    if (close(socked_id_ctrl) < 0 || close(socked_id_data) < 0) {
        perror("close()");
        exit(-1);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    printf("\033[0;32mFTP DOWNLOAD APPLICATION BY Filip and Valentino. FEUP 2024");
    printf("\033[0;37m\n");

    //Parsing of the URL

    char url [MAX_CHAR_SIZE] ;
    char user [MAX_CHAR_SIZE] = "anonymous";
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
            //There is only user.
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> \n");
            exit(-1);
        }

        strcpy(user, token);

        char *rest = strtok(NULL, "");

        if (!rest) {
            //There is no password. 
            
            char *a = strtok(temp, "@");

            strcpy(user, a);

            rest = strtok(NULL, "");

        }

        else {

            char *a = strtok(rest, "@");

            strcpy(password, a);
            
            rest = strtok(NULL, "");

        }

        if (!rest) {
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> \n");
            exit(-1);
        }

        strcpy(url, rest);

        if (!strlen(user) || !strlen(url)){
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> \n");
            exit(-1);
        }

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
            printf("Invalid URL. Format: ftp://[<user>:<password>@]<host>/<url-path> \n");
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

    char *url_resolved = inet_ntoa(*((struct in_addr *)h->h_addr));
    printf("URL Resolved: %s\n", url_resolved);

    // Server address handling
    struct sockaddr_in server_addr_ctrl = get_server_addr(url_resolved, FTP_DATA_PORT);

    // Open a TCP socket
    int socked_id_ctrl = socket(AF_INET, SOCK_STREAM, 0);

    if (socked_id_ctrl < 0) {
        perror("socket()");
        exit(-1);
    }

    // Connect to the server
    printf("Connecting...\n");
    if (connect(socked_id_ctrl, (struct sockaddr *)&server_addr_ctrl, sizeof(server_addr_ctrl)) < 0) {
        perror("connect()");
        exit(-1);
    }

    // Establish a connection for controling
    initialize_connection(socked_id_ctrl);

    /* Login */
    // username
    char *response = "";
    char login_command[MAX_CHAR_SIZE];
    sprintf(login_command, "user %s\r\n", user);
    response = send_command(socked_id_ctrl, login_command); 

    if (strncmp(response, "331 Please specify the password." , 32) != 0) {
        //Not the right answer from the server. Leaving.
        printf("Error: Leaving...\n");
        exit(-1);
    }

    // password
    char password_command[MAX_CHAR_SIZE];

    sprintf(password_command, "pass %s\r\n", password);
    response = send_command(socked_id_ctrl, password_command);

    if (strncmp(response, "230" , 3) != 0) {
        //Not the right answer from the server. Leaving.
        printf("Error while logging in.\n");
        exit(-1);
    }

    // Ask for passive mode and get response
    response = send_command(socked_id_ctrl, "pasv\r\n");

    if (strncmp(response, "530", 3) == 0) {
        printf("Wrong username/password combination.\n");
        exit(-1);
    }

    if (strncmp(response, "227" , 3) != 0) {
        //Not the right answer from the server. Leaving.
        printf("Error while entering pasv\n");
        exit(-1);
    }

    /* Calculate new port port = x5*256 + x6.*/
    int new_port = 0;
    get_new_port(response, url_resolved, &new_port);

    /* Establish a connestion for receiving data*/
    struct sockaddr_in server_addr_data = get_server_addr(url_resolved, new_port);

    int socked_id_data = socket(AF_INET, SOCK_STREAM, 0);

    if (socked_id_data < 0) {
        perror("socket()");
        exit(-1);
    }

    if (connect(socked_id_data, (struct sockaddr *)&server_addr_data, sizeof(server_addr_data)) < 0) {
        perror("connect()");
        exit(-1);
    }   

    // Receive file. 

    char filename[MAX_CHAR_SIZE]; // Command to download the file
    sprintf(filename, "RETR %s\n", url_path);
    response = send_command(socked_id_ctrl, filename);

    // Check for faillure
    if(strstr(response, "550 Failed to open file")) {
        close_connection(socked_id_ctrl, socked_id_data);
        printf("[Error] Failed to open file! Does that file exist?\n");

        exit(-1);
    }

    // Save received file
    FILE *file;
    char *received_file = strrchr(url_path, '/');
    received_file++;
    fprintf(stderr, "file : %s\n", received_file);

    file = fopen(received_file, "wb");
    if (file == NULL) {
        perror("Error opening file");
        exit(-1);
    }

    receive_data(socked_id_data, file);

    fclose(file);

    close_connection(socked_id_ctrl, socked_id_data);

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 9002
#define DATA_PORT 9003
#define BUFFER_SIZE 1024

int network_socket;

void setuser(int sock, char* username);
void setpass(int sock, char* password);

void send_file(int sock, char* filename);
void receive_file(int sock, char* filename);
int create_data_socket();
void handleClientInput(int sock);

void signal_handler(int signum) {
    // to create the semaphors and shared memory if the program is terminated intentionally with Ctrl + Z
    if (signum == SIGINT || signum == SIGTERM) {
		close(network_socket);
		printf("\nSocked closed successfully");
        exit(EXIT_SUCCESS);
    }
}

int main(int argc, char *argv[]) {

	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
	
	char* ftp_server_ip_address = argv[1];
	int ftp_server_port = atoi(argv[2]);

	//create a socket
	//use an int to hold the fd for the socket

	//1st argument: domain/family of socket. For Internet family of IPv4 addresses, we use AF_INET 
	//2nd: type of socket TCP
	//3rd: protocol for connection
	network_socket = socket(AF_INET , SOCK_STREAM, 0);

	//check for fail error
	if (network_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }
    else {
		printf("Client: socket CREATION success..\n");
	}

    if(setsockopt(network_socket,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int))<0) {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }

	//connect to another socket on the other side
	//specify an address for the socket we want to connect to
	// Set up the addressing
	struct sockaddr_in server_address;
	memset(&server_address, '0', sizeof(server_address));
	server_address.sin_family = AF_INET;
	// Convert IPv4 and IPv6 addresses from text to binary form
	printf("Connected to the server at %s:%d\n", ftp_server_ip_address, ftp_server_port);
	server_address.sin_port = htons(ftp_server_port);
	if (inet_pton(AF_INET, ftp_server_ip_address, &server_address.sin_addr) <= 0) {
		perror("Invalid address/ Address not supported");
		return -1;
	}

	//connect
	//1st : socket 
	//2nd: server address structure , cast to a pointer to a sockaddr struct so pass the address
	//3rd: size of address
	int connection_status = 
	connect(network_socket, 
			(struct sockaddr *) &server_address,
			sizeof(server_address));

	//check for errors with the connection
	if(connection_status == -1){
		printf("There was an error making a connection to the remote socket \n\n");
		exit(EXIT_FAILURE);
	}
	else{
		printf("Client: socket CONNECT success..\n");
	}

	//now that we have the connect, we either send or receive data
	//since since is the client, we receive data
	//1st: socket
	//2nd: location for some string to hold data we get back from the server
	//3rd: size of data to be received
	//4th: optional flags , 0 default
	char server_response[256]; //empty string

	handleClientInput(network_socket);

	// recv(network_socket , &server_response , sizeof(server_response),0);
	// print out the data we get back
	// printf("The server sent the data: %s\n" , server_response);
	//close socket after we are done
	close(network_socket);
	return 0;
}


void handleClientInput(int sock){
	char client_command[20];
	printf("ftp-> Waiting for client command :");
	while (1)
	{
		printf("\nftp->");
		scanf("%s\n",client_command);
		if(strcmp(client_command,"user") == 0){
			char username[20];
			scanf("%s",username);
			printf("Settingusername File.....%s",username);
			setuser(sock,username);
		}
		if(strcmp(client_command,"pass") == 0){
			char password[20];
			scanf("%s",password);
			setpass(sock,password);
		}
		if(strcmp(client_command,"put") == 0){
			printf("Sending File.....\n");
			send_file(sock,"textclient.txt");
		}
		if(strcmp(client_command,"get") == 0){
			printf("Getting File.....");
			// receive_file(networksock_socket,"textserver.txt");
		}
	}
}


void setuser(int sock, char* username) {
   char newusername[20] = "u";
   char server_response[256]; //empty string
   strcat(newusername,username);
   send(sock , newusername , sizeof(newusername),0);
   recv(sock , &server_response , sizeof(server_response),0);
   printf("Server response: %s",server_response);
}

void setpass(int sock, char* password) {

   char new_password[20] = "p";
   strcat(new_password,password);
   char server_response[256]; //empty string
 
   send(sock , new_password , sizeof(new_password),0);
   recv(sock , &server_response , sizeof(server_response),0);
   printf("Server response: %s",server_response);
}

// Handle File Transfer
void send_file(int sock, char* filename){

   char message[20] = "f";
   char server_response[256]; //empty string
   send(sock , message , sizeof(message),0);
   recv(sock , &server_response , sizeof(server_response),0);
   printf("Server response: %s",server_response);


   FILE *file = fopen(filename, "rb");
	
	if (file == NULL){
		printf("\nFailed to open the file\n");
		exit(EXIT_FAILURE);
	}

	char buffer[BUFFER_SIZE];
	size_t bytes_read;

	while((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0){
		if (send(sock, buffer, bytes_read, 0) < 0){
			printf("\nfailed to send the file");
			break;
		}
	}
	const char *eof_marker = "EOF";
    send(sock, eof_marker, strlen(eof_marker), 0);

    fclose(file);
    printf("\nFile uploaded successfully.\n");
}

//Download File from the server
void receive_file(int sock, char* filename) {
	char messagetype[20] = "s";
  	send(sock , messagetype , sizeof(messagetype),0); // Send the message identifier

	char server_response[256]; //empty string
  	recv(sock , &server_response , sizeof(server_response),0);
  	printf("\nServer response: %s",server_response);

	send(sock , filename , sizeof(filename),0); // Send the file name;

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open the file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    if (bytes_received < 0) {
        perror("Failed to receive the file");
    }
}


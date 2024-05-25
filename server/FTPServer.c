// Server Code
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

int server_socket;


void handleClient(int sock);
void handleFiletransfer(int sock);
void receive_files(int new_socket);
void send_files(int sock, char* filename);
int create_data_socket();

void signal_handler(int signum) {
    // to create the semaphors and shared memory if the program is terminated intentionally with Ctrl + Z
    if (signum == SIGINT || signum == SIGTERM) {
		printf("\nSocked closed successfully");
		close(server_socket);
        exit(EXIT_SUCCESS);
    }
}


int main()
{
	
	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
	char *custom_ip = "127.0.0.1"; // Custom IP address
	//create a string to hold data we will send to the client
	char server_message[256] = "You have reached the server!";

	//create socket

	server_socket = socket(AF_INET , SOCK_STREAM,0);



	//check for fail error
	if (server_socket == -1) {
        printf("socket creation failed..\n");
        exit(EXIT_FAILURE);
    }
    else{
		printf("Server: socket CREATION success..\n");
	}

    if(setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int))<0) {
        perror("setsockopt error");
        exit(EXIT_FAILURE);
    }
    
	//define server address structure
	struct sockaddr_in server_address;

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = inet_addr(custom_ip);

	//bind the socket to our specified IP and port
	if (bind(server_socket , 
		(struct sockaddr *) &server_address,
		sizeof(server_address)) < 0)
	{
		printf("socket bind failed..\n");
        exit(EXIT_FAILURE);
	}
	 else
    	printf("Server: socket BIND success..\n");
    


	//after it is bound, we can listen for connections
	
	//2nd: how many connections can be waiting for this socket 
	if(listen(server_socket,5)<0){
		printf("Listen failed..\n");
        exit(EXIT_FAILURE);
	}
	 else
    	printf("Server: socket LISTEN success..\n");
    

	//now after listen, socket is a fully functional listening socket

	//once we are able to listen to connections, we can accept connections
	//when we accept a connection, what we get back is the client socket that we will write to	

	int addrlen = sizeof(server_address);	

	int client_socket;
	
	//In the call to accept(), the server is put to sleep and when there is an incoming client request, then the function accept() wakes up and returns the socket descriptor representing the client socket
	//1st: The socket that has been created with socket(), bound to a local address with bind(), and is listening for connections after a listen().
	//2nd: A pointer to the sockaddr structure
	//3rd: addrlen - the size (in bytes) of the sockaddr structure
	client_socket = accept(server_socket,
		(struct sockaddr *) &server_address,
		(socklen_t *) &addrlen);

	if(client_socket<0){
		printf("accept failed..\n");
        exit(EXIT_FAILURE);
	}
	else { 
		printf("Server: socket ACCEPT success..\n");
	}    
	handleClient(client_socket);

	close(server_socket); 
	return 0;
}


void handleClient(int sock){
	char client_message[256];
	char server_message_1[256] = "331 User-name OK, password required";
	char server_message_2[256] = "Password authenticated!";
	char server_message_3[256] = "Ready to receive files!";
	char server_message_4[256] = "Ready to send files!";
	while (1) {
		recv(sock , &client_message , sizeof(client_message),0);
	
		if(strcmp(client_message, "")){
			if(client_message[0]=='u'){
				printf("Server got the client message (type : %c): %s\n",client_message[0],client_message);
				printf("Sending data now: %s\n",server_message_1);
				send(sock , &server_message_1 , sizeof(server_message_1),0);
			}
			if(client_message[0]=='p'){
				printf("Server got the client message (type : %c): %s\n",client_message[0],client_message);
				printf("Sending data now: %s\n",server_message_2);
				send(sock , &server_message_2 , sizeof(server_message_2),0);
			}
			if(client_message[0]=='f'){
				printf("Getting the file from the client: %s\n","..................");
				send(sock , &server_message_3 , sizeof(server_message_3),0);
				// receive_files(sock);
				handleFiletransfer(sock);
			}
			if(client_message[0]=='s'){
				printf("Sending the file to client: %s\n","..................");
				send(sock , &server_message_4 , sizeof(server_message_4),0);
				printf("Filename:%s",client_message);
				// send_files(sock,client_message);
			}
		}
		client_message[0]= '\0';
	}
}

void handleFiletransfer(int sock) {

	char server_message[256]; //empty string
	send(sock , server_message , sizeof(server_message),0);


 // Create data socket
	int data_sock = create_data_socket();
	if (data_sock < 0) {
		perror("Data socket creation failed");
		close(sock);
	}

	send(sock, "150 Opening data connection\r\n", 29, 0);

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	int data_client_sock = accept(data_sock, (struct sockaddr *)&client_addr, &client_addr_len);
	if (data_client_sock < 0) {
		perror("Data accept failed");
		close(data_sock);
		close(sock);
	}

	send_files(data_sock,"textclient.txt");
	// receive_file(data_sock,"textclient.txt");

}


void receive_files(int sock) {

    FILE *file = fopen("received_file.txt", "wb");
    if (file == NULL) {
        perror("Failed to open the file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
		if (strncmp(buffer, "EOF", 3) == 0) {
            break;
        }
		fwrite(buffer, 1, bytes_received, stdout);
        fwrite(buffer, 1, bytes_received, file);
    }

    if (bytes_received < 0) {
        perror("Failed to receive the file");
    }

    fclose(file);
    printf("File received successfully.\n");
}

void send_files(int sock, char* filename) {

  FILE *file = fopen("text.txt", "rb");

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

int create_data_socket() {
    int data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) {
        perror("Data socket creation failed");
        return -1;
    }
    
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_addr.s_addr = INADDR_ANY;
    data_addr.sin_port = htons(DATA_PORT);
    
    if (bind(data_sock, (struct sockaddr *)&data_addr, sizeof(data_addr)) < 0) {
        perror("Data bind failed");
        close(data_sock);
        return -1;
    }
    
    if (listen(data_sock, 1) < 0) {
        perror("Data listen failed");
        close(data_sock);
        return -1;
    }
    
    return data_sock;
}


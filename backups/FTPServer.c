// Server Code
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdbool.h>
#include <dirent.h>
#include <limits.h> 

#define PORT 9002
#define DATA_PORT 9003
#define BUFFER_SIZE 1024
#define MAX_LENGTH 256

int server_socket;


void handleClient(int sock, char* client_message);
void handleFileDownload(int sock , char *filename);
void handleFileUpload(int sock , char *filename);
void receive_files(int data_sock, char* filename);
void send_files(int data_sock, char* filename);
char* listFilesInCurrentDirectory();
char* getCurrentDirectoryPath();
void handle_cd_command(char *command);
bool file_exists(const char *filename);
bool checkUsernameExists(const char* username);
bool checkPasswordExists(const char* password);
void copyExcludingFirstCharacter(const char* original, char* result);
void handleDataClient(int sock, char* client_message);

char server_message_1[256] = "331 Username OK, need password.”";
char server_message_2[256] = "530 Not logged in.";
char server_message_3[256] = "230 User logged in,";
char server_message_4[256] = "530 Not logged in.";
char server_message_5[256] = "Valid User";
char server_message_6[256] = "Valid User";
char server_message_7[256] = "User not authenticated!";
char server_message_8[256] = "221 Service closing control connection";
char server_message_9[256] = "File Found";

bool authenticated = false;

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
	authenticated = true;
	signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
	char *custom_ip = "127.0.0.1"; // Custom IP address
	char server_message[256] = "You have reached the server!";

	//create socket
	server_socket = socket(AF_INET , SOCK_STREAM,0);

	printf("Server fd = %d \n",server_socket);
	
	//check for fail error
	if(server_socket<0)
	{
		perror("socket:");
		exit(EXIT_FAILURE);
	}

	//setsock
	int value  = 1;
	setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	
	//define server address structure
	struct sockaddr_in server_address , client_address;
	socklen_t client_addr_len = sizeof(client_address);

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);
	server_address.sin_addr.s_addr = inet_addr(custom_ip);

	//bind
	if(bind(server_socket, (struct sockaddr*)&server_address,sizeof(server_address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	//listen
	if(listen(server_socket,5)<0)
	{
		perror("listen failed");
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	printf("Server is listening...\n");

	// Create a socket for the data connection
	int data_sock;
    data_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (data_sock < 0) {
        perror("Data socket creation failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

	struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(DATA_PORT);
    data_addr.sin_addr.s_addr = INADDR_ANY;


    // Bind the data socket
    if (bind(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("Data bind failed");
        close(data_sock);
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming data connections
    if (listen(data_sock, 5) < 0) {
        perror("Data listen failed");
        close(data_sock);
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d for data...\n", DATA_PORT);

	//DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
	fd_set all_sockets;
	fd_set ready_sockets;

	int client_sock;
	int data_client_sock;

	//zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	//adds one socket (the current socket) to the fd set of all sockets
	FD_SET(server_socket,&all_sockets);
	FD_SET(data_sock,&all_sockets);

	int addrlen = sizeof(server_address);	
	char buffer[BUFFER_SIZE];
	int max_fd = (server_socket > data_sock) ? server_socket : data_sock;

	while(1) {
		ready_sockets = all_sockets;

		if (select(max_fd + 1, &ready_sockets, NULL, NULL, NULL) < 0) {
            perror("Select error");
            exit(EXIT_FAILURE);
        }

	 	// Check for command connections
        if (FD_ISSET(server_socket, &ready_sockets)) {
            client_sock = accept(server_socket, (struct sockaddr*)&client_address, &client_addr_len);
            if (client_sock < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

          	printf("Client Connected fd = %d \n",client_sock);
            FD_SET(client_sock, &all_sockets);
            if (client_sock > max_fd) {
                max_fd = client_sock;
            }
        }

        // Check for data connections
        if (FD_ISSET(data_sock, &ready_sockets)) {
            data_client_sock = accept(data_sock, (struct sockaddr*)&client_address, &client_addr_len);
            if (data_client_sock < 0) {
                perror("Data accept failed");
                exit(EXIT_FAILURE);
            }

          	printf("Data Socket Created fd = %d \n",client_sock);
            FD_SET(data_client_sock, &all_sockets);
            if (data_client_sock > max_fd) {
                max_fd = data_client_sock;
            }
        }

        // Check existing connections for incoming data
        for (int fd = 0; fd <= FD_SETSIZE; fd++) {
            if (fd != server_socket && fd != data_sock && FD_ISSET(fd, &ready_sockets)) {
                int bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0);
                if (bytes_received <= 0) {
                    if (bytes_received == 0) {
                        printf("Client closed the connection\n");
                    } else {
                        perror("Recv error");
                    }
                    close(fd);
                    FD_CLR(fd, &all_sockets);
                } else {
        

					// Handles the User Authentication and other commands
					handleClient(fd,buffer);


					//Handle DataClients
					if(buffer[0]=='f'){ // For uploading to the server from client
						if(authenticated){
							printf("Getting the file from the client: %s\n","..................");
							send(fd , &server_message_5 , sizeof(server_message_5),0);

							char filename[20];
							recv(fd , &filename , sizeof(filename),0); // Filename received
							printf("Filename:%s\n",filename);
							  // Wait for the data connection to be ready
							while (1) {
								ready_sockets = all_sockets;
								if (select(max_fd + 1, &ready_sockets, NULL, NULL, NULL) < 0) {
									perror("Select error");
									exit(EXIT_FAILURE);
								}

								if (FD_ISSET(data_client_sock, &ready_sockets)) {
									receive_files(data_client_sock, filename); // Buffer is the file name
									close(data_client_sock);
									FD_CLR(data_client_sock, &all_sockets);
									snprintf(buffer, BUFFER_SIZE, "226 Transfer complete. Data socket closed\r\n");
									send(fd, buffer, strlen(buffer), 0);
									break;
								}
							}
						}
						else{
							printf("User not authenticated\n");
							send(fd , &server_message_7 , sizeof(server_message_7),0);
						}
					}
					if(buffer[0]=='s'){
						if(authenticated){
							printf("Sending the file to client: %s\n","..................");
							char filename[20];
							send(fd , &server_message_6 , sizeof(server_message_6),0); // Send Valid user
							recv(fd , &buffer , sizeof(buffer),0); // Filename received
							strcpy(filename,buffer);
							printf("Filename:%s\n",filename);

							if(file_exists(filename)){
								snprintf(buffer, BUFFER_SIZE, "150 File status okay; about to open data connection.\r\n");
                        		send(fd, buffer, strlen(buffer), 0);
                        		printf("Ready to send data connection on port %d\n", DATA_PORT);

								while (1) {
									ready_sockets = all_sockets;
									if (select(max_fd + 1, &ready_sockets, NULL, NULL, NULL) < 0) {
										perror("Select error");
										exit(EXIT_FAILURE);
									}
									if (FD_ISSET(data_sock, &ready_sockets)) {
											data_client_sock = accept(data_sock, NULL, NULL);
											if (data_client_sock < 0) {
												perror("Data accept failed");
												exit(EXIT_FAILURE);
											}
											send_files(data_client_sock, filename);
											close(data_client_sock);
											snprintf(buffer, BUFFER_SIZE, "226 Transfer complete.\r\n");
											send(client_sock, buffer, strlen(buffer), 0);
											break;
									}
								}
							}
							else{
								char msg[20] = "File does not exists";
								send(fd , &msg , sizeof(msg),0);
							}
						}
					else{
						printf("User not authenticated\n");
						send(fd , &server_message_7 , sizeof(server_message_7),0);
					}
				}
            }
        }
    }
	}


	bzero(buffer,sizeof(buffer));			
    close(server_socket);
    close(data_sock);
	return 0;
	
}


void handleClient(int sock, char* client_message) {

	

	if(client_message[0]=='u'){
		printf("Server got the client message (type : %c): %s\n",client_message[0],client_message);
		char username[20];
		copyExcludingFirstCharacter(client_message,username);
		if(checkUsernameExists(username)){
			printf("Sending data now: %s\n",server_message_1);
			send(sock , &server_message_1 , sizeof(server_message_1),0);
		}
		else{
			printf("Sending data now: %s\n",server_message_2);
			send(sock , &server_message_2 , sizeof(server_message_2),0);
		}
	}
	if(client_message[0]=='p'){
		printf("Server got the client message (type : %c): %s\n",client_message[0],client_message);
		char password[20];
		copyExcludingFirstCharacter(client_message,password);
		if(checkPasswordExists(password)){
			printf("Sending data now: %s\n",server_message_3);
			send(sock , &server_message_3 , sizeof(server_message_3),0);
			authenticated = true;
		}
		else{
			printf("Sending data now: %s\n",server_message_4);
			send(sock , &server_message_4 , sizeof(server_message_4),0);
		}
	}
	if(client_message[0]=='l'){ // cd (list)
		if(authenticated){
			char *files = listFilesInCurrentDirectory();
			size_t length = strlen(files);
			files[length] = '\0';
			send(sock , files , length,0);
			free(files); // Don't forget to free the allocated memory
		}
		else{
			printf("User not authenticated\n");
			send(sock , &server_message_7 , sizeof(server_message_7),0);
		}
	}
	if(client_message[0]=='t'){ // cd (pwd)
		if(authenticated){
			char *pathname = getCurrentDirectoryPath();
			size_t length = strlen(pathname);
			pathname[length] = '\0';
			send(sock , pathname , length,0);
			free(pathname); // Don't forget to free the allocated memory
		}
		else{
			printf("User not authenticated\n");
			send(sock , &server_message_7 , sizeof(server_message_7),0);
		}
	}
	if(client_message[0]=='q'){ // cd (server)
		if(authenticated){
			send(sock , &server_message_6 , sizeof(server_message_6),0);
			recv(sock , &client_message , sizeof(client_message),0);
			handle_cd_command(client_message);
		}
		else{
			printf("User not authenticated\n");
			send(sock , &server_message_7 , sizeof(server_message_7),0);
		}
	}
	if(client_message[0]=='z'){ // cd (server)
		if(authenticated){
			send(sock , &server_message_8 , sizeof(server_message_8),0);
		}
		else{
			printf("User not authenticated\n");
			send(sock , &server_message_7 , sizeof(server_message_7),0);
		}
	}
}

void handleDataClient(int sock, char* client_message) {
	if(client_message[0]=='f'){
		if(authenticated){
			printf("Getting the file from the client: %s\n","..................");
			send(sock , &server_message_5 , sizeof(server_message_5),0);

			char filename[20];
			recv(sock , &filename , sizeof(filename),0); // Filename received
			printf("Filename:%s\n",filename);
		}
		else{
			printf("User not authenticated\n");
			send(sock , &server_message_7 , sizeof(server_message_7),0);
		}
	}
	if(client_message[0]=='s'){
		if(authenticated){
			printf("Sending the file to client: %s\n","..................");
			send(sock , &server_message_6 , sizeof(server_message_6),0); // Send Valid user
			recv(sock , &client_message , sizeof(client_message),0); // Filename received
			printf("Filename:%s\n",client_message);

			if(file_exists(client_message)){
				send(sock , &server_message_9 , sizeof(server_message_9),0);
			}
			else{
				char msg[20] = "File does not exists";
				send(sock , &msg , sizeof(msg),0);
			}
		}
		else{
			printf("User not authenticated\n");
			send(sock , &server_message_7 , sizeof(server_message_7),0);
		}
	}


}

//Client downloads from the server
void handleFileDownload(int data_sock , char *filename){
	send_files(data_sock,filename);
}

//Client uploads to the server
void handleFileUpload(int data_sock , char *filename){
	receive_files(data_sock,filename);
}

void receive_files(int data_sock, char* filename){

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open the file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(data_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
		fwrite(buffer, 1, bytes_received, stdout);
    }

    if (bytes_received < 0) {
        perror("Failed to receive the file");
    }

    fclose(file);
    printf("File received successfully.\n");
}

void send_files(int data_sock, char* filename) {


    FILE *file = fopen(filename, "rb");
    if (file == NULL){
        printf("\nFailed to open the file\n");
        exit(EXIT_FAILURE);
    }
	
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0){

		fwrite(buffer, 1, bytes_read, stdout);
        if (send(data_sock, buffer, bytes_read, 0) < 0){
            printf("\nfailed to send the file");
            break;
        }
    }

    fclose(file);
    printf("\nFile sent to client successfully successfully.\n");
}


void copyExcludingFirstCharacter(const char* original, char* result) {
    // Check if the original string is empty
    if (original == NULL || strlen(original) == 0) {
        result[0] = '\0'; // Ensure the result is an empty string
        return;
    }
    
    // Point to the second character of the original string
    const char* src = original + 1;
    
    // Copy the rest of the string to the result
    strcpy(result, src);
};

char* listFilesInCurrentDirectory() {
    DIR *dir;
    struct dirent *entry;
    size_t buffer_size = 1024;
    size_t buffer_length = 0;
    char *result = malloc(buffer_size);
    if (!result) {
        perror("Unable to allocate memory");
        exit(EXIT_FAILURE);
    }
    result[0] = '\0'; // Start with an empty string

    dir = opendir(".");
    if (dir == NULL) {
        perror("Unable to open directory");
        free(result);
        exit(EXIT_FAILURE);
    }
    while ((entry = readdir(dir)) != NULL) {
        size_t entry_length = strlen(entry->d_name) + 1; // +1 for the newline character
        // Check if we need more space
        if (buffer_length + entry_length + 1 > buffer_size) { // +1 for the null terminator
            buffer_size *= 2; // Double the buffer size
            result = realloc(result, buffer_size);
            if (!result) {
                perror("Unable to reallocate memory");
                closedir(dir);
                exit(EXIT_FAILURE);
            }
        }
        strcat(result, entry->d_name);
        strcat(result, "\n");
        buffer_length += entry_length;
    }
    closedir(dir);
    return result;
}

char* getCurrentDirectoryPath() {
    char *buffer = malloc(PATH_MAX);
    if (buffer == NULL) {
        perror("Unable to allocate buffer");
        return NULL;
    }
    if (getcwd(buffer, PATH_MAX) != NULL) {
        return buffer;
    } else {
        perror("getcwd() error");
        free(buffer);
        return NULL;
    }
}

void handle_cd_command(char *command) {
    if (chdir(command) < 0) {
        perror("chdir failed");
    }
}

bool file_exists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

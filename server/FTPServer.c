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
#define MAX_LINE_LENGTH 256

int server_socket;


void handleClient(int sock);
void handleDataSocket(int sock ,char *filename, bool value);
void handleFileDownload(int sock , char *filename);
void handleFileUpload(int sock , char *filename);
void receive_files(int data_sock, char* filename);
void send_files(int data_sock, char* filename);
char* listFilesInCurrentDirectory();
int create_data_socket();
char* getCurrentDirectoryPath();
void handle_cd_command(char *command);
bool file_exists(const char *filename);

bool checkUsernameExists(const char* username);
bool checkPasswordExists(const char* password);
void copyExcludingFirstCharacter(const char* original, char* result);

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
	char server_message_1[256] = "331 Username OK, need password.”";
	char server_message_2[256] = "530 Not logged in.";
	char server_message_3[256] = "230 User logged in,";
	char server_message_4[256] = "530 Not logged in.";
	char server_message_5[256] = "Valid User";
	char server_message_6[256] = "Valid User";
	char server_message_7[256] = "User not authenticated!";
	char server_message_8[256] = "221 Service closing control connection";
	char server_message_9[256] = "File Found";

	while (1) {
		recv(sock , &client_message , sizeof(client_message),0);
		if(strcmp(client_message, "")){
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
			if(client_message[0]=='f'){
				if(authenticated){
					printf("Getting the file from the client: %s\n","..................");
					send(sock , &server_message_5 , sizeof(server_message_5),0);
					recv(sock , &client_message , sizeof(client_message),0);
					printf("Filename:%s\n",client_message);
					handleDataSocket(sock,client_message,false); // Client Upload to server
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
						handleDataSocket(sock,client_message,true); // Client Download to server
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
		client_message[0]= '\0';
	}
}

void handleDataSocket(int sock ,char *filename, bool value) { // True : Upload , False Download

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

	if(value == true){
		handleFileDownload(data_client_sock,filename); // Download from server
	}
	else{
		handleFileUpload(data_client_sock,filename); // Upload to server
	}

	send(sock, "150 Closing data connection\r\n", 29, 0);
	close(data_sock);

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
        if (send(data_sock, buffer, bytes_read, 0) < 0){
            printf("\nfailed to send the file");
            break;
        }
    }
	
    fclose(file);
	close(data_sock); // Close the data socket
    printf("\nFile sent to client successfully successfully.\n");
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

bool checkUsernameExists(const char* username){
	 FILE *file = fopen("users.txt", "r");
    if (!file) {
        perror("Error opening file");
        return false;
    }

    char line[MAX_LINE_LENGTH];
    char prefix[] = "username:";
    size_t prefix_len = strlen(prefix);

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, prefix, prefix_len) == 0) {
            char *file_username = line + prefix_len;
            // Remove the newline character at the end of the username, if it exists
            file_username[strcspn(file_username, "\n")] = 0;

            if (strcmp(file_username, username) == 0) {
                fclose(file);
                return true;
            }
        }
    }

    fclose(file);
    return false;
}


bool checkPasswordExists(const char* password){
	 FILE *file = fopen("users.txt", "r");
    if (!file) {
        perror("Error opening file");
        return false;
    }

    char line[MAX_LINE_LENGTH];
    char prefix[] = "password:";
    size_t prefix_len = strlen(prefix);

    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, prefix, prefix_len) == 0) {
            char *file_password = line + prefix_len;
            // Remove the newline character at the end of the password, if it exists
            file_password[strcspn(file_password, "\n")] = 0;

            if (strcmp(file_password, password) == 0) {
                fclose(file);
                return true;
            }
        }
    }

    fclose(file);
    return false;
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

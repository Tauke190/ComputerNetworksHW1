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

	//zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	//adds one socket (the current socket) to the fd set of all sockets
	FD_SET(server_socket,&all_sockets);
	FD_SET(data_sock,&all_sockets);

	int addrlen = sizeof(server_address);	
	char buffer[BUFFER_SIZE];
	
	while(1){
		ready_sockets = all_sockets ; // Keep track of sockets we want to watch
		if(select(FD_SETSIZE,&ready_sockets,NULL,NULL,NULL)<0)
		{
			perror("select error");
			exit(EXIT_FAILURE);
		}

		//Check if there is a new command connection	
		for(int fd = 0 ; fd < FD_SETSIZE; fd++)
		{
			//check to see if that fd is SET
			if(FD_ISSET(fd,&ready_sockets))
			{
				//if it is set, that means that fd has data that we can read right now
				//when this happens, we are interested in TWO CASES
				
				//1st case: the fd is our server socket
				//that means it is telling us there is a NEW CONNECTION that we can accept
				if(fd==server_socket)
				{
					//accept that new connection
					int client_sd = accept(server_socket,0,0);
					printf("Client Connected fd = %d \n",client_sd);

					//add the newly accepted socket to the set of all sockets that we are watching
					FD_SET(client_sd,&all_sockets);
					
				}
				if(fd==data_sock)
				{
					//accept that new connection
					int data_client_sd = accept(data_sock,0,0);
					printf("Data Socket opened Connected fd = %d \n",data_client_sd);
					
					//add the newly accepted socket to the set of all sockets that we are watching
					FD_SET(data_client_sd,&all_sockets);
					
				}

				//2nd case: when the socket that is ready to read from is one from the all_sockets fd_set
				//in this case, we just want to read its data
				else
				{	
					char buffer[256];
					bzero(buffer,sizeof(buffer));
					int bytes = recv(fd,buffer,sizeof(buffer),0);

					if(bytes==0)   //client has closed the connection
					{
						printf("connection closed from client side \n");
							
						//we are done, close fd
						close(fd);

						//once we are done handling the connection, remove the socket from the list of file descriptors that we are watching
						
						FD_CLR(fd,&all_sockets);
				 	}
					// //displaying the message received 
					handleClient(fd,buffer);
				}
			}
		}
	}



	

	close(server_socket); 
	return 0;
}


void handleClient(int sock, char* client_message){


	char server_message_1[256] = "331 Username OK, need password.”";
	char server_message_2[256] = "530 Not logged in.";
	char server_message_3[256] = "230 User logged in,";
	char server_message_4[256] = "530 Not logged in.";
	char server_message_5[256] = "Valid User";
	char server_message_6[256] = "Valid User";
	char server_message_7[256] = "User not authenticated!";
	char server_message_8[256] = "221 Service closing control connection";
	char server_message_9[256] = "File Found";

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



//Client downloads from the server
void handleFileDownload(int data_sock , char *filename){
	send_files(data_sock,filename);
}

//Client uploads to the server
void handleFileUpload(int data_sock , char *filename){
	// receive_files(data_sock,filename);
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



bool checkUsernameExists(const char* username){

	char line[MAX_LENGTH * 2];
    char user[MAX_LENGTH], pwd[MAX_LENGTH];
    
    // Open the file
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Error opening file");
        return false;
    }
    
    // Loop through each line in the file
    while (fgets(line, sizeof(line), file)) {
        // Parse the username and password from the line
        sscanf(line, "%49[^,],%49s", user, pwd);
        
        // Check if the username matches
        if (strcmp(user, username) == 0) {
            fclose(file);
            return true;
        }
    }
    
    // Close the file
    fclose(file);
    return false;
}


bool checkPasswordExists(const char* password){

    char line[MAX_LENGTH * 2];
    char user[MAX_LENGTH], pwd[MAX_LENGTH];
    
    // Open the file
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Error opening file");
        return false;
    }
    
    // Loop through each line in the file
    while (fgets(line, sizeof(line), file)) {
        // Parse the username and password from the line
        sscanf(line, "%49[^,],%49s", user, pwd);
        
        // Check if the password matches
        if (strcmp(pwd, password) == 0) {
            fclose(file);
            return true;
        }
    }
    
    // Close the file
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

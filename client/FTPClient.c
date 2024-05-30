#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <dirent.h>


#define CMD_PORT 9002
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

void upload_file(int data_sock, const char* filename);
void download_file(int data_sock, const char* filename);
int handle_data_client(int cmd_sock);
bool file_exists(const char *filename);
void listFilesInCurrentDirectory();
void displayCurrentDirectory();
void handle_cd_command(char *command);

int data_port;

int main() {
    int cmd_sock;
    struct sockaddr_in server_addr;
    char command[BUFFER_SIZE];
    char server_response[BUFFER_SIZE];

    // Connect to command socket
    if ((cmd_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Command socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CMD_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(cmd_sock);
        exit(EXIT_FAILURE);
    }

    if (connect(cmd_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Command socket connection failed");
        close(cmd_sock);
        exit(EXIT_FAILURE);
    }

    if (read(cmd_sock, server_response, BUFFER_SIZE) > 0) {
        printf("%s\n", server_response);
    }

    while (1) {
        printf("%s","ftp-> ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = '\0';  // Remove newline character

        char *cmd = strtok(command, " ");   // First argument
        char *filename = strtok(NULL, " "); // Second argument

        if(strcmp(cmd,"STOR")==0 && !file_exists(filename)){
            printf("%s","File name does not exists , Try Again\n");
            continue;
        }
        
        if (cmd && filename) {
            if (strcmp(cmd, "USER") == 0) {
                char cmd_buffer[BUFFER_SIZE];  
                snprintf(cmd_buffer, sizeof(cmd_buffer), "USER %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);
            } 
            else if (strcmp(cmd, "PASS") == 0) {
                char cmd_buffer[BUFFER_SIZE];  
                snprintf(cmd_buffer, sizeof(cmd_buffer), "PASS %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);
            } 
            else if(strcmp(cmd, "STOR") == 0) {
                
                char cmd_buffer[BUFFER_SIZE];  
                snprintf(cmd_buffer, sizeof(cmd_buffer), "STOR %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);

                char server_response[256]; //empty string
                recv(cmd_sock , &server_response , sizeof(server_response),0);
                printf("%s\n",server_response);
                if(strcmp(server_response,"Valid User")==0){
                    int data_client_sock = handle_data_client(cmd_sock); // handle the data socket
                    upload_file(data_client_sock, filename);
                    printf("Data socket closed on port : %d\n", data_port);
                    close(data_client_sock);
                }
                
            } 
            else if (strcmp(cmd, "RETR") == 0) {
                char cmd_buffer[BUFFER_SIZE];  // Send to Server
                snprintf(cmd_buffer, sizeof(cmd_buffer), "RETR %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);
              
                char server_response[256]; //empty string
                recv(cmd_sock , &server_response , sizeof(server_response),0);
                printf("%s\n",server_response);
                if(strcmp(server_response,"Valid User")==0){
                    int data_client_sock = handle_data_client(cmd_sock); // handle the data socket
                    download_file(data_client_sock, filename);
                    close(data_client_sock);
                    printf("Data socket closed on port : %d\n", data_port);
                }
            }
            else if (strcmp(cmd, "CWD") == 0) {
                char cmd_buffer[BUFFER_SIZE];  
                snprintf(cmd_buffer, sizeof(cmd_buffer), "CWD %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);
            }
            else if (strcmp(cmd, "!CWD") == 0) {
                if (chdir(filename) < 0) {
                    perror("chdir failed");
                }
                else{
                    printf("Sucessfully changed to directory : %s\n",filename);
                }
                continue;
            } 
            else{
                printf("%s","Invalid command format. Use USER <username> PASS <password> STOR <filename> or RETR <filename>.\n");
                continue;
            }
        } else if(cmd){
            if (strcmp(cmd, "LIST") == 0) {
                char cmd_buffer[BUFFER_SIZE];  
                snprintf(cmd_buffer, sizeof(cmd_buffer), "LIST %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);
            }
            else if (strcmp(cmd, "!LIST") == 0) {
                listFilesInCurrentDirectory();
                continue;
            }
            else if (strcmp(cmd, "PWD") == 0) {
                char cmd_buffer[BUFFER_SIZE];  
                snprintf(cmd_buffer, sizeof(cmd_buffer), "PWD %s", filename);
                send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);
            }
            else if (strcmp(cmd, "!PWD") == 0) {
                displayCurrentDirectory();
                continue; 
            }
           
            else if (strcmp(cmd, "QUIT") == 0) {
               break;
            }
            else{
                printf("%s","Invalid command format. Use USER <username> PASS <password> STOR <filename> or RETR <filename>.\n");
                continue;
            }
        }
        else{
            printf("%s","Invalid command format. Use STOR <filename> or RETR <filename>.\n");
        }
        if (read(cmd_sock, server_response, BUFFER_SIZE) > 0) {
            printf("%s\n", server_response);
        }
    }

    close(cmd_sock);
    return 0;
}

int handle_data_client(int cmd_sock) {
    char commandbuffer[BUFFER_SIZE];
    int port, p1, p2;
    int data_sock;
    struct sockaddr_in data_address;
    socklen_t data_len = sizeof(data_address);

    // Create data socket
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Data socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind the data socket to an available port
    data_address.sin_family = AF_INET;
    data_address.sin_addr.s_addr = INADDR_ANY;
    data_address.sin_port = htons(0); // Let the OS choose the port

    if (bind(data_sock, (struct sockaddr*)&data_address, sizeof(data_address)) < 0) {
        perror("Data socket bind failed");
        close(data_sock);
        exit(EXIT_FAILURE);
    }

    // Get the dynamic port assigned
    if (getsockname(data_sock, (struct sockaddr*)&data_address, &data_len) < 0) {
        perror("getsockname failed");
        close(data_sock);
        exit(EXIT_FAILURE);
    }
    port = ntohs(data_address.sin_port);
    p1 = port / 256;
    p2 = port % 256;

    data_port = port;

    sprintf(commandbuffer, "PORT 127,0,0,1,%d,%d", p1, p2);
    printf("Data socket created on port : %d\n", port);
    // printf("%s\n", commandbuffer);
    send(cmd_sock, commandbuffer, strlen(commandbuffer), 0);

    if (listen(data_sock, 1) < 0) {
        perror("Data socket listen failed");
        close(data_sock);
        exit(EXIT_FAILURE);
    }

    // Accept the connection from the server
    int conn_sock = accept(data_sock, (struct sockaddr*)&data_address, &data_len);
    if (conn_sock < 0) {
        perror("Data socket accept failed");
        close(data_sock);
        exit(EXIT_FAILURE);
    }

    close(data_sock);
    return conn_sock;
}

void upload_file(int data_sock, const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        close(data_sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        // fwrite(buffer, 1, bytes_read, stdout);
        send(data_sock, buffer, bytes_read, 0);
        
    }

    fclose(file);
    close(data_sock);
    printf("%s","\nFile uploaded successfully.\n");
}

void download_file(int data_sock, const char* filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File open failed");
        close(data_sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(data_sock, buffer, BUFFER_SIZE)) > 0) {
        fwrite(buffer, 1, bytes_read, file);
    }

    fclose(file);
}

bool file_exists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

void listFilesInCurrentDirectory() {
    DIR *dir;
    struct dirent *entry;

    // Open the current directory
    dir = opendir(".");
    if (dir == NULL) {
        perror("Unable to open directory");
        exit(EXIT_FAILURE);
    }

    printf("%s","Files in current client directory:\n");

    // Read the directory entries
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }
    // Close the directory
    closedir(dir);
}

void displayCurrentDirectory() {
    char buffer[1024]; // Buffer to hold the path
    if (getcwd(buffer, sizeof(buffer)) != NULL) {
        printf("Current Directory: %s\n", buffer);
    } else {
        perror("getcwd() error");
    }
}

void handle_cd_command(char *command) {
    if (chdir(command) < 0) {
        perror("chdir failed");
    }	
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <dirent.h>


void loadUserFile();
#define USERMAX 1024  // max number of users that can be read from file
#define MAX_LENGTH 256

#define CMD_PORT 9002
#define BUFFER_SIZE 1024

void handle_client(int cmd_sock);
void handle_stor(int data_sock, const char* filename);
void handle_retr(int data_sock, const char* filename);
void ftpUserCmd(int i, char *resDat);
void ftpPassCmd(int i, char *resDat , char username[256]);
bool isAuthenticated(int i) ;
bool file_exists(const char *filename);
char* listFilesInCurrentDirectory();
char* getCurrentDirectoryPath();
void handle_cd_command(char *command);



struct ClientStruct{
    int userIndex;
    int userCurrentDataPort;
    bool username;
    bool password;
    char currDir[256];
    char *clientIDAddrs;
};

struct acc {
  char user[256];
  char pw[256];
};

struct ClientStruct listOfConnectedClients[FD_SETSIZE];
// variable to store accepted user accounts
static struct acc accFile[USERMAX];
// variable to hold the number of users read from file
int userCount = 0;


int main() {

    int cmd_sock, new_cmd_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    fd_set readfds;
    int max_sd;

    loadUserFile();

    // Create command socket
    if ((cmd_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Command socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(cmd_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(CMD_PORT);

    if (bind(cmd_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Command socket bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(cmd_sock, 3) < 0) {
        perror("Command socket listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Minimal FTP server listening on port %d for commands\n", CMD_PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(cmd_sock, &readfds);
        max_sd = cmd_sock;

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(cmd_sock, &readfds)) {
            if ((new_cmd_sock = accept(cmd_sock, (struct sockaddr*)&client_addr, &client_len)) < 0) {
                perror("Command socket accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New client connected\n");
            send(new_cmd_sock, "New client connected\n", strlen("New client connected\n"), 0);

            if (fork() == 0) {
                close(cmd_sock);
                handle_client(new_cmd_sock);
                close(new_cmd_sock);
                exit(0);
            } else {
                close(new_cmd_sock);
            }
        }
    }

    close(cmd_sock);
    return 0;
}

void handle_client(int cmd_sock) {
    char buffer[BUFFER_SIZE];
    char username[BUFFER_SIZE];
    int valread;
    struct sockaddr_in data_addr;
    int data_sock , data_port;

    bool isupload;
    char filename[20];
    while ((valread = read(cmd_sock, buffer, BUFFER_SIZE)) > 0) {
        buffer[valread] = '\0';
        char *cmd = strtok(buffer, " ");
        char *arg = strtok(NULL, " ");

        chdir(listOfConnectedClients[cmd_sock].currDir);

        if (cmd) {
            if (strcmp(cmd, "USER") == 0) {
                printf("Received USER command: %s\n", arg);
                strcpy(username,arg);
                ftpUserCmd(cmd_sock,arg); // arg = username
            }
            else if(strcmp(cmd, "PASS") == 0) {
                printf("Received PASS command: %s\n", arg);
                ftpPassCmd(cmd_sock,arg,username); // arg = password
            }
            else if(strcmp(cmd, "PORT") == 0) {
              if(isAuthenticated(cmd_sock)){
                printf("Received PORT command: %s\n", arg);
                int ip1, ip2, ip3, ip4, p1, p2;
                sscanf(arg, "%d,%d,%d,%d,%d,%d", &ip1, &ip2, &ip3, &ip4, &p1, &p2);
                data_port = p1 * 256 + p2;
                printf("Data port %d:\n",data_port);

                  // save
                listOfConnectedClients[cmd_sock].userCurrentDataPort = data_port;
                // listOfConnectedClients[i].clientIPAddr = ipAdr;

                // Prepare data socket for connection
                data_sock = socket(AF_INET, SOCK_STREAM, 0);
                if (data_sock == 0) {
                    perror("Data socket creation failed");
                    continue;
                }

                data_addr.sin_family = AF_INET;
                data_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                data_addr.sin_port = htons(data_port);

                if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
                    perror("Data socket connect failed");
                    close(data_sock);
                    continue;
                }
                send(cmd_sock, "200 PORT Sucess: New data port open \n", strlen("2200 PORT Sucess: New data port open \n"), 0);

                if(isupload){
                    handle_stor(data_sock, filename); 
                }
                else{
                    handle_retr(data_sock, filename); 
                } 
                close(data_sock);
                // send(cmd_sock, "Data PORT closed \n", strlen("Data PORT closed \n"), 0);
              }
            } else if (strcmp(cmd, "STOR") == 0) {
                 printf("Received STOR command: %s\n", arg);
                 if (isAuthenticated(cmd_sock)) {
                    isupload = true;
                    strcpy(filename,arg);
                    char corResponse[BUFFER_SIZE] = "Valid User";
                    send(cmd_sock, corResponse, sizeof(corResponse), 0);
                   
                 }
                 else{
                    char corResponse[BUFFER_SIZE] = "530 Not logged in.";
                    send(cmd_sock, corResponse, sizeof(corResponse), 0);
                 }
            } else if (strcmp(cmd, "RETR") == 0) {
                printf("Received RETR command: %s\n", arg);
                 if(isAuthenticated(cmd_sock)) {
                    isupload = false;
                    strcpy(filename,arg);
                    
                    if(file_exists(filename)){
                      char corResponse[BUFFER_SIZE] = "Valid User";
                      send(cmd_sock, corResponse, sizeof(corResponse), 0);
                    }
                    else{
                      char corResponse[BUFFER_SIZE] = "File does not exists";
                      send(cmd_sock, corResponse, sizeof(corResponse), 0);
                    }
                 }
                 else{
                    char corResponse[BUFFER_SIZE] = "530 Not logged in.";
                    send(cmd_sock, corResponse, sizeof(corResponse), 0);
                 }
            } else if(strcmp(cmd, "LIST") == 0) {
              printf("Received LIST command: %s\n", arg);

                if(isAuthenticated(cmd_sock)){
                  char *files = listFilesInCurrentDirectory();
                  size_t length = strlen(files);
                  files[length] = '\0';
                  send(cmd_sock , files , length,0);
                  free(files); // Don't forget to free the allocated memory
                }
                else{
                    char corResponse[BUFFER_SIZE] = "530 Not logged in.";
                    send(cmd_sock, corResponse, sizeof(corResponse), 0);
                }
            }
             else if(strcmp(cmd, "PWD") == 0) {
                if(isAuthenticated(cmd_sock)){
                  printf("Received PWD command: %s\n", arg);
                  char *files = getCurrentDirectoryPath();
                  size_t length = strlen(files);
                  files[length] = '\0';
                  send(cmd_sock , files , length,0);
                  free(files); // Don't forget to free the allocated memory
                }
                else{
                    char corResponse[BUFFER_SIZE] = "530 Not logged in.";
                    send(cmd_sock, corResponse, sizeof(corResponse), 0);
                }
               
            }
            else if(strcmp(cmd, "CD") == 0) {
              printf("Received LIST command: %s\n", arg);
              if(isAuthenticated(cmd_sock)){
                  handle_cd_command(arg);
                  char corResponse[BUFFER_SIZE] = "Changed directory sucessfully /";
                  strncat(corResponse, arg, BUFFER_SIZE - strlen(corResponse) - 1);
                  send(cmd_sock, corResponse, sizeof(corResponse), 0);
              }
              else{
                  char corResponse[BUFFER_SIZE] = "530 Not logged in.";
                  send(cmd_sock, corResponse, sizeof(corResponse), 0);
              }
            }
            else{

            }
        }
    }
}

void handle_stor(int data_sock, const char* filename) {

  
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File open failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = read(data_sock, buffer, BUFFER_SIZE)) > 0) {
         fwrite(buffer, 1, bytes_read, file);
    }
    
    fclose(file);
}

void handle_retr(int data_sock, const char* filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        write(data_sock, buffer, bytes_read);
    }

    fclose(file);
}


void loadUserFile() {
  FILE *userFile = fopen("users.txt", "r");
  if (!userFile) {
    // if user file does not exist
    userCount = 0;
    printf("User.txt file not found. \n");
    return;
  }
  int strCount = 0;
  char str = ' ';
  // get the number of users
  while (!feof(userFile)) {
    str = fgetc(userFile);
    if (str == '\n') {
      userCount += 1;
    }
  }
  rewind(userFile);
  // store in the variable
  while (strCount < userCount + 1) {
    fscanf(userFile, "%s %s", accFile[strCount].user, accFile[strCount].pw);
    strCount += 1;
  }
}

void ftpUserCmd(int i, char *resDat) {
  bool foundDat = false;
  // loop to check if user exists
  for (int n = 0; n < userCount; n++) {
    if (strcmp(resDat, accFile[n].user) == 0) {
      // if found
      foundDat = true;
      listOfConnectedClients[i].userIndex = n;  // found at nth pos in array
      listOfConnectedClients[i].username = true;
      char corResponse[BUFFER_SIZE] = "331 Username OK, need password.";
      send(i, corResponse, sizeof(corResponse), 0);
      break;
    }
  }
  // if not found
  if (!foundDat) {
    char corResponse[BUFFER_SIZE] = "530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  }
}

// PASS command, i = socket
void ftpPassCmd(int i, char *resDat , char username[256]) {
  // check if USER command is run first
  if (!listOfConnectedClients[i].username) {
    char corResponse[BUFFER_SIZE] = " 530 Not logged in.";
    send(i, corResponse, sizeof(corResponse), 0);
  } else {
    // check if password is correct
    if (strcmp(resDat, accFile[listOfConnectedClients[i].userIndex].pw) == 0) {
      char corResponse[BUFFER_SIZE] = "230 User logged in, proceed.";
      listOfConnectedClients[i].password = true;
      strcpy(listOfConnectedClients[i].currDir,username);
      send(i, corResponse, sizeof(corResponse), 0);
    } else {
      char corResponse[BUFFER_SIZE] = "530 Not logged in.";
      send(i, corResponse, sizeof(corResponse), 0);
    }
  }
}

bool isAuthenticated(int i) {
  if (!listOfConnectedClients[i].password || !listOfConnectedClients[i].username) {
    // not authenticated
    return false;
  }
  return true;
}

bool file_exists(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;

}
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
    char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        perror("Unable to allocate buffer");
        return NULL;
    }
    if (getcwd(buffer, BUFFER_SIZE) != NULL) {
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
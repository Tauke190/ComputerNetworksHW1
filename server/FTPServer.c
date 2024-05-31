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
#include <ctype.h>


void loadUserfromfile();
#define USERMAX 1024  // max number of users that can be read from file
#define MAX_LENGTH 256

#define CMD_PORT 9004
#define BUFFER_SIZE 1024

void handle_client(int cmd_sock , char buffer[BUFFER_SIZE]);
void handlefilestore(int data_sock, const char* filename);
void handlefileretrieve(int data_sock, const char* filename);
void handleUserCommand(int i, char *resDat);
void handlePassCommand(int i, char *resDat , char username[256]);
bool isAuthenticated(int i) ;
bool file_exists(const char *filename);
char* listFilesInCurrentDirectory();
char* getCurrentDirectoryPath();
void handle_cd_command(char *command);
char* trimWhitespace(char *str);
void processInput(const char *input_string, char **cmd, char **arg);

 char * server_root;  // PATH_MAX is the maximum length of a path in the system



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

int main()
{
  server_root  = getCurrentDirectoryPath();

	int server_socket = socket(AF_INET,SOCK_STREAM,0);
	printf("Server fd = %d \n",server_socket);
	
	//check for fail error
	if(server_socket<0)
	{
		perror("socket:");
		exit(EXIT_FAILURE);
	}

  loadUserfromfile();

	//setsock
	int value  = 1;
	setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&value,sizeof(value)); //&(int){1},sizeof(int)
	
	//define server address structure
	struct sockaddr_in server_address;
	bzero(&server_address,sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(CMD_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;


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
	

	//DECLARE 2 fd sets (file descriptor sets : a collection of file descriptors)
	fd_set all_sockets;
	fd_set ready_sockets;


	//zero out/iniitalize our set of all sockets
	FD_ZERO(&all_sockets);

	//adds one socket (the current socket) to the fd set of all sockets
	FD_SET(server_socket,&all_sockets);


	printf("Server is listening...\n");

	while(1)
	{		
		//so that is why each iteration of the loop, we copy the all_sockets set into that temp fd_set
		ready_sockets = all_sockets;

		if(select(FD_SETSIZE,&ready_sockets,NULL,NULL,NULL)<0)
		{
			perror("select error");
			exit(EXIT_FAILURE);
		}

		//when select returns, we know that one of our file descriptors has work for us to do
		//but which one??
		//select returns the fd_set containing JUST the file descriptors ready for reading
		//(because select is destructive, so that is why we made the temp fd_set ready_sockets copy because we didn't want to lose the original set of file descriptors that we are watching)
		
		//to know which ones are ready, we have to loop through and check
		//go from 0 to FD_SETSIZE (the largest number of file descriptors that we can store in an fd_set)
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
          send(client_sd, "220 Service ready for new user\n", strlen("220 Service ready for new user\n"), 0);
        
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
          else{
             handle_client(fd,buffer);
          }
				}
			}
		}

	}

	//close
  free(server_root);
	close(server_socket);
	return 0;
}


void handle_client(int cmd_sock , char *buffer){
    char username[BUFFER_SIZE];
    struct sockaddr_in data_addr;
    int data_sock , data_port;

    bool isupload;
    char filename[20];

    char *cmd = NULL;
    char *arg = NULL;
    processInput(buffer, &cmd, &arg);
    if (strcmp(cmd, "STOR") == 0) 
    {
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
    }
    if (strcmp(cmd, "RETR") == 0) 
    {
      printf("Received RETR command: %s\n", arg);
      if (isAuthenticated(cmd_sock)) {
          if(chdir(listOfConnectedClients[cmd_sock].currDir) >= 0){ // If file exists in the server
            if(file_exists(arg)){
              isupload = false;
              strcpy(filename,arg);
              char corResponse[BUFFER_SIZE] = "Valid User";
              send(cmd_sock, corResponse, sizeof(corResponse), 0);
             
            }
            else{
                char corResponse[BUFFER_SIZE] = "File does not exists in the server";
                send(cmd_sock, corResponse, sizeof(corResponse), 0);
            }
           
          }
          else{
                char corResponse[BUFFER_SIZE] = "Invalid Directory";
                send(cmd_sock, corResponse, sizeof(corResponse), 0);
          }
          chdir(server_root);
      }
      else{
        char corResponse[BUFFER_SIZE] = "530 Not logged in.";
        send(cmd_sock, corResponse, sizeof(corResponse), 0);
      }
    }
    if (strcmp(cmd, "USER") == 0) 
    {
        printf("Received USER command: %s\n", arg);
        strcpy(username,arg);
        handleUserCommand(cmd_sock,arg); // arg = username
    }
    if(strcmp(cmd, "PASS") == 0) 
    {
        printf("Received PASS command: %s\n", arg);
        handlePassCommand(cmd_sock,arg,username); // arg = password
    }
    if(strcmp(cmd, "PORT") == 0) 
    {
     
        if(isAuthenticated(cmd_sock)){
           pid_t pid = fork();
          if(pid == 0) {
                  close(cmd_sock);
                  printf("Received PORT command: %s\n", arg);
                  int ip1, ip2, ip3, ip4, p1, p2;
                  sscanf(arg, "%d,%d,%d,%d,%d,%d", &ip1, &ip2, &ip3, &ip4, &p1, &p2);
                  data_port = p1 * 256 + p2;
                
                  listOfConnectedClients[cmd_sock].userCurrentDataPort = data_port;
                  // listOfConnectedClients[i].clientIPAddr = ipAdr;

                  // Prepare data socket for connection
                  data_sock = socket(AF_INET, SOCK_STREAM, 0);
                  if (data_sock == 0) {
                      perror("Data socket creation failed");   
                  }

                  data_addr.sin_family = AF_INET;
                  data_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                  data_addr.sin_port = htons(data_port);

                  if (connect(data_sock, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
                      perror("Data socket connect failed");
                      close(data_sock);
                    
                  }
                  printf("Data socked opened on port :%d\n",data_port);
                  // send(cmd_sock, "200 PORT Sucess: New data port open \n", strlen("2200 PORT Sucess: New data port open \n"), 0);


                  chdir(listOfConnectedClients[cmd_sock].currDir);
                  if(isupload){
                      handlefilestore(data_sock, filename); 
                  }
                  else{
                      handlefileretrieve(data_sock, filename); 
                  }
                  //send(cmd_sock, "Data PORT closed \n", strlen("Data PORT closed \n"), 0);
                }
              else{
                  close(data_sock);
                  chdir(server_root); 
              }
        }
    }
    if(strcmp(cmd, "LIST") == 0) {
      printf("Received LIST command: %s\n", arg);

        if(isAuthenticated(cmd_sock)){

            if(chdir(listOfConnectedClients[cmd_sock].currDir) >= 0){
              char *files = listFilesInCurrentDirectory();
              size_t length = strlen(files);
              files[length] = '\0';
              send(cmd_sock , files , length,0);
              free(files); // Don't forget to free the allocated memory
              chdir(server_root);
            }
            else{
                char corResponse[BUFFER_SIZE] = "Invalid Directory";
                send(cmd_sock, corResponse, sizeof(corResponse), 0);
            }
        }
        else{
            char corResponse[BUFFER_SIZE] = "530 Not logged in.";
            send(cmd_sock, corResponse, sizeof(corResponse), 0);
        }
    }
    if(strcmp(cmd, "PWD") == 0) {
        if(isAuthenticated(cmd_sock)){
           printf("Received PWD command: %s\n", listOfConnectedClients[cmd_sock].currDir);
           send(cmd_sock, listOfConnectedClients[cmd_sock].currDir, sizeof(listOfConnectedClients[cmd_sock].currDir), 0);
          // free(files); // Don't forget to free the allocated memory
        }
        else{
            char corResponse[BUFFER_SIZE] = "530 Not logged in.";
            send(cmd_sock, corResponse, sizeof(corResponse), 0);
        }
    }
    if(strcmp(cmd, "CWD") == 0) {
      printf("Received LIST command: %s\n", arg);
        if(isAuthenticated(cmd_sock)){
            char checkdir[1024];
            checkdir[0] = '\0';
            strcat(checkdir,listOfConnectedClients[cmd_sock].currDir);
            strcat(checkdir,"/");
            strcat(checkdir,arg);
            checkdir[strlen(checkdir)] = '\0';

            printf("%s\n",checkdir);

            if(chdir(checkdir) >= 0){
               strcat(listOfConnectedClients[cmd_sock].currDir,"/");
               strcat(listOfConnectedClients[cmd_sock].currDir,arg);
               send(cmd_sock, listOfConnectedClients[cmd_sock].currDir, sizeof(listOfConnectedClients[cmd_sock].currDir), 0);
               chdir(server_root);
            }
            else{
                char corResponse[BUFFER_SIZE] = "Invalid Directory";
                send(cmd_sock, corResponse, sizeof(corResponse), 0);
            }
        } else {
              char corResponse[BUFFER_SIZE] = "530 Not logged in.";
              send(cmd_sock, corResponse, sizeof(corResponse), 0);
          }
      }
}

void handlefilestore(int data_sock, const char* filename) {

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

void handlefileretrieve(int data_sock, const char* filename) {

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
    close(data_sock);
}

void loadUserfromfile() {
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

void handleUserCommand(int i, char *resDat) {
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
void handlePassCommand(int i, char *resDat , char username[256]) {
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
    return false; // return false 
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

char* trimWhitespace(char *str) {
    char *end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end+1) = '\0';

    return str;
}

void processInput(const char *input_string, char **cmd, char **arg) {
    // Duplicate the input string because strtok modifies the string
    char *input_copy = strdup(input_string);
    if (input_copy == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // Tokenize the string
    *cmd = strtok(input_copy, " ");
    *arg = strtok(NULL, " ");

    // Ensure both cmd and arg are properly null-terminated
    if (*cmd != NULL) {
        *cmd = strdup(*cmd);
        if (*cmd == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            free(input_copy);
            exit(1);
        }
    }

    if (*arg != NULL) {
        *arg = strdup(*arg);
        if (*arg == NULL) {
            fprintf(stderr, "Memory allocation failed\n");
            free(input_copy);
            free(*cmd);
            exit(1);
        }
    }

    // Free the duplicated input string
    free(input_copy);
}
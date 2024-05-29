#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define CMD_PORT 9002
#define DATA_PORT 9003
#define BUFFER_SIZE 1024

void upload_file(int cmd_sock, const char* filename);
void download_file(int cmd_sock, const char* filename);

int main(int argc, char *argv[]) {
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

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
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
        printf("ftp-> ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = '\0';  // Remove newline character

        char *cmd = strtok(command, " ");
        char *filename = strtok(NULL, " ");

        if (cmd && filename) {
            if (strcmp(cmd, "STOR") == 0) {
                upload_file(cmd_sock, filename);
            } else if (strcmp(cmd, "RETR") == 0) {
                download_file(cmd_sock, filename);
            } else {
                printf("Invalid command. Use STOR for upload and RETR for download.\n");
            }
        } else {
            printf("Invalid command format. Use STOR <filename> or RETR <filename>.\n");
        }

        if (read(cmd_sock, server_response, BUFFER_SIZE) > 0) {
            printf("%s\n", server_response);
        }

        if (read(cmd_sock, server_response, BUFFER_SIZE) > 0) {
            printf("%s\n", server_response);
        }
    }

    close(cmd_sock);
    return 0;
}

void upload_file(int cmd_sock, const char* filename) {
    int data_sock;
    struct sockaddr_in server_addr;

    char cmd_buffer[BUFFER_SIZE];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "STOR %s", filename);
    send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);

    // Connect to data socket
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Data socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DATA_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(data_sock);
        return;
    }

    if (connect(data_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Data socket connection failed");
        close(data_sock);
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("File open failed");
        close(data_sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(data_sock, buffer, bytes_read, 0);
    }

    fclose(file);
    close(data_sock);
}

void download_file(int cmd_sock, const char* filename) {
    int data_sock;
    struct sockaddr_in server_addr;

    char cmd_buffer[BUFFER_SIZE];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "RETR %s", filename);
    send(cmd_sock, cmd_buffer, strlen(cmd_buffer), 0);

    // Connect to data socket
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Data socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DATA_PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(data_sock);
        return;
    }

    if (connect(data_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Data socket connection failed");
        close(data_sock);
        return;
    }

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
    close(data_sock);
}

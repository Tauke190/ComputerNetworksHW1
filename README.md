```markdown
# Readme file for the description of our project

This GitHub repo contains the solution for Project 1 for the Computer Networks course taught by Professor Yasir Zaki at NYU Abu Dhabi. Here, we implement a simple file transfer protocol designed to handle multiple concurrent client connections and file transfers. The team members for this project are Avinash Gyawali and Aadim Nepal.

## Running the Repo

In order to run the code in your terminal, please clone this repo. This can be achieved by:

```bash
git clone aadimnepal.com
```

After this, navigate to the server and client directories and inspect them closely. The server directory contains three subdirectories - `Bob`, `Harry`, and `Ron` - and a text file `users.txt`. The directories `Bob`, `Harry`, and `Ron` are for the authenticated users stored in the server. The `users.txt` file contains usernames and passwords that the server is assumed to have authenticated before running the program.

## How to Proceed

Open four terminals, one for the server, and the rest for `Bob`, `Harry`, and `Ron`. In order to run the server code, navigate to the server directory by doing `cd server`, and compile the makefile. After doing this, navigate to another terminal, `cd client`, and run the makefile again for all three clients. You are ready to start authenticating. The procedures for authentication will be displayed by the terminal itself after you run. In short, you must proceed by inputting `USER <username>`, `PASS <password>`, and then can do `RETR <filename>`, `STOR <filename>`, `CWD <directory>`, `!CWD <directory>`, `LIST <directory>`, `!LIST <directory>`, `QUIT`. The code must handle concurrent connections smoothly.

## Limitations and Assumptions

Due to time constraints, we were not able to handle all the edge cases. We assume that once the client changes the server directory, it cannot revert. Similarly, once you change the client directory, you cannot revert. Additionally, if the client attempts to send a filename that already exists, the new file overwrites the existing file. Moreover, in our `users.txt` file, we assume that there is a delimiter at the end. For our case, we have "-------" at the end, just for the ease of loading this content into our code. So if you plan to design `users.txt` on your own, make sure that the username and password for each user is on one line and separated by one space, with a delimiter at the end of the file. For instance:

```
Aadim Nepal
Avinash Gyawali
----------------
```

## Design Choice for Port

We did most of our project on MacOS. The assignment mentions using PORT 21 and 20, but these were not free, so we used 9002. If, while running the code, at some point, you receive a "cannot bind to address" error, just navigate to `#DEFINE CMD_PORT 9002` and change 9002. You can try out different port numbers to see if this works. We also discussed a minor deviation on the implementation of the PORT command. For context, the client sends a PORT command to the server before initiating a new TCP connection for file transfer. We used the PORT that is free and provided by the OS itself, which means we don't implement N+i, but rather get a random free PORT at that time from the OS and use it to pass the PORT command. We discussed this with both the professor and TA, and both were fine with it.
```

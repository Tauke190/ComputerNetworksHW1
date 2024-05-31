# Readme file for the description of our project

This github repo contains the solution for Project 1 for Computer Networks course taught by Professor Yasir Zaki at NYU Abu Dhabi. Here, we implement a simple file transfer protocol designed to handle multiple concurrent client connections and file transfer. The teammembers for this project are Avinash Gyawali and Aadim Nepal. 

## Running the repo

In order to run the code in your terminal, please clone this repo. This can be achieved my:

git clone aadimnepal.com

After this navigate to the server and client directories and insepct them closely. The server directory contains 3 sub directories - Bob, harry and ron and a text file users.txt. The directories, Bob, Harry and Ron are the directories for the authenticated users stored in the server. The users.txt contains usernames and passwords that the server is assumed to have authenticated before running the program.

## How to proceed

Open 4 terminals, one for server, and the rest for Bob, Harry and Ron. In order to run the server code, navigate to the server directory by doing cd Server, and compile the make file. After doing this, navigate to other terminal, cd client and run the makefile again for all 3 clients. You are ready to start authenticating. The procedures for authentication will be displayed by the temrinal itself after you run. In short, you must proceed by inputing USER <username>, PASS <password>, and then can do RETR <filename>, STOR <filename> CWD <filename> !CWD <filename> LIST <filename> !LIST <filename> QUIT. The code must handle concurrent connections smoothly

## Limitations and Assumptions

Due to the time constraints, we were not able to handle all the edge cases. We assume that once the client changes the server directory, it cannot revert. Similarly, once you change the client directory, you cannot revert. Similarly, if the client attempts to send a filename that already exists, the new file overwrites the existing file. Moreover, in our users.txt file, we assume that there is a delimiter at the end. For our case we have "-------" at the end, just for the ease of laoding this content into our code. So if you plan to design users.txt on your own, make sure that the username and password for each user is in one line and separated by one space with delimiter at the end of the file. For instance:

Aadim Nepal
Avinash Gyawali
----------------


## DESIGN CHOICE FOR PORT

We did most of our project in MacOS. The assignment mentions to use PORT 21 and 20 but these were not free so we used 9002. If while running the code, at some point, you receive cannot bind to address error, just navigate to #DEFINE CMD_PORT 9002 and change 9002. You can try out different port numbers to see if this works. We also discussed a minor deviation on the implementation of PORT command. For context, the client sends a PORT command to the server before initaitng a new TCP connection for file transfer. We used the PORT that is free and provided by the OS itself. Which means, we dont implement N+i, but rather get a random free PORT at that time from the OS and use it to pass the PORT command. We discussed this with both the professor and TA and both were fine with it.






/*
 * TEMPLATE 
 */

/*
 ****************************
                            *
 LORENZO SANTOLINI s252916  *
 All Rights reserved        *
                            *
 ****************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <ctype.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rpc/xdr.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../errlib.h"
#include "../sockwrap.h"

#define CHECK 0
#define BUFFRCV 4096
#define SNDBUF 1024

int transfer_started = 0;
char *prog_name, *filename;


void handler(int signum);
void freeMem(char *filename);

int main (int argc, char *argv[])
{
	struct timeval tmv;
	struct sockaddr_in server_addr;
	
	int sockfd, port, i, j, remaining, length, n;
	char request[SNDBUF], received[5], terminator;
	
	uint32_t timestamp, size;
	int f_fd;
    fd_set fdset;
	char file[BUFFRCV];
	FILE *fp;

	if(argc <= 3)
		err_quit("usage: <dest_address> <dest_port> <filename> [filename ... ]\n");

	for(i=0; i<strlen(argv[2]); i++){
		if(!isdigit(argv[2][i]))
			err_quit("Invalid port\n");
	}

	port = atoi(argv[2]);

	if(port < 0 || port > 65535)
		err_quit("Invalid port\n");

	signal(SIGINT, handler);
	
	prog_name = argv[0];
	memset(&server_addr, '0', sizeof(server_addr));
    
	sockfd = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
    
	Inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
    
    /*      */
    errno = 0;
again:
    if(fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0){
        if(INTERRUPTED_BY_SIGNAL){
            goto again;
        }else{
            err_sys("Fcntl failed \n");
        }
    }
    errno = 0;
    connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    tmv.tv_sec = 75;             /* 10 second timeout */
    tmv.tv_usec = 0;
    
    errno = 0;
    if ((n = select(sockfd + 1, NULL, &fdset, NULL, &tmv)) > 0)
    {
        int so_error;
        socklen_t len = sizeof so_error;
        
        if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1){
            err_sys("Getsockopt failed");
        }else{
            if (so_error != 0) {
                err_quit("Connection can't be established");
            }
        }
    }else if ( n == 0){
        err_sys("Timeout expired, no server reachable");
    }else {
        err_sys("Select failed\n");
    }

	if(CHECK) {
		printf("connected\n");
	}
    
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) & ~O_NONBLOCK);

	for(i = 3; i < argc; i++){
        unsigned int filename_size;
        filename_size = 1 + strlen(argv[i]);
        filename_size *= sizeof(char);
        
		filename = malloc(filename_size);
		strncpy(filename, argv[i], strlen(argv[i]));
        filename[strlen(argv[i])] = '\0';
        
		if(CHECK) {
			printf("%s\n", filename);
		}
		memset(request, '\0', sizeof(request));
		sprintf(request, "GET %s\r\n", filename);
    
		tmv.tv_sec = 45;
		tmv.tv_usec = 0;

		if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmv, sizeof(tmv)) < 0)
        	err_sys("Setsockopt client_fd failed\n");
		
		if(CHECK) {
			printf("%s\n", request);
		}

		Write(sockfd, request, (strlen(filename) + 6) * sizeof(char));
		if(CHECK) { 
			printf("requested file n: %d, size string: %d \n", i-2, (int)strlen(request));
		}

		Recv(sockfd, received, 5*sizeof(char), 0);
		if(CHECK) {
			printf("message received %s\n", received);
		}

		if(((strncmp(received, "+OK\r\n", 5) != 0) && (strncmp(received, "+ok\r\n", 5)) != 0)){
			if(strcmp(received, "-ERR") == 0) {
				freeMem(filename);
				err_quit("Error receiving file (-ERR)\n");
			}
			else{
				freeMem(filename);
				err_quit("Error receiving file\n");
			}
		}
		else if(CHECK) {
			printf("message received correctly\n");
		}

		Recv(sockfd, &size, 4*sizeof(char), 0);
		size = ntohl(size);
		if(CHECK) {
			printf("filesize : %d\n", size);
		}

		Recv(sockfd, &timestamp, 4*sizeof(char), 0);
		timestamp = ntohl(timestamp);
		
		if(CHECK) {
			printf("timestamp : %d\n", timestamp);
        }

        if((size < 0) || (timestamp < 0)) {
            freeMem(filename);
			err_quit("Wrong size or timestamp");
        }

        //printf("Metadata received\n");
		if(CHECK) {
			printf("buffer allocated correctly\n");
		}
		
		remaining = size;
		transfer_started = i-2;
		errno = 0;


re_open:
			if((f_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
				if(errno == EINTR){
					goto re_open;
				}
				else {
					freeMem(filename);
					err_sys("Oh dear, something went wrong in file opening\n");			
				}
			}

        fflush(stdout);
		while((remaining > 0) && ((length = recv(sockfd, file, BUFFRCV, 0)) > 0)){
			//fwrite(file, sizeof(char), length, fp);
            if(write(f_fd, file, length) != length){
                free(filename);
                err_sys("Error in writing file\n");
            }
			remaining -= length;
			if(CHECK) {
				printf("remaining: %d\n", remaining);
			}
			//if(remaining == 0) break;
		}
		if(remaining != 0){
			if((remove(filename)) > 0){
				freeMem(filename);
				err_sys("Error receiving and deleting uncomplete file");
			}
			else{
				freeMem(filename);
				err_sys("Error receiving file");
			}
		}
		else if (remaining == 0){
            terminator = '\0';
            fflush(stdout);
            fflush(stdin);
			//write(f_fd, &terminator, 1);
			if(fsync(f_fd) < 0){
				freeMem(filename);
				err_sys("Error writing file to disk");
			}
			fprintf(stdout, "Received file %s\n", filename);
			fprintf(stdout, "Received file size : %d\n", size);
			fprintf(stdout, "Received file timestamp : %d\n", timestamp);
		}
		else if(length < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				freeMem(filename);
				err_sys("Timeout of read expired, shutting down\n");
			}
			else{
				freeMem(filename);
				err_sys("Error in reading answer\n");
			}
		}

		Close(f_fd);
		
		if(CHECK) {
			printf("out of receive\n");
        }
        free(filename);
	}

	memset(request, '\0', sizeof(request));
	sprintf(request, "QUIT\r\n");
	Send(sockfd, request, sizeof(request), 0);
	if(CHECK) {
		printf("quit message sent \n");
	}
	Close(sockfd);
	return 0;
}

void freeMem(char *filename){
    free(filename);
}

void handler(int signum) {
	if(transfer_started != 0) {
		if((remove(filename)) < 0){
			err_sys("Terminating client and error in removing file\n");
		}
		else{
			err_quit("Terminating client and deleting uncomplete file [%d]...\n", transfer_started);
		}
	}
	else{
		err_quit("Terminating client\n");
	}
}



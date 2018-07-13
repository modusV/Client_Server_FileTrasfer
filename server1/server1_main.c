/*
 * TEMPLATE 
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
#define RCVBUFSIZE 512
#define SNDBUFSIZE 4096
#define SERRMSG 6
#define STDSIZE 13

char *prog_name;


void sigintHandler(int signum);
int fd_is_valid(int fd);

int main (int argc, char *argv[]) {
	int port, file_fd, i, n, error, sent, allocated;
	int server_fd, client_fd, val, c_length, s_length, f_length, f_size;
	
	uint32_t size, last_modified, metadata[2];
	
	struct sockaddr_in server_addr, client_addr;
	struct hostent *host;
	struct timeval tmv;
	struct stat st;
	
    char *token, *filename;
    //char *filename_path;
	char err_occurred[SERRMSG], defanswer[5], rcv_buffer[RCVBUFSIZE], file_buffer[SNDBUFSIZE];
	

	
	prog_name = argv[0];
	if(argc <= 1) {
		err_quit("usage: <dest_port>\n");
	}
	/*if(!isdigit(argv[1]))
		err_quit("port not a digit\n");
	*/
	for(i=0; i<strlen(argv[1]); i++){
		if(!isdigit(argv[1][i]))
			err_quit("Invalid port\n");
	}

	port = atoi(argv[1]);
	if(CHECK) printf("port saved\n");

	if(port < 0 || port > 65535)
		err_quit("Invalid port\n");
	
	server_fd = Socket(AF_INET, SOCK_STREAM, 0);
	if(CHECK) printf("socket created\n");
	val = 1;


	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&val , sizeof(int)))
		err_sys("Setsockopt server_fd failed\n");
	bzero((char *) &server_addr, sizeof(server_addr));

	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, sigintHandler);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons((unsigned short)port);

	Bind(server_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
	/* chech later if 0 is fine */
	if(CHECK) printf("waiting on listen, bind successfull\n");
	Listen(server_fd, 1);
	c_length = sizeof(client_addr);
	strncpy(err_occurred, "-ERR\r\n", 6);


	tmv.tv_sec = 70;
	tmv.tv_usec = 0;

	while(1){
		
		client_fd = Accept(server_fd, (struct sockaddr *) &client_addr, &c_length);
		if(CHECK) printf("successfully connected with client\n");
		fflush(stdout);

		while(1) {
            allocated = 0;
		//set receiving socket blocking with timeout
			if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmv, sizeof(tmv)) < 0)
        		err_sys("Setsockopt client_fd failed\n");
			
			bzero(rcv_buffer, RCVBUFSIZE);
			errno = 0;
re_read:
			if(CHECK) printf("waiting on read\n");
			if((n = read(client_fd, rcv_buffer, RCVBUFSIZE)) < 0){
				if (INTERRUPTED_BY_SIGNAL) {
					if(CHECK) printf("interrupted by signal\n");
					goto re_read;
				}
			}
			else if (n == 0){
				err_msg("Client closed connection\n");
				break;
			}
			else if( errno == EAGAIN || errno == EWOULDBLOCK ){
				err_msg("Timeout expired, closing connection\n");
				break;
			}
			if(CHECK) printf("errno: %d\n", errno );
			
			if(CHECK) printf("received string: %s\n",rcv_buffer);
			if(CHECK) printf("length string: %d\n", (int) strlen(rcv_buffer));
			
            if(strlen(rcv_buffer) != 512){
                rcv_buffer[strlen(rcv_buffer)] = '\0';
            }
                s_length = strlen(rcv_buffer);

			if(!strcmp(rcv_buffer, "quit\r\n") || !strcmp(rcv_buffer, "QUIT\r\n")){
				printf("client requested closing connection\n");
				break;
			}
			else if(!(strncmp(rcv_buffer, "GET ", 4) || strncmp(rcv_buffer, "get ", 4))
				|| (rcv_buffer[s_length-1] != '\n') || (rcv_buffer[s_length-2] != '\r')) {

				if((write(client_fd, err_occurred, SERRMSG)) != SERRMSG){
					err_msg("Uncorrect command received\n");
					err_msg("Error sending close message back, closing anyway\n");
					break;
				}
				else{
					err_msg("Uncorrect command received\n");
					break;
				}
			}

			token = strtok(rcv_buffer, " ");
		 	token = strtok(NULL, " ");

			f_length = strlen(token);

			if(f_length == 0 || token == NULL || f_length >= 512) {
				if((write(client_fd, err_occurred, SERRMSG)) != SERRMSG){
						err_msg("Error sending close message back, closing anyway\n");
					}
					err_msg("Unvalid commad received");
					break;
			}

			filename = malloc(f_length * sizeof(*filename));
            allocated = 1;
            memset(filename, '\0', sizeof(filename));

			i=0;

			do {
				if(token[i] == '\0' || token[i] == '\n') {
					if((write(client_fd, err_occurred, SERRMSG)) != SERRMSG){
						err_msg("Error sending close message back, closing anyway\n");
					}
					err_msg("Unvalid commad received\n");
					break;
				}

				if(CHECK) printf("%c", token[i]);
				filename[i] = token[i];
				i++;

			} while(token[i] != '\r');

			if(token[i+1] != '\n') {
				if((write(client_fd, err_occurred, SERRMSG)) != SERRMSG){
					err_msg("Error sending close message back, closing anyway\n");
				}
				err_msg("Unvalid commad received");
				break;
			}

            filename[i] = '\0';
            if(CHECK) printf("\n");
            
            if(filename != NULL && filename+1 != NULL) {
                if(filename[0] == '/' || ((filename[0] == '.') && (filename[1] == '.'))){
                    err_msg("client tried to request files in unauthorized directory");
                    break;
                }
            }
            else{
                err_msg("Invalid filename");
            }
            
			if(CHECK) printf("\n");
			if(CHECK) printf("filename: %s, %d\n", filename, (int)strlen(filename));

			if(stat(filename, &st)) {
				if((write(client_fd, err_occurred, SERRMSG)) != SERRMSG){
					err_msg("Error sending close message back, closing anyway\n");
					break;
				}
				err_msg("404 - File not found\n");
				break;
			}

			if(CHECK) printf("stat successfull\n");
			f_size = st.st_size;
			size = htonl(st.st_size);
			last_modified = htonl(st.st_mtime);

			if(CHECK) printf("size= %u, sizehton= %u last modified= %u\n", ntohl(size), size, ntohl(last_modified));


			errno = 0;
re_open:
			if((file_fd = open(filename, O_RDONLY)) < 0) {
				if(errno == EINTR){
					goto re_open;
				}
				else {
					err_msg("Oh dear, something went wrong in file opening\n");
					if((write(client_fd, err_occurred, SERRMSG)) != SERRMSG){
						err_msg("Error sending close message back, closing anyway\n");
					}
					break;				
				}
			}

			strncpy(defanswer, "+OK\r\n", 5);

			metadata[0] = size;
			metadata[1] = last_modified;

			if((send(client_fd, defanswer, sizeof(defanswer), MSG_NOSIGNAL)) != sizeof(defanswer)) {
				err_msg("Error in sending confirm answer\n");
				break;
			}

			if((send(client_fd, metadata, 2*sizeof(uint32_t), MSG_NOSIGNAL)) != (2*sizeof(uint32_t))){
				err_msg("Error in sending metadata\n");
				break;
			}

			errno = 0;
			error = 0;
re_read_f:
			while((i = read(file_fd, file_buffer, SNDBUFSIZE)) > 0) {
				errno = 0;
				sent = 0;
				if((sent = sendn(client_fd, file_buffer, i, MSG_NOSIGNAL)) != i) {
					err_msg("Error sending file\n");
					error = 1;
					break;
				}
				if(errno != 0) {
					err_msg("Some error occurred, closing connection\n");
					error = 1;
					break;
				}
			}
			if(i < 0) {
				if(INTERRUPTED_BY_SIGNAL) {
					goto re_read_f;
				} else{
					err_msg("Error in read file\n");
					break;
				}
			}
			else if(error){
				printf("Some error occurrend in sending file\n");
				break;
			}
			else{
				if(!error){
					printf("File successfully sent\n");
                    free(filename);
				}
			}
		}
        
        if(fd_is_valid(file_fd)) {
            Close(file_fd);
        }
        
		if(allocated) {
			free(filename);
        }
	}
	Close(client_fd);
	return 0;
}

void sigintHandler(int signum){
    if(signum == SIGINT){
        printf("Termination request received, shutting down\n");
        exit(0);
    }
}

int fd_is_valid(int fd) {
    int a;
    errno=0;
    a = fcntl(fd, F_GETFD);
    if(a == -1){
        if(errno == EBADF){
            return 0;
        }
    }
    else if (a > 0){
        return 1;
    }
}




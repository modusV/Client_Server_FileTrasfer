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
#include <sys/wait.h>

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
#define MAX_CLIENTS 3

char *prog_name;
//sem_t semaphore;
int current_clients;


void sigIntHandler(int signum);
void sigChldHandler(int signum);
void sigIntHandlerChild(int signum);
int fd_is_valid(int fd);


int main (int argc, char *argv[]) {
	int port, file_fd, i, n, error, sent, allocated;
	int server_fd, client_fd, val, c_length, s_length, f_length, f_size;
	

	pid_t active_child[MAX_CLIENTS], pid;
	
	uint32_t size, last_modified, metadata[2];
	
	struct sockaddr_in server_addr, client_addr;
	struct hostent *host;
	struct timeval tmv;
	struct stat st;
	
	char *token, *filename;
	char err_occurred[SERRMSG], defanswer[5], rcv_buffer[RCVBUFSIZE], file_buffer[SNDBUFSIZE];
	

	
	prog_name = argv[0];
	if(argc < 2) {
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
	signal(SIGINT, sigIntHandler);
	signal(SIGCHLD, sigChldHandler);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons((unsigned short)port);

	Bind(server_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
	/* chech later if 0 is fine */
	if(CHECK) printf("waiting on listen, bind successfull\n");
	Listen(server_fd, 1);
	c_length = sizeof(client_addr);
	strncpy(err_occurred, "-ERR\r\n", 6);

	/*
	errno = 0;
	if((current_clients = mmap(NULL, sizeof (*current_clients), 
		PROT_WRITE | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0) < 0){
		err_sys("mmap failed\n");
	}
	*/

	tmv.tv_sec = 70;
	tmv.tv_usec = 0;
	current_clients = 0;

	/*
    if((sem_init(&semaphore, 1, 1)) < 0){
    	err_sys("Error initializing semaphore");
    }
    */

	while(1){
		
		while(current_clients >= MAX_CLIENTS){
			usleep(1000*40);
		}
		client_fd = Accept(server_fd, (struct sockaddr *) &client_addr, &c_length);
		if(CHECK) printf("successfully connected with client\n");
		fflush(stdout);

		if( (pid = fork()) != 0){
			/*
			sem_wait(&semaphore);
			*current_clients = *current_clients + 1;
			sem_post(&semaphore);
			*/
			current_clients++;
			//father 
			Close(client_fd);
		}
		else{
			signal(SIGINT, sigIntHandlerChild);
			while(1) {
			//set receiving socket blocking with timeout
				if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tmv, sizeof(tmv)) < 0)
	        		err_sys("Setsockopt client_fd failed\n");
				
				bzero(rcv_buffer, RCVBUFSIZE);
				errno = 0;
                allocated = 0;
re_read:
				if((n = read(client_fd, rcv_buffer, RCVBUFSIZE)) < 0){
					if (INTERRUPTED_BY_SIGNAL) {
						if(CHECK) printf("Interrupted by signal\n");
						goto re_read;
					}
				}
				else if (n == 0){
					err_msg("Client closed connection");
					break;
				}
				else if( errno == EAGAIN || errno == EWOULDBLOCK ){
					err_msg("Timeout expired, closing connection\n");
					break;
				}
				
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
				bzero(filename, sizeof(filename));
                
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
            	else {
               		err_msg("Invalid filename");
           		}

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
					err_msg("Some error occurred in sending file \n");
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
            
            if(allocated){
                free(filename);
            }
			return 0;
		}
	}
	while(pid = wait(NULL) > 0);
	return 0;
}

void sigIntHandlerChild(int signum){
	if(signum == SIGINT){
		exit(1);
	}
}

void sigChldHandler(int signum){
	pid_t pid;
	if(signum == SIGCHLD) {
		pid = wait(NULL);
		current_clients--;
		if(CHECK) printf("child %d correctly terminated\n", pid);
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

void sigIntHandler(int signum){

	if(signum == SIGINT){
		while((wait(NULL)) > 0);
		printf("All child processes are dead, shutting down father...\n");
		exit(0);
	}
}





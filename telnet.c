/*===========================;
 *
 * File: telnet.c
 * Content: A telnet program to
 * connect remotely to machines
 * over ip/tcp using the telnet protocol.
 * Uses 1 input arguement of the form host:port where
 * host is either a domain name or an ipv4 address in dot decimal notation and
 * port specifies in decimal the remote machine port number of the connection.
 * Date: 17/6/2022
 *
 *****************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

// define command codes
#define IAC 255
#define DONT 254
#define DO 253
#define WONT 252
#define WILL 251
#define GO_AHEAD 249

int client_socket_fd = -1;

// Host string is a null terminated string (domain name or ipv4 address in 
// dot decimal) that may contain a port number or may not. This function returns
// the port number as an integer if one is present, and -1 otherwise. 
int port_specified(char *host_string)
{
	int str_len = strlen(host_string);
	for (int i=0; i<str_len; ++i) {
		if (host_string[i] == ':') {
			char *endptr = NULL;
			int port = strtol(&(host_string[i+1]), &endptr, 10);
			if (endptr == &(host_string[i+1])) {
				// no port number present after colon
				return -1;
			}
			return port;
		}
	}
	return -1;
}

// Returns program_input_string without the appended port number
char *get_remote_host(char *program_input_string)
{
	char remote_host[1000] = {0};
	for (int i=0; i<strlen(program_input_string); ++i) {
		if ((program_input_string[i]) != ':') {
			remote_host[i] = program_input_string[i];
		}
		else {
			break;
		}
	}
	char *host = malloc(strlen(remote_host)+1);
	memcpy(host, remote_host, strlen(remote_host));
	host[strlen(remote_host)] = 0;
	return host;
}

void iohandler(int sigio)
{
	char *error_message = "an error occurred in iohandler.\n";
	unsigned char recv_buffer[100000];
	int no_of_bytes = recv(client_socket_fd, recv_buffer, 100000, MSG_PEEK);
	if (no_of_bytes == -1) {
		write(1, error_message, strlen(error_message));
		exit(EXIT_FAILURE);
	}
	if (no_of_bytes > 0) {
		no_of_bytes = recv(client_socket_fd, recv_buffer, 100000, 0);
		if (no_of_bytes <= 0) {
			char *error_message2 = "Error receiving bytes in iohandler.\n";
			write(1, error_message2, strlen(error_message2));
			exit(EXIT_FAILURE);
		}
		char options_response_required = 0;
		int options_buffer_len = 10000;
		unsigned char options_buffer[options_buffer_len];
                int NoOfOptions = 0;
                for (int i=0; i<no_of_bytes-2; ++i) {
                        if ((recv_buffer[i] == IAC) &&
                            (recv_buffer[i+1] == DO)) {
                                int curr_option_index = NoOfOptions*3;
                                options_buffer[curr_option_index] = IAC;
                                options_buffer[curr_option_index+1] = WONT;
                                options_buffer[curr_option_index+2] = recv_buffer[i+2];
                                ++NoOfOptions;
                                options_response_required = 1;
                        }
                        else if ( (recv_buffer[i] == IAC) &&
                                  (recv_buffer[i+1] == WILL) ) {
                                int curr_option_index = NoOfOptions*3;
                                options_buffer[curr_option_index] = IAC;
                                options_buffer[curr_option_index+1] = DONT;
                                options_buffer[curr_option_index+2] = recv_buffer[i+2];
                                ++NoOfOptions;
                                if (!options_response_required) {
                                        options_response_required = 1;
                                }
                        }
                }
		if (options_response_required) {
			int NoOfBytesSent = NoOfOptions*3;
			if (send(client_socket_fd, options_buffer, NoOfBytesSent, 0)!=NoOfBytesSent) {
				char *error_message3 = "Error sending option response to server.\n";
				write(1, error_message3, strlen(error_message3));
				exit(EXIT_FAILURE);
			}
		}
		if (options_response_required) {
                        for (int i=0; i<no_of_bytes-1; ++i) {
                                if ( (recv_buffer[i] == IAC) &&
                                     ( (recv_buffer[i+1]==DO) ||
                                       (recv_buffer[i+1]==WILL) ) ) {
                                        for (int j=i; j<no_of_bytes-3; ++j) {
                                                recv_buffer[j] = recv_buffer[j+3];
                                        }
                                        no_of_bytes -= 3;
                                        --i;
                                }
                        }
                }
		if (no_of_bytes > 0) {
                        // search for IAC GA at end of recv_buffer
                        // and output to terminal
                        unsigned char go_ahead_present = 0;
                        int go_ahead_index = -1;
                        for (int i=0; i<no_of_bytes-1; ++i) {
                                if ( (recv_buffer[i] == IAC) &&
                                     (recv_buffer[i+1]== GO_AHEAD)) {
                                        go_ahead_index = i;
                                        go_ahead_present = 1;
                                        break;
                                }
                        }
                        if (!go_ahead_present) {
                        	recv_buffer[no_of_bytes] = 0;
                                if (write(1,
				          recv_buffer,
                        	          no_of_bytes) == -1) {
					char *error_message4 = "Error writing received bytes to terminal";
					write(1, error_message4, strlen(error_message4));	
					exit(EXIT_FAILURE);
			        }
                                if (fflush(stdout)== EOF) {
                                        char *error_message5 = "Error flushing stdout to terminal.";
                                        write(1, error_message5, 
					      strlen(error_message5));
					exit(EXIT_FAILURE);
                                }
			}
			else {
                                if (write(1,
					  recv_buffer,
                                          go_ahead_index) != go_ahead_index) {
                                        char *error_message6 = "Error printing received data before go ahead.\n";
					write(1, error_message6, strlen(error_message6));
					exit(EXIT_FAILURE);
                                }
                                if ( write(1, 
					   &(recv_buffer[go_ahead_index+2]),
                                            no_of_bytes - (go_ahead_index+2)) != (no_of_bytes - (go_ahead_index+2))) {
                                        char *error_message7 = "Error printing received data after go ahead.\n";
					write(1,error_message7, strlen(error_message7));
							exit(EXIT_FAILURE);

                                }
                                if (fflush(stdout)== EOF) {
                                        char *error_msg = "Error flushing stdout to terminal.";
					write(1, error_msg, strlen(error_msg));
                                        exit(EXIT_FAILURE);
                                }
                                
                        }
		}
	}
	else if (no_of_bytes == 0) {
		char *socket_ready_to_send = "socket available for write.\n";
		write(1, socket_ready_to_send, strlen(socket_ready_to_send));
	}
}

int main(int argc, char *argv[])
{
	char *remote_host = NULL;
	int port_val = -1;
	if (argc>1) {
		remote_host = get_remote_host(argv[1]);
	        port_val = port_specified(argv[1]);
	}
	char port[10] = {0};
	sprintf(port, "%d", port_val);

	struct addrinfo hints;
	hints.ai_flags = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_addrlen = 0;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;
	hints.ai_next = NULL;

	struct addrinfo *res = NULL;
	int get_remote_addr = getaddrinfo(remote_host, port, &hints, &res);
	if (get_remote_addr != 0) {
		printf("Error retrieving remote host address info. %s.", 
			gai_strerror(get_remote_addr));
		exit(EXIT_FAILURE);
	}
	
	client_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	fcntl(client_socket_fd, F_SETOWN, getpid()); // set owner process to
                                                     // receive SIGIO and SIGURG
                                                     // signals to his process

        fcntl(client_socket_fd, F_SETFL, O_ASYNC);
	struct sigaction sigact = {0};
        sigact.sa_handler = iohandler;
        if (sigemptyset(&(sigact.sa_mask))==-1) {
                printf("Error setting sigset_t to empty. %s.\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
        }
        sigact.sa_flags = SA_RESTART;
	sigaction(29, &sigact, NULL); // SIGIO is 29

	sigset_t set;
	if (sigemptyset(&set) == -1) {
		printf("Error emptying the set of signals. %s.\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (sigaddset(&set, 29)==-1) {
		printf("Error adding SIGIO to set of isgnals. %s.\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	int conn_res = -1;
	while (conn_res == -1) {
		if (sigprocmask(SIG_BLOCK, &set, NULL)==-1) {
			printf("Error blocking SIGIO for this process. %s.\n",
				strerror(errno));
			exit(EXIT_FAILURE);
		}
		conn_res = connect(client_socket_fd, res->ai_addr, res->ai_addrlen);
		if (sigprocmask(SIG_UNBLOCK, &set, NULL)==-1) {
			printf("Error unblocking SIGIO for this process. %s\n",
				strerror(errno));
			exit(EXIT_FAILURE);	
		}
		if (conn_res == -1) {
			if (res->ai_next == NULL) {
				printf("Exhausted remote machine address list and"
				       " couldn't connect to any of them.\n"
				       "%s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			res = res->ai_next;
		}
	}

	int send_buffer_length = 100000;
	unsigned char send_buffer[send_buffer_length];
	char curr_char = fgetc(stdin);
        int send_pos = 0;
	while (1) {
                while ((curr_char != '\n') && (curr_char != EOF)) {
               	        send_buffer[send_pos++] = curr_char;
               		curr_char = fgetc(stdin);
                }
                if (curr_char == '\n') {
                	send_buffer[send_pos++] = '\r';
                	send_buffer[send_pos++] = '\n';
                	send_buffer[send_pos] = 0;
                }
                if (send(client_socket_fd, send_buffer, send_pos, 0)==-1) {

                	printf("Error sending server client input. %s.\n",
                        strerror(errno));
                }
	        send_pos = 0;	       
		curr_char = fgetc(stdin);
	}

}

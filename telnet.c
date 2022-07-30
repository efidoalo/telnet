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
 * gcc -c telnet.c
 * gcc telnet.o -o telnet -pthread ~/Documents/Containers/C/vector.o
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
#include <termios.h>
#include <pthread.h>
#include "/home/andy/Documents/Containers/C/vector.h"

// define telnet protocol command codes
#define SE 240  // end of subnegotiation of parameters for option
#define NOP 241 // No Operation
#define DM 242   // Data Mark command (used in Urgent tcp segment)
#define BRK 243 // NVT Break character
#define IP 244  // Interrupt process command
#define AO 245 // Abort Output command
#define AYT 246  // Are you there command
#define EC 247  // Erase Character command
#define EL 248 // Erase Line character
#define SB 250 // Subnegotiation of option started

#define IAC 255
#define DONT 254
#define DO 253
#define WONT 252
#define WILL 251
#define GA 249

// Application specific defines
#define RECV_BUFFER_LEN 100000


struct thread_shared_data
{
        int client_socket_fd;
        struct vector *input_data;
        pthread_mutex_t data_synch_mutex;
};

struct thread_shared_data tsd;
unsigned char password_prompt_from_server = 0;

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


struct received_data
{
	unsigned char type; // 0 for normal, 1 for urgent
	unsigned char *recv_buffer;
	int recv_buffer_len;	
};


void *normal_data_handler(void *data)
{
	struct thread_shared_data *tsd  = (struct thread_shared_data *)data;
	int recv_buffer_len = 100000;
	unsigned char recv_buffer[recv_buffer_len];
	while (1) {
		int no_of_bytes = recv(tsd->client_socket_fd, 
				       recv_buffer, 
				       recv_buffer_len, 
				       0);
		// acquire mutex lock
		pthread_mutex_lock(&(tsd->data_synch_mutex));
		if (no_of_bytes == -1) {
			printf("Error receiving normal data. %s.\n", strerror(errno));
			exit(EXIT_FAILURE);
		}		
		else if (no_of_bytes == 0) {
			pthread_mutex_unlock(&(tsd->data_synch_mutex));
			continue;
		}
		else if (no_of_bytes > 0) {
			struct received_data *rd = (struct received_data *)malloc(sizeof(struct received_data));
			if (rd == NULL) {
				printf("Error allocating memory on heap for received data structure,\n");
				exit(EXIT_FAILURE);
			}
			rd->type = 0;
			rd->recv_buffer = malloc(no_of_bytes);
			if ((rd->recv_buffer)==NULL) {
				printf("Error alloacting memory on heap for received normal data.\n");
				exit(EXIT_FAILURE);
			}
			memcpy(rd->recv_buffer, recv_buffer, no_of_bytes);
			rd->recv_buffer_len = no_of_bytes;
			vector_push_back(tsd->input_data, rd);	
		}
		pthread_mutex_unlock(&(tsd->data_synch_mutex));
	}
}

void urghandler(int sigurg)
{
	int recv_buffer_len = 100000;
        unsigned char recv_buffer[recv_buffer_len];
        unsigned char carry_DOWILL = 0;
        unsigned char carry_OPT = 0;
        
        int no_of_bytes = recv(tsd.client_socket_fd,
                               recv_buffer,
                               recv_buffer_len,
                               MSG_OOB);
	// acquire mutex lock
	pthread_mutex_lock(&(tsd.data_synch_mutex));
	if (no_of_bytes == -1) {
		pthread_mutex_unlock(&(tsd.data_synch_mutex));
		return;
	}
	else if (no_of_bytes == 0) {
		pthread_mutex_unlock(&(tsd.data_synch_mutex));
		return;
	}
	else if (no_of_bytes > 0) {
		struct received_data *rd = (struct received_data *)malloc(sizeof(struct received_data));
		if (rd == NULL) {
			printf("Error allocating memory on heap for received data structure,\n");
			exit(EXIT_FAILURE);
		}
		rd->type = 1;
		rd->recv_buffer = malloc(no_of_bytes);
		if ((rd->recv_buffer)==NULL) {
			printf("Error alloacting memory on heap for received normal data.\n");
			exit(EXIT_FAILURE);
		}
		memcpy(rd->recv_buffer, recv_buffer, no_of_bytes);
		rd->recv_buffer_len = no_of_bytes;
		vector_push_back(tsd.input_data, rd);
	}
	pthread_mutex_unlock(&(tsd.data_synch_mutex));
        
}

void *urgent_data_handler(void *data)
{		
	sigset_t signalset;
	if (sigemptyset(&signalset)==-1) {
                printf("Error setting signalset to empty.%s.\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
        }
        if (sigaddset(&signalset, 23)==-1) { // add SIGURG to signalset
                printf("Error adding SIGURG to signalset.%s.\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
        }
        if (pthread_sigmask(SIG_UNBLOCK, &signalset, NULL)==-1) {
                printf("Error blocking SIGURG process wide. %s.\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
        }	

	while (1) {

	}
}



// buffer[EC_index] == EC and the previous byte is IAC, indicating an erase character
// this function returns the starting index in buffer of the previous print position
// before IAC EC, otherwise this function returns -1 (if no previous print position exists in buffer, for 
// example if IAC EC is the first two bytes of buffer so that EC_index == 1). buffer from EC_index and backwards
// includes no options (except of course the IAC EC where EC is at EC_INDEX).
//  8 is backspace
int get_previous_print_position(unsigned char *buffer, int EC_index)
{
	int bs_count = 0;
	int char_count = 0;
	unsigned char not_chars_list[7] = {0, 10, 13, 7, 9, 11, 12};
	int buff_index = EC_index - 2;
	while (buff_index >= 0) {
		while (buff_index >= 0) {
			unsigned char valid_char = 1;
			for (int i=0; i<7; ++i) {
				if (buffer[buff_index] == not_chars_list[i]) {
					valid_char = 0;
				}
			}
			if (!valid_char) {
				--buff_index;
			}
			else {
				break;
			}
		}
		if (buff_index < 0) {
			break;
		}
		// buffer[buff_index] is a valid_char
		if (buffer[buff_index] == 8) {
			++bs_count;
		}
		else {
			++char_count;
		}
		if (char_count > bs_count) {
			int temp_index = buff_index;
			while (buff_index >= 0) {
                        	unsigned char valid_char = 1;
                        	for (int i=0; i<7; ++i) {
                        	        if (buffer[buff_index] == not_chars_list[i]) {
                        	                valid_char = 0;
                        	        }
                        	}
                        	if (!valid_char) {
                        	        --buff_index;
                        	}
				else {
					break;
				}
       		        }
			if (buff_index <0) {
				break;
			}
			if (buffer[buff_index]!=8) {
				return temp_index;	
			}
			else {
				buff_index = temp_index;		
			}
		}
		--buff_index;
	}	
	return buff_index; // returns -1
}
// returns the number of backspaces to output to get to the beginning of recv_buffer. recv_buffer[EC_index] == EC
// and recv_buffer[EC_index-1] == IAC. there are no other commands in recv_buffer before index EC_index-1
int get_no_of_bs_from_curr_output(unsigned char *recv_buffer, int EC_index)
{
	unsigned char cr_present = 0;
        int last_cr_index = -1;
        for (int i=EC_index-2; i>=0; --i) {
                if (recv_buffer[i]==13) {
                        last_cr_index = i;
                        cr_present = 0;
                        break;
                }
        }
        if (cr_present) {
                return (EC_index-2) - last_cr_index + 1;
        }
	unsigned char impassible_char_list[4] = {9,10,11,12};
        unsigned char impassible_char_present = 0;
        int last_impassible_char_index = -1;
        for (int i=EC_index - 2; i>=0; --i) {
                for (int j=0; j<4; ++j) {
                        if (recv_buffer[i] == impassible_char_list[j]) {
                                impassible_char_present = 1;
                                last_impassible_char_index = i;
                                break;
                        }
                }
        }
        if (impassible_char_present) {
                return (EC_index-2) - last_impassible_char_index + 1;
        }
	int valid_char_count = 0;
	int bs_count = 0;
	for (int i=0; i<=EC_index-2; ++i) {
		if (recv_buffer[i] == 8) {
			++bs_count;
		}
		else if ( (recv_buffer[i]!=0) && (recv_buffer[i]!=7) ) {
			++valid_char_count;
		}
	}
	return (valid_char_count - bs_count);
}

// output is a buffer containing byte terminal output, whereby the only command is IAC EC where EC is at 
// byte index EC_index. output[0] == '\n' is guaranteed. This functionr returns the number of backspaces required to print to the terminal
// that will set the print position back to the character output[char_index]
int get_no_of_bs_to_output(unsigned char *output, int EC_index, int char_index)
{
	unsigned char cr_present = 0;
	int last_cr_index = -1;
	for (int i=EC_index-2; i>=char_index; --i) {
		if (output[i]==13) {
			last_cr_index = i;
			cr_present = 0;
			break;
		}
	}	
	if (cr_present) {
		return (EC_index-2) - char_index + 1;
	}
	unsigned char impassible_char_list[4] = {9,10,11,12};
	unsigned char impassible_char_present = 0;
	int last_impassible_char_index = -1;
	for (int i=EC_index - 2; i>=char_index; --i) {
		for (int j=0; j<4; ++j) {
			if (output[i] == impassible_char_list[j]) {
				impassible_char_present = 1;
				last_impassible_char_index = i;
				break;
			}
		}		
	}
	if (impassible_char_present) {
		return (EC_index-2) - last_impassible_char_index + 1;
	}
	if (char_index == 1) {
		return (EC_index-2);
	}
	// char_index refers to the starting index of the previous print position before IAC EC
	//
	return 1;			
}

// EL is the index of EL (recv_buffer[EL_index] == EL and recv_buffer[EL_index-1] == IAC). recv buffer 
// contains no telnet commands before IAC EL at EL_index. This function retuns the index in recv_buffer
// of the first character after the last newline. If no newline is present in recv_buffer, -1 is returned
int get_start_of_current_line_index(unsigned char *recv_buffer, int EL_index)
{
	int index = EL_index-2;
	for (index; index>=0; --index) {
		if (recv_buffer[index] == '\n') {
			return index+1;
		}
	}
	return -1;
}

// Thread that gets user input and sends it to the server
//
void *handle_user_input(void *threadshareddata)
{
	struct thread_shared_data *tsd = (struct thread_shared_data *)threadshareddata;
	int client_socket_fd = tsd->client_socket_fd;
	int send_buffer_len = 100000;
	unsigned char send_buffer[send_buffer_len]; // user input buffer that is used to send user input to server
        char curr_char = fgetc(stdin);
        int send_pos = 0;
	// send command strings
        char *send_synch = "send synch";
        char *send_brk = "send brk";
        char *send_ip = "send ip";
        char *send_ao = "send ao";
        char *send_ayt = "send ayt";
        char *send_ec = "send ec";
        char *send_el = "send el";
        char *list_cmds = "send ?";
	while (1) {	
                while ((curr_char != '\n') && (curr_char != EOF)) {
			if (curr_char == 29) {
				//consume the rest of the line/newline character
				curr_char = fgetc(stdin);
				while (curr_char != '\n') {
					curr_char = fgetc(stdin);
				}
				// ^] hit, present the user with a command menu
				while (1) {
					printf("\ntelnet> type send <command> followed by the enter key to send a command."
				       	       "\ntelnet> type send ? followed by the enter key for a list of commands that you can send."
					       "\ntelnet> ");
					if (fflush(stdout)==EOF) {
						printf("Error flushing output to terminal.\n");
						exit(EXIT_FAILURE);
					}	
					char user_input[1000];
					char send_cmd_ch = fgetc(stdin);
					int pos = 0;
					while ((send_cmd_ch != '\n') && (send_cmd_ch != EOF) && (pos<100)) {
						user_input[pos++] = send_cmd_ch;
						send_cmd_ch = fgetc(stdin);
					}
					if (send_cmd_ch == EOF) {
						printf("Error reading telnet input command from terminal.\n");
						continue;
					}
					if (pos >= 100) {
						printf("Error sending command to server. Input too long.\n");
						continue;
					}
					if (strncmp(user_input, send_synch, strlen(send_synch))== 0 ) {
						// send a SYNCH command to server in data stream
						// first send current buffered data without urg tcp flag set
						if (send_pos>0) {
							if (send(client_socket_fd, send_buffer, send_pos, 0) != send_pos) { 
								printf("Error sending message to server before sending synch command.");
								exit(EXIT_FAILURE);
							}
						}
						unsigned char iac[1] = {IAC};
						if (send(client_socket_fd, iac, 1, 0)!= 1) {
							printf("Error sending iac 0xff in normal stream before sending data mark in urgent stream.\n");
							exit(EXIT_FAILURE);
						}
						unsigned char data_mark[1] = {DM};
						if (send(client_socket_fd, data_mark, 1, MSG_OOB) != 1) {  // URG tcp flag set 
							printf("Error sending synch command.\n");
							exit(EXIT_FAILURE);
						}
						send_pos = 0;
						curr_char = fgetc(stdin);
						break;
					}	
					else if (strncmp(user_input, send_brk, strlen(send_brk))==0) {
				        	send_buffer[send_pos++] = IAC;
						send_buffer[send_pos++] = BRK;
						curr_char = fgetc(stdin);
						break;		
					}
					else if (strncmp(user_input, send_ip, strlen(send_ip))==0) {
						send_buffer[send_pos++] = IAC;
						send_buffer[send_pos++] = IP;
						curr_char = fgetc(stdin);
						break;
					}
					else if (strncmp(user_input, send_ao, strlen(send_ao))==0) {
						send_buffer[send_pos++] = IAC;
						send_buffer[send_pos++] = AO;
						curr_char = fgetc(stdin);
						break;
					}
					else if (strncmp(user_input, send_ayt, strlen(send_ayt))==0) {
						send_buffer[send_pos++] = IAC;
						send_buffer[send_pos++] = AYT;
						curr_char = fgetc(stdin);
						break;
					}
					else if (strncmp(user_input, send_ec, strlen(send_ec))==0) {
						send_buffer[send_pos++] = IAC;
                                                send_buffer[send_pos++] = EC;
                                                curr_char = fgetc(stdin);
                                                break;
					}
					else if (strncmp(user_input, send_el, strlen(send_el))==0) {
                                                send_buffer[send_pos++] = IAC;
                                                send_buffer[send_pos++] = EL;
                                                curr_char = fgetc(stdin);
                                                break;
                                        }
					else if (strncmp(user_input, list_cmds, strlen(list_cmds))==0) {
						printf("ao       send abort output command\n"
						       "ayt      send are you there command\n"
						       "brk      send break command\n"
						       "ec       send erase character command\n"
						       "el       send erase line character\n"
						       "ip       send interrupt process command\n"
						       "synch    send synch command\n"
						       "?        obtain a list of commands that can be sent\n");
						continue;        	
					}
					else {
						printf("Unrecognized input. Try again.\n");
						continue;
					}
				}
				continue;          
			}
               	        send_buffer[send_pos++] = curr_char;
               		curr_char = fgetc(stdin);
                }
                if (curr_char == '\n') {
                	send_buffer[send_pos++] = '\r';
                	send_buffer[send_pos++] = '\n';
                	send_buffer[send_pos] = 0;
			unsigned char bs = 8;
			if (write(0, &bs, 1) != 1) { // backspace to delete locally  echoed newline character
				printf("Error moving backwards one byte in the terminal output stream. %s.\n",
					strerror(errno));
				exit(EXIT_FAILURE);
			}
                }
                if (send(client_socket_fd, send_buffer, send_pos, 0) != send_pos) {

                	printf("Error sending server client input. %s.\n",
                        strerror(errno));
                }
		else {
			if (password_prompt_from_server == 1) {
				// turn local echo back on
				struct termios curr_termattr;
                                if (tcgetattr(0, &curr_termattr)==-1) {
                                        char *terminal_error1 = "Error getting current terminal attributes.\n";
                                        write(1, terminal_error1, strlen(terminal_error1));
                                        exit(EXIT_FAILURE);
                                }
                                curr_termattr.c_lflag |= ECHO;
                                if (tcsetattr(0, TCSANOW, &curr_termattr)==-1) {
                                        char *terminal_error2 = "Error setting terminal attributes.\n";
                                        write(1, terminal_error2, strlen(terminal_error2));
                                        exit(EXIT_FAILURE);
                                }
				password_prompt_from_server = 0;
			}
		}
	        send_pos = 0;	       
		curr_char = fgetc(stdin);
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
	sigset_t signalset;
	if (sigemptyset(&signalset)==-1) {
		printf("Error setting signalset to empty.%s.\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (sigaddset(&signalset, 23)==-1) { // add SIGURG to signalset
		printf("Error adding SIGURG to signalset.%s.\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (sigaddset(&signalset, 29)==-1) { // add SIGIO to signalset
                printf("Error adding SIGURG to signalset.%s.\n",
                        strerror(errno));
                exit(EXIT_FAILURE);
        }
	if (sigprocmask(SIG_BLOCK, &signalset, NULL)==-1) {
		printf("Error blocking SIGURG process wide. %s.\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}	

	
	int client_socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
        tsd.client_socket_fd = client_socket_fd;
        tsd.input_data = (struct vector *)vector_null_init(sizeof(struct received_data));
        pthread_mutex_init(&(tsd.data_synch_mutex), NULL);

        pthread_t normal_data;
        pthread_t urgent_data;
        pthread_t user_terminal_input;
        if (pthread_create(&urgent_data,
                           NULL,
                           urgent_data_handler,
                           (void *)&tsd) != 0) {

                printf("Error creatinng thread to receive urgent data.\n");
                exit(EXIT_FAILURE);
        }
        if (pthread_create(&user_terminal_input,
                           NULL,
                           handle_user_input,
                           (void *)&tsd) != 0) {
                printf("Error creating thread to handle user input.\n");
                exit(EXIT_FAILURE);
        }
	int get_optval;
	int false_val = 0;
	if (setsockopt(client_socket_fd, 
		       SOL_SOCKET, 
		       SO_OOBINLINE,
		       (void *)&false_val, 
		       sizeof(int)) == -1) {
		printf("Error setting SO_OOBINLINE to false on clinet socket.%s.\b", 
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	fcntl(client_socket_fd, F_SETOWN, getpid()); // set owner process to
                                                     // receive SIGIO and SIGURG
                                                     // signals to his process. Only the urgent_data_handler thread accepts SIGURG signals
        fcntl(client_socket_fd, F_SETFL, O_ASYNC);
	

	// set up signal handler for SIGURG (to handle Data Mark out of band commands)
	struct sigaction sigacturg = {0};
	sigacturg.sa_handler = urghandler;
	if (sigemptyset(&(sigacturg.sa_mask))==-1) {
		printf("Error setting sa_mask for sigurg signal mask to empty. %s.\n", 
			strerror(errno));
		exit(EXIT_FAILURE);
	}
	sigacturg.sa_flags = 0;
	sigaction(23, &sigacturg, NULL); // SIGURG is 23
	//

	int conn_res = -1;
	while (conn_res == -1) {
		printf("Connecting to %s on port %d....\nPress ^] (control and ]) to send a choice of telnet command characters.\n", remote_host, port_val);
		conn_res = connect(client_socket_fd, res->ai_addr, res->ai_addrlen);
		if (conn_res == -1) {
			if (res->ai_next == NULL) {
				printf("Exhausted remote machine address list and"
				       " couldn't connect to any of them.\n"
				       "%s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			res = res->ai_next;
		}
		else {
			printf("Connected successfully.\n");
		}
	}
	
	if (pthread_create(&normal_data,
                           NULL,
                           normal_data_handler,
                           (void *)&tsd) != 0 ) {
                printf("Error creating thread to receive normal data.\n");
                exit(EXIT_FAILURE);
        }

	int options_buffer_len = 10000;
	unsigned char options_buffer[options_buffer_len];

	int output_buffer_len = 100000;
	unsigned char output_buffer[output_buffer_len];
	int output_buffer_no_of_bytes = 0;

	unsigned char treat_all_data_as_urg = 0;
	int NoOfOptions = 0;
	unsigned char carry_OPT = 0;
	unsigned char carry_DOWILL = 0;
	while (1) {
		int no_of_received_segments = vector_get_size(tsd.input_data);
		// parse received data/synch command
		if (no_of_received_segments == 0) {
			continue;
		}
		int segment_index_of_DM = -1;
		int buffer_index_of_DM = -1;
		unsigned char DM_found = 0;
		for (int i=no_of_received_segments-1; i>=0; --i) {
			struct received_data *rd = (struct received_data *)vector_read(tsd.input_data, i);
			for (int j=rd->recv_buffer_len-1; j>=0; --j) {
				if ((rd->recv_buffer)[j] == DM) {
					if (j>0) {
						if ( ((rd->recv_buffer)[j-1])==IAC) {
							segment_index_of_DM = i;
							buffer_index_of_DM = j;
							DM_found = 1;
							break;
						}
						else {
							printf("Expected IAC before DM as per protocol spec. Program terminating.\n");
							exit(EXIT_FAILURE);
						}
					}
					else {
						rd = (struct received_data *)vector_read(tsd.input_data, i-1);
						if ( (rd->recv_buffer)[rd->recv_buffer_len - 1] == IAC) {
							segment_index_of_DM = i;
							buffer_index_of_DM = 0;
							DM_found = 1;
							break;
						}		
						else {
							printf("Expected IAC before DM. Program terminating.\n");
							exit(EXIT_FAILURE);
						}
					}
				}
			}
			if (DM_found) {
				break;
			}
		}
		if (segment_index_of_DM == -1) {
			// no IAC DM found. 
			if (treat_all_data_as_urg) {
				// continue treating all data as urgent
				//
				for (int i=0; i<no_of_received_segments; ++i) {
					struct received_data *curr_data_segment = (struct received_data *)vector_read(tsd.input_data, i);
					for (int j=0; j<curr_data_segment->recv_buffer_len; ++j) {
						if ( (j==0) && (carry_DOWILL || carry_OPT) ) {
							if (carry_DOWILL) {
								if ((curr_data_segment->recv_buffer)[0] == IP) {
									printf("IP command received from server. Program terminating.\n");
									exit(EXIT_SUCCESS);
								}
								else if ( ((curr_data_segment->recv_buffer)[0] == DO) ||
									  ((curr_data_segment->recv_buffer)[0] == WILL) ) {
									if (curr_data_segment->recv_buffer_len > 1) {
										int options_buffer_index = NoOfOptions*3; // should be zero
										options_buffer[options_buffer_index++] = IAC;
										if ((curr_data_segment->recv_buffer)[0] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}				
										else {
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (curr_data_segment->recv_buffer)[1];
										++NoOfOptions;
										++j;
										carry_DOWILL = 0;
									}
									else {
										carry_OPT = 1;
										int options_buffer_index = NoOfOptions*3;
										output_buffer[options_buffer_index++] = IAC;
										if ((curr_data_segment->recv_buffer)[0] == DO) {
                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                }
                                                                                else {
                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                }
										carry_DOWILL = 0;
									}
								}
								 
							}
							else {
								int options_buffer_index = NoOfOptions*3;
								options_buffer[options_buffer_index+2] = (curr_data_segment->recv_buffer)[j];
								++NoOfOptions;
								carry_OPT = 0;
							}
						}
						else {
							if ( (curr_data_segment->recv_buffer)[j] == IAC) {
								if (j==(curr_data_segment->recv_buffer_len)-1) {
									carry_DOWILL = 1;
								}
								else {
									if ((curr_data_segment->recv_buffer)[j+1] == IP) {
										printf("Received IAC IP command. Program terminating.\n");
										exit(EXIT_SUCCESS);
									}
									else if ( ((curr_data_segment->recv_buffer)[j+1]==DO) ||
										  ((curr_data_segment->recv_buffer)[j+1]==WILL) ) {
										if ((j+1)==(curr_data_segment->recv_buffer_len-1)) {
											int options_buffer_index = NoOfOptions*3;
											options_buffer[options_buffer_index++] = IAC;
											if ((curr_data_segment->recv_buffer)[j+1]==DO) {
												options_buffer[options_buffer_index] = WONT;
											}
											else {
												options_buffer[options_buffer_index] = DONT;
											}
											carry_OPT = 1;
											++j;	
										}
										else {
											int options_buffer_index = NoOfOptions*3;
                                                                                        options_buffer[options_buffer_index++] = IAC;
                                                                                        if ((curr_data_segment->recv_buffer)[j+1]==DO) {
                                                                                                options_buffer[options_buffer_index++] = WONT;
                                                                                        }
                                                                                        else {
                                                                                                options_buffer[options_buffer_index++] = DONT;
                                                                                        }
											options_buffer[options_buffer_index] = (curr_data_segment->recv_buffer)[j+2];
											j += 2;
											++NoOfOptions;
										}
									}	
								}
							}
						}
					}
				}
				int no_of_option_bytes = NoOfOptions*3;
				if (send(tsd.client_socket_fd, options_buffer, no_of_option_bytes, 0)!= no_of_option_bytes) {
					printf("Error sending option responses. Program temrinating.\n");
					exit(EXIT_FAILURE);
				}
				if (carry_OPT) {
					output_buffer[0] = IAC;
					output_buffer[1] = output_buffer[no_of_option_bytes+1];
				}
				NoOfOptions = 0;
				for (int i=0; i<no_of_received_segments; ++i) {
					struct received_data *rd = (struct received_data *)vector_read(tsd.input_data, i);
					free(rd->recv_buffer);
				}
				for (int i=0; i<no_of_received_segments; ++i) {
					vector_remove(tsd.input_data, i);	
				}
			}
			else {
				// treat_all_data_as_urg == 0
				// handle only the first data segment
			 	// NoOfOptions == 0
				struct received_data *first_data_segment = (struct received_data *)vector_read(tsd.input_data, 0);
				if (first_data_segment->type == 1) {
					treat_all_data_as_urg = 1;
					for (int i=0; i<(first_data_segment->recv_buffer_len); ++i) {
						if ((i==0) && (carry_DOWILL || carry_OPT)) {
							if (carry_DOWILL) {
								if ( (first_data_segment->recv_buffer)[0] == IP) {
									printf("Received IP command. Program terminating.\n");
									exit(EXIT_SUCCESS);
								}
								else if ( ((first_data_segment->recv_buffer)[0] == DO) ||
									  ((first_data_segment->recv_buffer)[0] == WILL) ) {
									if (first_data_segment->recv_buffer_len > 1) {
										carry_DOWILL =0;
										int options_buffer_index = NoOfOptions*3;
										options_buffer[options_buffer_index++] = IAC;
										if ((first_data_segment->recv_buffer)[0] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (first_data_segment->recv_buffer)[1];
										++NoOfOptions;
										++i;
									}
									else {
										carry_DOWILL = 0;
										carry_OPT = 1;
										int options_buffer_index = NoOfOptions*3;
                                                                                options_buffer[options_buffer_index++] = IAC;
										if ((first_data_segment->recv_buffer)[0] == DO) {
                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                }
                                                                                else {
                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                }
									}
								}
							}
							else {
								options_buffer[2] = (first_data_segment->recv_buffer)[0];
								++NoOfOptions; // NoOfOptions now equals 1
								carry_OPT = 0;
							}
						}
						else if ( (first_data_segment->recv_buffer)[i] == IAC) {
							if (i==(first_data_segment->recv_buffer_len -1)) {
								carry_DOWILL = 1;
							}
							else {
								if ((first_data_segment->recv_buffer)[i+1] == IP) {
									printf("Interrupt process command received. Program terminating.\n");
									exit(EXIT_SUCCESS);
								}
								else if ( ((first_data_segment->recv_buffer)[i+1] == DO) ||
								          ((first_data_segment->recv_buffer)[i+1] == WILL) ) {
									if ((i+2) < (first_data_segment->recv_buffer_len)) {
										int options_buffer_index = NoOfOptions*3;
										options_buffer[options_buffer_index++] = IAC;
										if ((first_data_segment->recv_buffer)[i+1] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (first_data_segment->recv_buffer)[i+2];
										i+=2;
										++NoOfOptions;
									}
									else {
										carry_OPT = 1;
										int options_buffer_index = NoOfOptions*3;
                                                                                options_buffer[options_buffer_index++] = IAC;
                                                                                if ((first_data_segment->recv_buffer)[i+1] == DO) {
                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                }
                                                                                else {
                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                }
										++i;
									}
								}
								else {
									++i;
								}
							}
						}		
					}
					int options_no_of_bytes = NoOfOptions*3;
					if (send(tsd.client_socket_fd, output_buffer, options_no_of_bytes, 0) != options_no_of_bytes) {
						printf("Error sending options responses.\n");
						exit(EXIT_FAILURE);
					}
					if (carry_OPT) {
						options_buffer[0] = IAC;
						options_buffer[1] = options_buffer[options_no_of_bytes+1];
					}
					free(first_data_segment->recv_buffer);
					vector_remove(tsd.input_data, 0);
				}
				else {
					// NoOfOptions == 0
					// treat_all_data_as_urg == 0
					int recv_buffer_len = first_data_segment->recv_buffer_len;		
					unsigned char abort_output_received = 0;
					unsigned char ga_received = 0;
					unsigned char abort_output_index = -1;
					unsigned char special_chars[6] = {8,9,10,11,12,13};
					// filter out charatcers the nvt doesnt recognize
					for (int i=0; i<recv_buffer_len; ++i) {
						unsigned char valid_char = 0;
						for (int j=0; j<6; ++j) {
							if ((first_data_segment->recv_buffer)[i] == special_chars[j]) {
								valid_char = 1;
								if ((first_data_segment->recv_buffer)[i] == 13) {
									
								}
								break;
							}
						}
						if (((first_data_segment->recv_buffer)[i]>=32) && ((first_data_segment->recv_buffer)[i]<=126)) {
							valid_char = 1;
						}
						if ((i==0) && carry_DOWILL) {
							if ( ((first_data_segment->recv_buffer)[0] == DO) ||
							     ((first_data_segment->recv_buffer)[0] == WILL) ) {
								++i;
								continue;
							}
							else {
								continue;
							}
						}
						else if ( (i==0) && carry_OPT) {
							continue;
						}
						if ((first_data_segment->recv_buffer)[i] == IAC) {
							if (i<(recv_buffer_len-1)) {
								if ( ((first_data_segment->recv_buffer)[i+1] == DO) ||
								     ((first_data_segment->recv_buffer)[i+1] == WILL) ) {
									i+=2;
									continue;				
								}
								else {
									++i;
									continue;
								}
							}
							else {
								break;
							}
						}
						if (!valid_char) {
							// filoter out character
							for (int j=i; j<recv_buffer_len-1; ++j) {
								(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
							}
							--recv_buffer_len;
							--i;
						}
					}
					for (int i=0; i<recv_buffer_len; ++i) {
						if ( (i==0) && 
						     //(unurgent_not_specially_treated_data_processed==1) &&
						     (carry_DOWILL || carry_OPT) ) {
							if (carry_DOWILL) {
								if ((first_data_segment->recv_buffer)[0] == IP) {
									printf("Client reeived IAC IP from server. Program terminating.\n");
									exit(EXIT_SUCCESS);
								}
								else if ( (first_data_segment->recv_buffer)[0] == IAC) {
									carry_DOWILL = 0;
									for (int j=i; j<recv_buffer_len-1; ++j) {
										(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
									}
									--recv_buffer_len;
									--i;
								}
								else if ( (first_data_segment->recv_buffer)[0] == AO) {
									// Abort Output received.
									abort_output_received = 1;
									abort_output_index = 0;
									for (int j=0; j<recv_buffer_len-1; ++j) {
										(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
									}
									--recv_buffer_len;
									i=-1;
									carry_DOWILL = 0;
								}
								else if ( (first_data_segment->recv_buffer)[0] == EC) {
									// Erase Character received
									if (output_buffer_no_of_bytes == 0) {
										carry_DOWILL = 0;
										for (int j=0; j<recv_buffer_len-1; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
										}	
										--recv_buffer_len;
										--i;	
									}
									else {
										carry_DOWILL = 0;
										(first_data_segment->recv_buffer)[0] = 8;
									}
								}
								else if ( (first_data_segment->recv_buffer)[0] == EL) {
									// Erase Line received
									if (output_buffer_no_of_bytes == 0) {
										carry_DOWILL = 0;
										for (int j=0; j<recv_buffer_len-1; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
										}
										--recv_buffer_len;
										--i;
									}	
									else {
										unsigned char *new_recv_buffer = (unsigned char *)malloc(output_buffer_no_of_bytes
																	 +recv_buffer_len-1);
										if (new_recv_buffer == NULL) {
											printf("Error allocating memory for new received buffer.\n");
											exit(EXIT_FAILURE);		
										}
										memset(new_recv_buffer, 8, output_buffer_no_of_bytes);
										memcpy(&(new_recv_buffer[output_buffer_no_of_bytes]),
										       &((first_data_segment->recv_buffer)[1]),
										       recv_buffer_len-1);
										free(first_data_segment->recv_buffer);
										first_data_segment->recv_buffer = new_recv_buffer;
										first_data_segment->recv_buffer_len += (output_buffer_no_of_bytes-1);
										--i;
										carry_DOWILL = 0;
									}
								}
								else if ((first_data_segment->recv_buffer)[0] = GA) {
									// GA command received as first byte.
									ga_received = 1;
									for (int j=0; j<recv_buffer_len-1; ++j) {
										(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
									}
									--recv_buffer_len;
									i=-1;
									carry_DOWILL = 0;
								}
								else if ( ((first_data_segment->recv_buffer)[0] == DO) ||
									  ((first_data_segment->recv_buffer)[0] == WILL) ) {
									if (first_data_segment->recv_buffer_len>1) {
										options_buffer[0] = IAC;
										if ((first_data_segment->recv_buffer)[0] == DO) {
											options_buffer[1] = WONT;
										}
										else {
											options_buffer[1] = DONT;
										}
										options_buffer[2] = (first_data_segment->recv_buffer)[1];
										++NoOfOptions;
										for (int j=0; j<recv_buffer_len-2; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+2];
										}
										recv_buffer_len -= 2;
										i=-1;
										carry_DOWILL = 0;
									}	
									else {
										carry_OPT = 1;
										carry_DOWILL = 0;
										options_buffer[0] = IAC;
										if ((first_data_segment->recv_buffer)[0] == DO) {
											options_buffer[1] = WONT;
										}
										else {
											options_buffer[1] = DONT;
										}
										--recv_buffer_len;												
									}	
								}
								else  {
									carry_DOWILL = 0;
									for (int j=0; j<recv_buffer_len-1; ++j) {
										(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
									}
									--recv_buffer_len;
									i=-1;
								}
							}
							else {
								options_buffer[2] = (first_data_segment->recv_buffer)[0];
								++NoOfOptions;
								for (int j=0; j<recv_buffer_len-1; ++j) {
								       (first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
								}
								--recv_buffer_len;
								i=-1;
								carry_OPT = 0;
							}
						}
						else if ((first_data_segment->recv_buffer)[i] == IAC) {
							if (i==(recv_buffer_len - 1)) {
								carry_DOWILL = 1;
								--recv_buffer_len;	
							}
							else {
								if ((first_data_segment->recv_buffer)[i+1] == IP) {
									printf("Received Interrupt Process command from server. Client terminating.\n");
									exit(EXIT_SUCCESS);			
								}	
								else if ( (first_data_segment->recv_buffer)[i+1] == AO) {
									if (!abort_output_received) {
										abort_output_received = 1;
										abort_output_index = i;
									}
									for (int j=i; j<recv_buffer_len-2; ++j) {
										(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+2];
									}
									recv_buffer_len -= 2;
									--i;
								}
								else if ( (first_data_segment->recv_buffer)[i+1] == IAC) {
									for (int j=i; j<recv_buffer_len-2; ++j) {
										(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+2];
									}
									recv_buffer_len -= 2;
									--i;
								}
								else if ( (first_data_segment->recv_buffer)[i+1] == EC) {
									int print_position_starting_index = get_previous_print_position(first_data_segment->recv_buffer,
																	i+1);
																	
									if (print_position_starting_index == -1) {
										// previous print position either lies in output_buffer or before output_buffer,
										// if it lies before output buffer we output backspaces until we reach the first character
										// after the newline in output_buffer
										unsigned char *previous_and_current_output = (unsigned char *)malloc(output_buffer_no_of_bytes+i+2);
										if (previous_and_current_output == NULL) {
											printf("Error allocating memory with malloc. Program terminating.\n");
											exit(EXIT_FAILURE);
										}
										memcpy(previous_and_current_output, output_buffer,output_buffer_no_of_bytes);
										memcpy(&(previous_and_current_output[output_buffer_no_of_bytes]),
										       first_data_segment->recv_buffer, i+2);
										int EC_index = output_buffer_no_of_bytes+i+1;
										int no_of_backspaces = -1;
										print_position_starting_index = get_previous_print_position(previous_and_current_output, EC_index);
										if (print_position_starting_index == -1) {
											// last print position lises before  newline in output_buffer
											// print backspaces to get back to character after output_buffer newline
											if (output_buffer_no_of_bytes < 2) {
												no_of_backspaces = get_no_of_bs_from_curr_output(first_data_segment->recv_buffer,
															       i+1);
											}
											else {		
												no_of_backspaces = get_no_of_bs_to_output(previous_and_current_output,
																	  EC_index, 
																	  1);	
											}
										}
										else {
											no_of_backspaces = get_no_of_bs_to_output(previous_and_current_output, EC_index,
																  print_position_starting_index);		
										}
										unsigned char *new_recv_buffer = (unsigned char *)malloc(recv_buffer_len + no_of_backspaces -2);
										if (new_recv_buffer == NULL) {
											printf("Error allocating memory to insert backspaces after IAC EC.\n");
											exit(EXIT_FAILURE);
										}	
										memcpy(new_recv_buffer, first_data_segment->recv_buffer, i);
										memset(&(new_recv_buffer[i]), 8, no_of_backspaces);
										memcpy(&(new_recv_buffer[i+no_of_backspaces]), 
										       &((first_data_segment->recv_buffer)[i+2]), recv_buffer_len - (i+2));
										free(first_data_segment->recv_buffer);
										first_data_segment->recv_buffer = new_recv_buffer;
										first_data_segment->recv_buffer_len = recv_buffer_len - 2 + no_of_backspaces;
										recv_buffer_len = first_data_segment->recv_buffer_len;
										--i;	   	
									}
									else {
										int gap = i+2 - print_position_starting_index;
										for (int j=print_position_starting_index; j<recv_buffer_len-gap; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+gap];
										}
										recv_buffer_len -= gap;
										--i;
									}
									
								}
								else if ( (first_data_segment->recv_buffer)[i+1] == EL) {
									int sol_index = get_start_of_current_line_index(first_data_segment->recv_buffer, i+1);
									if (sol_index < 0 ) {
										int no_of_backspaces = -1;
										if (output_buffer[0] == '\n') {
											unsigned char *temp_buffer = (unsigned char *)malloc(output_buffer_no_of_bytes +
																	     i+2);
											if (temp_buffer == NULL) {
												printf("Error allocating buffer for previous line and currently received data.\n");
												exit(EXIT_FAILURE);
											}			
											memcpy(temp_buffer, output_buffer, output_buffer_no_of_bytes);
											memcpy(&(temp_buffer[output_buffer_no_of_bytes]), 
											       first_data_segment->recv_buffer,
											       i+2);
											int EL_index = output_buffer_no_of_bytes + (i+1);
											no_of_backspaces = get_no_of_bs_to_output(temp_buffer,
																  EL_index,
																  1);
											free(temp_buffer);
											unsigned char *new_recv_buffer = (unsigned char *)malloc(first_data_segment->recv_buffer_len - 2 + no_of_backspaces);
											memcpy(new_recv_buffer, first_data_segment->recv_buffer, i);
											memset(&(new_recv_buffer[i]), 8, no_of_backspaces);
											memcpy(&(new_recv_buffer[i+8]), 
											       &((first_data_segment->recv_buffer)[i+2]),
											       recv_buffer_len -(i+2));
											free(first_data_segment->recv_buffer);
											first_data_segment->recv_buffer = new_recv_buffer;
											first_data_segment->recv_buffer_len = recv_buffer_len - 2 + no_of_backspaces;	
											recv_buffer_len = first_data_segment->recv_buffer_len;
											--i;
										}
										else {
											for (int j=0; j<recv_buffer_len-(i+2); ++j) {
												(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[i+2+j];
											}
											first_data_segment->recv_buffer_len = recv_buffer_len - (i+2);
											recv_buffer_len -= (i+2);
											i=-1;
										}
									}
									else {
										for (int j=0; j<recv_buffer_len-(i+2); ++j) {
											(first_data_segment->recv_buffer)[sol_index+j] = (first_data_segment->recv_buffer)[i+2+j];
										}	
										first_data_segment->recv_buffer_len -= (i+1 - sol_index) + 1;
										recv_buffer_len = first_data_segment->recv_buffer_len;
										i = sol_index - 1;
									}
								}
								else if ( ((first_data_segment->recv_buffer)[i+1] == DO) ||
									  ((first_data_segment->recv_buffer)[i+1] == WILL) ) {
									if ((i+1)==(recv_buffer_len-1)) {
										int options_buffer_index = NoOfOptions*3;
										if ((first_data_segment->recv_buffer)[i+1] == DO) {
											options_buffer[options_buffer_index++] = IAC;
											options_buffer[options_buffer_index++] = WONT;		
										}
										else {
											options_buffer[options_buffer_index++] = IAC;
											options_buffer[options_buffer_index++] = DONT;
										}
										carry_OPT = 1;
										recv_buffer_len -= 2;
									}
									else {
										int options_buffer_index = NoOfOptions*3;
										if ((first_data_segment->recv_buffer)[i+1] == DO) {
											options_buffer[options_buffer_index++] = IAC;
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											options_buffer[options_buffer_index++] = IAC;
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (first_data_segment->recv_buffer)[i+2];
										for (int j=i+3; j<recv_buffer_len; ++j) {
											(first_data_segment->recv_buffer)[j-3] =(first_data_segment->recv_buffer)[j];
										}
										recv_buffer_len -= 3;
										--i;
										first_data_segment->recv_buffer_len = recv_buffer_len;
										++NoOfOptions;
									}
								}
								else if ( (first_data_segment->recv_buffer)[i+1] == GA) {
									for (int j=i+2; j<recv_buffer_len; ++j) {
										(first_data_segment->recv_buffer)[j-2] =(first_data_segment->recv_buffer)[j];
									}
									recv_buffer_len -= 2;
									--i;
									first_data_segment->recv_buffer_len = recv_buffer_len;
								}
								else {
									// command unsupported or has no effect
									for (int j=i+2; j<recv_buffer_len; ++j) {
										(first_data_segment->recv_buffer)[j-2] =(first_data_segment->recv_buffer)[j];
									}
									recv_buffer_len -= 2;
									--i;
									first_data_segment->recv_buffer_len = recv_buffer_len;
								}
							}
						}	
					}
					first_data_segment->recv_buffer_len = recv_buffer_len;
					int no_of_option_bytes_to_send = NoOfOptions*3;
					if (send(tsd.client_socket_fd, 
						 options_buffer, 
						 no_of_option_bytes_to_send, 0) != no_of_option_bytes_to_send) {
						printf("Error sending option responses.\n");
						exit(EXIT_FAILURE);
					}
					if (carry_OPT) {
						output_buffer[0] = IAC;
						output_buffer[1] = output_buffer[no_of_option_bytes_to_send+1];
					}			
					NoOfOptions = 0;
					if (abort_output_received) {
						unsigned char newline_present = 0;
						int newline_index = -1;
						for (int i=abort_output_index-1; i>=0; --i) {
							if ((first_data_segment->recv_buffer)[i] == '\n') {
								newline_present = 1;
								newline_index = i;
								break;
							}
						}	
						if (newline_present) {
							output_buffer_no_of_bytes = abort_output_index - newline_index;
							memcpy(output_buffer, 
							       &((first_data_segment->recv_buffer)[newline_index]),
							       output_buffer_no_of_bytes);
						}
						else {
							memcpy(&(output_buffer[output_buffer_no_of_bytes]),
							       first_data_segment->recv_buffer,
					       		       recv_buffer_len);
	 						output_buffer_no_of_bytes += recv_buffer_len;						
						}
						char *password_str = "Password: ";
						int passwd_str_len = strlen(password_str);
						for (int i=0; i<abort_output_index-(passwd_str_len-1); ++i) {
							if (strncmp(&((first_data_segment->recv_buffer)[i]),
								    password_str,
						    		    passwd_str_len)==0) {
								password_prompt_from_server = 1;
								// turn local echo off
								struct termios curr_termattr;
								if (tcgetattr(0, &curr_termattr)==-1) {
									char *terminal_error1 = "Error getting current terminal attributes.\n";
									write(1, terminal_error1, strlen(terminal_error1));
									exit(EXIT_FAILURE);
								}
								curr_termattr.c_lflag &= (~ECHO);
								if (tcsetattr(0, TCSANOW, &curr_termattr)==-1) {
									char *terminal_error2 = "Error setting terminal attributes.\n";
									write(1, terminal_error2, strlen(terminal_error2));
									exit(EXIT_FAILURE);
								}
								break;
							}			
						}
						write(1, first_data_segment->recv_buffer, abort_output_index);
					}
					else {
						unsigned char newline_present = 0;
						int newline_index = -1;
						for (int i=recv_buffer_len-1; i>=0; --i) {
							if ( (first_data_segment->recv_buffer)[i] == '\n') {
								newline_present = 1;
								newline_index = i;
								break;
							}
						}
						if (newline_present) {
							memcpy(output_buffer, 
							       &((first_data_segment->recv_buffer)[newline_index]),
							       recv_buffer_len-newline_index);
							output_buffer_no_of_bytes = recv_buffer_len-newline_index;	
						}
						else {
							memcpy(&(output_buffer[output_buffer_no_of_bytes]),
								first_data_segment->recv_buffer,
								recv_buffer_len);
							output_buffer_no_of_bytes += recv_buffer_len;
						}
						char *password_str = "Password: ";
						int passwd_str_len = strlen(password_str);
                                                for (int i=0; i<recv_buffer_len-(passwd_str_len-1); ++i) {
                                                        if (strncmp(&((first_data_segment->recv_buffer)[i]),
                                                                    password_str,
                                                                    passwd_str_len)==0) {
                                                                password_prompt_from_server = 1;
                                                                // turn local echo off
                                                                struct termios curr_termattr;
                                                                if (tcgetattr(0, &curr_termattr)==-1) {
                                                                        char *terminal_error1 = "Error getting current terminal attributes.\n";
                                                                        write(1, terminal_error1, strlen(terminal_error1));
                                                                        exit(EXIT_FAILURE);
                                                                }
                                                                curr_termattr.c_lflag &= (~ECHO);
                                                                if (tcsetattr(0, TCSANOW, &curr_termattr)==-1) {
                                                                        char *terminal_error2 = "Error setting terminal attributes.\n";
                                                                        write(1, terminal_error2, strlen(terminal_error2));
                                                                        exit(EXIT_FAILURE);
                                                                }
                                                                break;
                                                        }
                                                }
						write(1, first_data_segment->recv_buffer, recv_buffer_len); // write received (processed) data to terminal for user to view	
					}
					free(first_data_segment->recv_buffer);
					vector_remove(tsd.input_data, 0);
					
				}
			}
		}		
		else {
			struct received_data *rd = (struct received_data *)vector_read(tsd.input_data, segment_index_of_DM);
			if ((rd->type == 1) && ((rd->recv_buffer_len - 1)==buffer_index_of_DM)) {
				// DM was last byte of urgent type received data segment
				// Process data
				for (int i=0; i<=segment_index_of_DM; ++i) {
					struct received_data *curr_data_segment = (struct received_data *)vector_read(tsd.input_data, i);
					for (int j=0; j<curr_data_segment->recv_buffer_len; ++j) {
						if ((carry_DOWILL) || (carry_OPT)) {
							// NoOfOptions was set to zero on previous received messages
							if (carry_DOWILL) {
								if ((curr_data_segment->recv_buffer)[0] == IP) {
									printf("Received IAC IP command. Terminating client program,\n");
									exit(EXIT_SUCCESS);
								}
								else if ( ((curr_data_segment->recv_buffer)[0] == DO) ||
									  ((curr_data_segment->recv_buffer)[0] == WILL) ) {
									if ((curr_data_segment->recv_buffer_len) > 1) {
										options_buffer[0] = IAC;
										if ((curr_data_segment->recv_buffer)[0] == DO) {
											options_buffer[1] = WONT;
											options_buffer[2] = (curr_data_segment->recv_buffer)[1];
										}	
										else {
											options_buffer[1] = DONT;
											options_buffer[2] = (curr_data_segment->recv_buffer)[1];
										}
										NoOfOptions = 1;
										j=1;
									}
									else {
										// i+1 guaranteed to be <= segment_index_of_DM
										struct received_data *next_segment = (struct received_data *)vector_read(tsd.input_data, i+1);
										options_buffer[0] = IAC;
										if ( (curr_data_segment->recv_buffer)[0] == DO) {
											options_buffer[1] = WONT;
										}
										else {
											options_buffer[1] = DONT;
										}
										options_buffer[2] = (next_segment->recv_buffer)[0];
										NoOfOptions = 1;
										++i;
										curr_data_segment = (struct received_data *)vector_read(tsd.input_data, i);
									}
								}
								carry_DOWILL = 0;
							}
							else {
								// options_buffer[0] and options_buffer[1] have been set to IAC WONT/DONT and is missing the
								// final option byte. NoOfBytes has been set to zero
								options_buffer[2] = (curr_data_segment->recv_buffer)[0];
								NoOfOptions = 1;	
								carry_OPT = 0;
							}
							continue;
						}
						if ( (curr_data_segment->recv_buffer)[j] == IAC) {
							unsigned char OPT_cmd = 0;
							if (j<(curr_data_segment->recv_buffer_len-1)) {
								if ( ((curr_data_segment->recv_buffer)[j+1] == DO) ||
								     ((curr_data_segment->recv_buffer)[j+1] == WILL) ) {
									OPT_cmd = 1;
								}
								else if ( (curr_data_segment->recv_buffer)[j+1] == IP) {
									printf("Received IP command. Program terminating.\n");
									exit(EXIT_SUCCESS);
								}
								if (OPT_cmd == 1) {
									if ((j+1) < (curr_data_segment->recv_buffer_len - 1)) {
										int options_buffer_index = NoOfOptions*3;
										options_buffer[options_buffer_index++] = IAC;
										if ((curr_data_segment->recv_buffer)[j+1] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (curr_data_segment->recv_buffer)[j+2];
									}
									else {
										if (i==segment_index_of_DM) {
											printf("Error DO/WILL last byte of urgent data segment when it should be DM.\n"
											       " Program terminating.\n");
											exit(EXIT_FAILURE);
										}
										struct received_data *next_data_segment = vector_read(tsd.input_data, i+1);
										int options_buffer_index = NoOfOptions*3;
                                                                                options_buffer[options_buffer_index++] = IAC;
                                                                                if ((curr_data_segment->recv_buffer)[j+1] == DO) {
                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                }
                                                                                else {
                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                }
                                                                                options_buffer[options_buffer_index] = (next_data_segment->recv_buffer)[0];	
									}
									++NoOfOptions;
								}
							}
							else {
								if (i==segment_index_of_DM) {
									printf("IAC was last byte of urgent segment when it should be DM. Program terminating.\n");
									exit(EXIT_FAILURE);
								}
								struct received_data *next_data_segment = (struct received_data *)vector_read(tsd.input_data, i+1);
								if ( ((next_data_segment->recv_buffer)[0] == DO) ||
								     ((next_data_segment->recv_buffer)[0] == WILL) ) {
									OPT_cmd = 1;
								}
								else if ((next_data_segment->recv_buffer)[0] == IP) {
									printf("Received IP command. Program terminating.\n");
                                                                        exit(EXIT_SUCCESS);
								}
								if (OPT_cmd == 1) {
									if ((next_data_segment->recv_buffer_len)>1) {
										int options_buffer_index = NoOfOptions*3;
                                                                                options_buffer[options_buffer_index++] = IAC;
										if ( (next_data_segment->recv_buffer)[0] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = next_data_segment->recv_buffer[1];
										++NoOfOptions;
									}	
									else {
										if ((i+2) > segment_index_of_DM) {
											printf("Error parsing IAC DO/WILL command received as it passes the final DM"
											       "\n when it shouldn't.\n");
											exit(EXIT_FAILURE);
										}
										struct received_data *opt_code_segment = (struct received_data *)vector_read(tsd.input_data,
																		i+2);
										int options_buffer_index = NoOfOptions*3;
										options_buffer[options_buffer_index++] == IAC;
										if ((next_data_segment->recv_buffer)[0] == DO) {
											options_buffer[options_buffer_index++] = WONT;	
										}
										else {
											options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (opt_code_segment->recv_buffer)[0];
										++NoOfOptions;
									}	
								}	
								
							}
						}
					}
				}
				// send options
				int options_buffer_byte_count = NoOfOptions*3;
				if (send(tsd.client_socket_fd, options_buffer, options_buffer_byte_count, 0)==-1) {
					printf("Error sending telnet options. Program terminating.\n");
					exit(EXIT_FAILURE);
				}
				// processed options, remove processed data from input_data vector
				for (int i=0; i<=segment_index_of_DM; ++i) {
					struct received_data *rd = (struct received_data *)vector_read(tsd.input_data, i);
					free(rd->recv_buffer);
				}	
				for (int i=0; i<=segment_index_of_DM; ++i) {
					vector_remove(tsd.input_data, 0);
				}
				NoOfOptions = 0;
			}
			else if (rd->type == 1) {
				// last DM was before the last byte in the urgent segment segment_index_of_DM
				// process all no_of_received_segments segments specially
				// and set treat_all_data_as_urg byte to 1
				treat_all_data_as_urg = 1;
				for (int i=0; i<no_of_received_segments; ++i) {
					struct received_data *recvdata = (struct received_data *)vector_read(tsd.input_data, i);
					for (int j=0; j<recvdata->recv_buffer_len; ++j) {
						if ((j==0) && (carry_DOWILL || carry_OPT)) {
							// NoOfOptions == 0 if we are here, was set during previous set of data segments received
							if (carry_DOWILL) {
                                                                if ((recvdata->recv_buffer)[0] == IP) {
                                                                        printf("Received IAC IP command. Terminating client program,\n");
                                                                        exit(EXIT_SUCCESS);
                                                                }
                                                                else if ( ((recvdata->recv_buffer)[0] == DO) ||
                                                                          ((recvdata->recv_buffer)[0] == WILL) ) {
                                                                        if ((recvdata->recv_buffer_len) > 1) {
                                                                                options_buffer[0] = IAC;
                                                                                if ((recvdata->recv_buffer)[0] == DO) {
                                                                                        options_buffer[1] = WONT;
                                                                                        options_buffer[2] = (recvdata->recv_buffer)[1];
                                                                                }
                                                                                else {
                                                                                        options_buffer[1] = DONT;
                                                                                        options_buffer[2] = (recvdata->recv_buffer)[1];
                                                                                }
                                                                                NoOfOptions = 1;
                                                                                j=1;
                                                                        }
                                                                        else {
										int options_buffer_index = NoOfOptions*3;
										carry_OPT = 1;	
                                                                                options_buffer[options_buffer_index++] = IAC;
                                                                                if ( (recvdata->recv_buffer)[0] == DO) {
                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                }
                                                                                else {
                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                } 
                                                                        }
                                                                }
                                                                carry_DOWILL = 0;
                                                        }
                                                        else {
                                                                // options_buffer[0] and options_buffer[1] have been set to IAC WONT/DONT and is missing the
                                                                // final option byte. NoOfBytes has been set to zero
                                                                options_buffer[2] = (recvdata->recv_buffer)[0];
                                                                NoOfOptions = 1;
                                                                carry_OPT = 0;
                                                        }
                                                        continue;		
								
						}
						else if ((recvdata->recv_buffer)[j] == IAC) {
							unsigned char OPT_cmd = 0;
							if ( (j < ((recvdata->recv_buffer_len) - 1)) &&
							     ( ((recvdata->recv_buffer)[j+1]==DO) || 
							       ((recvdata->recv_buffer)[j+1]==WILL) ||
							       ((recvdata->recv_buffer)[j+1]==IP) ) ) {
								if ((recvdata->recv_buffer)[j+1] == IP) {
									printf("IP command received. Program terminating.\n");
									exit(EXIT_SUCCESS);
								}
								else {
									OPT_cmd = 1;
									if ((j+2) < (recvdata->recv_buffer_len)) {
										int options_buffer_index = NoOfOptions*3;
										options_buffer[options_buffer_index++] = IAC;
										if ((recvdata->recv_buffer)[j+1] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											options_buffer[options_buffer_index++] = DONT;																       }
										options_buffer[options_buffer_index] = (recvdata->recv_buffer)[j+2];
										++NoOfOptions;
									}
									else {
										if ((i+1)>=no_of_received_segments) {
											int options_buffer_index = NoOfOptions*3;
											options_buffer[options_buffer_index++] = IAC;
											if ((recvdata->recv_buffer)[j+1] == DO) {
												options_buffer[options_buffer_index++] = WONT;
											}
											else {
												options_buffer[options_buffer_index++] = DONT;
											}
											carry_OPT = 1;
											++j;
											break;																					       }
										struct received_data *next_segment = (struct received_data *)vector_read(tsd.input_data, i+1);
										int options_buffer_index = NoOfOptions*3;
										options_buffer[options_buffer_index++] = IAC;
										if ((recvdata->recv_buffer)[j+1] == DO) {
											options_buffer[options_buffer_index++] = WONT;
										}
										else {
											 options_buffer[options_buffer_index++] = DONT;
										}
										options_buffer[options_buffer_index] = (next_segment->recv_buffer)[0];
										++NoOfOptions;
									}
								}
							}
							else if (j==(recvdata->recv_buffer_len-1)) {
								carry_DOWILL = 1;
							}
							else {  // encountered IAC CMD that is not relevant to urgent processing. Skip it
								++j;
							}
							
						}
					}
				}
				int options_buffer_byte_count = NoOfOptions*3;
				if (send(tsd.client_socket_fd, options_buffer, options_buffer_byte_count, 0)!= options_buffer_byte_count) {
					printf("Error sending option requests to server.\n");
					exit(EXIT_FAILURE);
				}	
				if (carry_OPT) {
					options_buffer[0] = IAC;
					options_buffer[1] = options_buffer[options_buffer_byte_count+1];
				}
				NoOfOptions = 0;
				// delete data from input vector as it has been handled
				for (int i=0; i<vector_get_size(tsd.input_data); ++i) {
					struct received_data *data = (struct received_data *)vector_read(tsd.input_data, i);
					free(data->recv_buffer);
				}
				for (int i=0; i<vector_get_size(tsd.input_data); ++i) {
					vector_remove(tsd.input_data, 0);
				}
			}
			else {
				// Last DM not in an urgent segment of received data
				// Only process the first piece of data and then continue the while loop
				if (treat_all_data_as_urg) {
					struct received_data *curr_data_segment = (struct received_data *)vector_read(tsd.input_data, 0);
					if ((curr_data_segment->type) == 1) {
				 		unsigned char last_byte_DM_cmd = 0;
						if ((curr_data_segment->recv_buffer)[(curr_data_segment->recv_buffer_len -1)] == DM) {
							if ((curr_data_segment->recv_buffer_len == 1) && (carry_DOWILL==1)) {
								last_byte_DM_cmd = 1;
							}
							else if ((curr_data_segment->recv_buffer)[(curr_data_segment->recv_buffer_len)-2]==IAC) {
								last_byte_DM_cmd = 1;
							}
						}
						// NoOfOptions is zero
						for (int i=0; i<curr_data_segment->recv_buffer_len; ++i) {
							if ((i==0) && (carry_DOWILL || carry_OPT)) { 
							       	if (carry_DOWILL) {
									if ( ((curr_data_segment->recv_buffer)[0]==DO) ||
								      	     ((curr_data_segment->recv_buffer)[0]==WILL) ||
								             ((curr_data_segment->recv_buffer)[0]==IP) ) {
										if ((curr_data_segment->recv_buffer)[0] == IP) {
											printf("Received IAC IP. Program terminating.\n");
											exit(EXIT_SUCCESS);
										}
										if ((curr_data_segment->recv_buffer_len)>1) {
											int options_buffer_index = 0;
											options_buffer[options_buffer_index++] = IAC;
											if ((curr_data_segment->recv_buffer)[0] == DO) {
												options_buffer[options_buffer_index++] = WONT;
											}			
											else {
												options_buffer[options_buffer_index++] = DONT;		
											}
											options_buffer[options_buffer_index] = (curr_data_segment->recv_buffer)[1];
											++NoOfOptions;
											++i;
											carry_DOWILL = 0;
										}	
										else {
											options_buffer[0] = IAC;
											if ( (curr_data_segment->recv_buffer)[0] == DO) {
												options_buffer[1] = WONT;
											}
											else {
												options_buffer[1] = DONT;
											}
											carry_OPT = 1;
											carry_DOWILL = 0;
											break; 
										}
									}
								}
								else {
									options_buffer[2] = (curr_data_segment->recv_buffer)[0];
									++NoOfOptions;
									carry_OPT = 0;
								}
							}
							else {
								if ((curr_data_segment->recv_buffer)[i] == IAC) {
									if ((i+2) < (curr_data_segment->recv_buffer_len)) {
										if ( ((curr_data_segment->recv_buffer)[i+1] == DO) ||
										     ((curr_data_segment->recv_buffer)[i+1] == WILL) ||
									             ((curr_data_segment->recv_buffer)[i+1] == IP) ) {
											if ((curr_data_segment->recv_buffer)[i+1] == IP) {
												printf("Received IAC IP. Program terminating.\n");
                                                                                		exit(EXIT_SUCCESS);
											}	
											int options_buffer_index = NoOfOptions*3;
											options_buffer[options_buffer_index++] = IAC;
											if ( (curr_data_segment->recv_buffer)[i+1] == DO) {
												options_buffer[	options_buffer_index++] = WONT;	
											}		
											else {
												options_buffer[options_buffer_index++] = DONT;
											}
											options_buffer[options_buffer_index] = (curr_data_segment->recv_buffer)[i+2];
											++NoOfOptions;
											i+=2;
										}
									}
									else if ((i+1) < (curr_data_segment->recv_buffer_len)) {
										if ((curr_data_segment->recv_buffer)[i+1] == IP) {
											printf("Received IAC IP. Program terminating.\n");
                                                                                        exit(EXIT_SUCCESS);
										}
										if ( ((curr_data_segment->recv_buffer)[i+1] == DO) ||
										     ((curr_data_segment->recv_buffer)[i+1] == WILL) ) {
											int options_buffer_index = NoOfOptions*3;
											carry_OPT = 1;
											if ((curr_data_segment->recv_buffer)[i+1] == DO) {
												options_buffer[options_buffer_index++] = IAC;
												options_buffer[options_buffer_index] = WONT;
											}
											else {
												options_buffer[options_buffer_index++] = IAC;
                                                                                                options_buffer[options_buffer_index] = DONT;
											}
											++i;
										}
									}
									else {
										// i == curr_data_segment->recv_buffer_len - 1
										carry_DOWILL = 1;
									}
								}
							}
						}
						int no_of_option_bytes = NoOfOptions*3;
						if (send(tsd.client_socket_fd, options_buffer, no_of_option_bytes, 0) != no_of_option_bytes) {
							printf("Error seinding option responses (maintining default NVT).\n");
							exit(EXIT_FAILURE);
						}			
						if (carry_OPT) {
							options_buffer[0] = IAC;
							options_buffer[1] = options_buffer[no_of_option_bytes+1];
						}
						NoOfOptions = 0;
						if (last_byte_DM_cmd) {
							treat_all_data_as_urg = 0;
						}
						// delete data from input vector as it has been handled
		                                struct received_data *data0 = (struct received_data *)vector_read(tsd.input_data, 0);
                               			free(data0->recv_buffer);
						vector_remove(tsd.input_data, 0);	
					}
					else {
						// treat_all_data_as_urg == 1
						// curr_data_segment->type == 0
						// NoOfOptions == 0
						unsigned char DM_found0 = 0;
						for (int i=0; i<curr_data_segment->recv_buffer_len; ++i) {
							if ((i==0) && (carry_DOWILL || carry_OPT)) {
								if (carry_DOWILL) {
									if ((curr_data_segment->recv_buffer)[0] == IP) {
										printf("Received IAC IP (interrupt process) command. Client terminating.\n");
										exit(EXIT_SUCCESS);
									}
									else if ( (curr_data_segment->recv_buffer)[0] == DM) {
										// end special handling of input data
										treat_all_data_as_urg = 0;
										if ((curr_data_segment->recv_buffer_len) > 1) {
											unsigned char *new_recv_buff = (unsigned char *)malloc(curr_data_segment->recv_buffer_len - 1);
											if (new_recv_buff == NULL) {
												printf("Error allocating memory whilst switching to non special handling of data.\n");
												exit(EXIT_FAILURE);
											}
											memcpy(new_recv_buff, 
											       &((curr_data_segment->recv_buffer)[1]), 
											       curr_data_segment->recv_buffer_len - 1);
											free(curr_data_segment->recv_buffer);
											curr_data_segment->recv_buffer = new_recv_buff;
											curr_data_segment->recv_buffer_len = curr_data_segment->recv_buffer_len -1;
											carry_DOWILL = 0;
											DM_found0 = 1;
											break;
										}
										else {
											free(curr_data_segment->recv_buffer);
											vector_remove(tsd.input_data, 0);
											DM_found0 = 1;
										}
									}
									else if ( ((curr_data_segment->recv_buffer)[0]==DO) ||
										  ((curr_data_segment->recv_buffer)[0]==WILL) ) {
										if (curr_data_segment->recv_buffer_len > 1) {
											options_buffer[0] = IAC;
											if ((curr_data_segment->recv_buffer)[0]==DO) {
												options_buffer[1] = WONT;
											}
											else {
												options_buffer[1] = DONT;
											}
											options_buffer[2] = (curr_data_segment->recv_buffer)[1];
											carry_DOWILL = 0;
											++i;
											++NoOfOptions;
										}
										else {  // curr_data_segment->recv_buffer_len == 1
											options_buffer[0] == IAC;
											if ((curr_data_segment->recv_buffer)[0] == DO) {
												options_buffer[1] = WONT;
											}
											else {
												options_buffer[1] = DONT;
											}
											carry_DOWILL = 0;
											carry_OPT = 1;
											break;
										}
									}
								}
								else {
									options_buffer[2] = (curr_data_segment->recv_buffer)[0];
									carry_OPT = 0;
									++NoOfOptions;
								}	
							}
							else {
								if ((curr_data_segment->recv_buffer)[i] == IAC) {
									if ((i+1) < (curr_data_segment->recv_buffer_len)) {
										if ((curr_data_segment->recv_buffer)[i+1] == DM) {
											treat_all_data_as_urg = 0;
											DM_found0 = 1;
											if ((i+1) == ((curr_data_segment->recv_buffer_len)-1)) {
												struct received_data *data0 = (struct received_data *)vector_read(tsd.input_data,0);
												free(data0->recv_buffer);
												vector_remove(tsd.input_data, 0);
											}
											else {
												int new_buffer_len = curr_data_segment->recv_buffer_len - (i+2);
												unsigned char *new_buffer = (unsigned char *)malloc(new_buffer_len);
												if (new_buffer==NULL) {
													printf("Error allocating memory ewith malloc. Program terminating.\n");
													exit(EXIT_FAILURE);
												}
												memcpy(new_buffer, &((curr_data_segment->recv_buffer)[i+2]), new_buffer_len);
												curr_data_segment->recv_buffer = new_buffer;
												curr_data_segment->recv_buffer_len = new_buffer_len;
												break;	
											}	
										
										}
										else if ((curr_data_segment->recv_buffer)[i+1] == IP) {
											printf("Received IP command. Cleint terminating.\n");
											exit(EXIT_SUCCESS);
										}
										else if ( ((curr_data_segment->recv_buffer)[i+1] == DO) ||
											  ((curr_data_segment->recv_buffer)[i+1] == WILL) ) {
											if ((i+2) < (curr_data_segment->recv_buffer_len)) {
												int options_buffer_index = NoOfOptions*3;
												options_buffer[options_buffer_index++] = IAC;
												if ((curr_data_segment->recv_buffer)[i+1] == DO) {
													options_buffer[options_buffer_index++] = WONT;
												}			
												else {
													options_buffer[options_buffer_index++] = DONT;	
												}
												options_buffer[options_buffer_index] = (curr_data_segment->recv_buffer)[i+2];
												++NoOfOptions;
												i+=2;
											}
											else {
												int options_buffer_index = NoOfOptions*3;
												carry_OPT = 1;
												options_buffer[options_buffer_index++] = IAC;
												if ((curr_data_segment->recv_buffer)[i+1] == DO) {
                                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                                }
                                                                                                else {
                                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                                }
												++i;
											}
										}
									}
									else {
										carry_DOWILL = 1;
									}
										
								}
							}							
						}
						int no_of_option_bytes = NoOfOptions*3;
						if (send(tsd.client_socket_fd, options_buffer, no_of_option_bytes, 0)!= no_of_option_bytes) {
							printf("Error sending option response bytes. Program terminating.\n");
							exit(EXIT_FAILURE);
						}
						if (carry_OPT) {
							options_buffer[0] = IAC;
							options_buffer[1] = options_buffer[no_of_option_bytes+1];
						}
						if (!DM_found0)	 {
							// DELETE data 0 from tsd.input_data
							struct received_data *data0 = (struct received_data *)vector_read(tsd.input_data, 0);
							free(data0->recv_buffer);
							vector_remove(tsd.input_data, 0);
						}
						NoOfOptions = 0;
					}
				}
				else {
					// Last DM not in an urgent segment of received data
	                                // Only process the first piece of data and then continue the while loop
					// treat_all_data_as_urg == 0
					// NoOfOptions == 0
					struct received_data *first_data_segment = (struct received_data *)vector_read(tsd.input_data, 0);
					if ((first_data_segment->type) == 1) {
						treat_all_data_as_urg = 1;
						unsigned char last_byte_DM = 0;
						for (int i=0; i<(first_data_segment->recv_buffer_len); ++i) {
							if ( (i==0) && (carry_DOWILL || carry_OPT) ) {
								if (carry_DOWILL) {
									if ((first_data_segment->recv_buffer)[0] == IP) {
										printf("Client received IAC IP. Program terminating.\n");
										exit(EXIT_SUCCESS);
									}
									else if ( ((first_data_segment->recv_buffer)[0] == DO) ||
										  ((first_data_segment->recv_buffer)[0] == WILL) ) {
										if ((first_data_segment->recv_buffer_len)>1) {
											options_buffer[0] = IAC;
											if ((first_data_segment->recv_buffer)[0] == DO) {
												options_buffer[1] = WONT;				
											}	
											else {
												options_buffer[1] = DONT;	
											}
											options_buffer[2] = (first_data_segment->recv_buffer)[1];
											++NoOfOptions;
											++i;
											carry_DOWILL = 0;
										}
										else {
											carry_DOWILL = 0;
											carry_OPT = 1;
											options_buffer[0] = IAC;
											if ((first_data_segment->recv_buffer)[0] == DO) {
												options_buffer[1] = WONT;				
											}
											else {
												options_buffer[1] = DONT;
											}	
										}
									}
									else if ( ((first_data_segment->recv_buffer)[0] == DM) &&
								                  ((first_data_segment->recv_buffer_len)==1))	{
										last_byte_DM = 1;
									}
								}
								else {
									options_buffer[2] = (first_data_segment->recv_buffer)[0];
									carry_OPT = 0;
									++NoOfOptions;
								}
							}
							else {
								if ((first_data_segment->recv_buffer)[i] == IAC) {
									if ((i+1) < (first_data_segment->recv_buffer_len)) {
										if ((first_data_segment->recv_buffer)[i+1] == IP) {
											printf("Received IAC IP command. Program terminating.\n");
											exit(EXIT_FAILURE);
										}
										else if ( ((first_data_segment->recv_buffer)[i+1] == DO) ||
											  ((first_data_segment->recv_buffer)[i+1] == WILL) ) {
											if ((i+2) < first_data_segment->recv_buffer_len) {
												int options_buffer_index = NoOfOptions*3;
												options_buffer[options_buffer_index++] = IAC;
												if ((first_data_segment->recv_buffer)[i+1] == DO) {
													options_buffer[options_buffer_index++] = WONT;
												}			
												else {
													options_buffer[options_buffer_index++] = DONT;
												}
												options_buffer[options_buffer_index] = (first_data_segment->recv_buffer)[i+2];
												i+=2;
												++NoOfOptions;
											}
											else {
												carry_OPT = 1;
												int options_buffer_index = NoOfOptions*3;
											 	options_buffer[options_buffer_index++] = IAC;
												if ((first_data_segment->recv_buffer)[i+1] == DO) {
                                                                                                        options_buffer[options_buffer_index++] = WONT;
                                                                                                }
                                                                                                else {
                                                                                                        options_buffer[options_buffer_index++] = DONT;
                                                                                                }
												++i;
												break;
											}
										}		
										else if ( (first_data_segment->recv_buffer)[i+1] == DM) {
											if ((i+1) == ( first_data_segment->recv_buffer_len-1)) {
												last_byte_DM = 1;
												++i;
											}
										}
									}
									else {
										carry_DOWILL = 1;
									}
								}
							}
						}
						if (last_byte_DM == 1) {
							treat_all_data_as_urg = 0;
						}
						int no_of_option_bytes = NoOfOptions*3;
						if (send(tsd.client_socket_fd, options_buffer, no_of_option_bytes,0)!=no_of_option_bytes) {
							printf("Error occurred sending option responses to server. Program terminating.\n");
							exit(EXIT_FAILURE);
						}
						if (carry_OPT) {
							options_buffer[0] = IAC;
							options_buffer[1] = options_buffer[no_of_option_bytes+1];
						}
						NoOfOptions = 0;
						free(first_data_segment->recv_buffer);
						vector_remove(tsd.input_data, 0);
					}
					else {
						// first_data_segment->type == 0
						// NoOfOptions == 0
						// treat_all_data_as_urg == 0
						int recv_buffer_len = first_data_segment->recv_buffer_len;		
						unsigned char abort_output_received = 0;
                                                unsigned char ga_received = 0;
						unsigned char abort_output_index = -1;
						unsigned char special_chars[6] = {8,9,10,11,12,13};
						// filter out charatcers the nvt doesnt recognize
						for (int i=0; i<recv_buffer_len; ++i) {
							unsigned char valid_char = 0;
							for (int j=0; j<6; ++j) {
								if ((first_data_segment->recv_buffer)[i] == special_chars[j]) {
									valid_char = 1;
									if ((first_data_segment->recv_buffer)[i] == 13) {
										
									}
									break;
								}
							}
							if (((first_data_segment->recv_buffer)[i]>=32) && ((first_data_segment->recv_buffer)[i]<=126)) {
								valid_char = 1;
							}
							if ((i==0) && carry_DOWILL) {
								if ( ((first_data_segment->recv_buffer)[0] == DO) ||
								     ((first_data_segment->recv_buffer)[0] == WILL) ) {
									++i;
									continue;
								}
								else {
									continue;
								}
							}
							else if ( (i==0) && carry_OPT) {
								continue;
							}
							if ((first_data_segment->recv_buffer)[i] == IAC) {
								if (i<(recv_buffer_len-1)) {
									if ( ((first_data_segment->recv_buffer)[i+1] == DO) ||
									     ((first_data_segment->recv_buffer)[i+1] == WILL) ) {
										i+=2;
										continue;				
									}
									else {
										++i;
										continue;
									}
								}
								else {
									break;
								}
							}
							if (!valid_char) {
								// filoter out character
								for (int j=i; j<recv_buffer_len-1; ++j) {
									(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
								}
								--recv_buffer_len;
								--i;
							}
						}
						for (int i=0; i<recv_buffer_len; ++i) {
							if ( (i==0) && 
							     //(unurgent_not_specially_treated_data_processed==1) &&
							     (carry_DOWILL || carry_OPT) ) {
								if (carry_DOWILL) {
									if ((first_data_segment->recv_buffer)[0] == IP) {
										printf("Client reeived IAC IP from server. Program terminating.\n");
										exit(EXIT_SUCCESS);
									}
									else if ( (first_data_segment->recv_buffer)[0] == IAC) {
										carry_DOWILL = 0;
										for (int j=i; j<recv_buffer_len-1; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
										}
										--recv_buffer_len;
										--i;
									}
									else if ( (first_data_segment->recv_buffer)[0] == AO) {
										// Abort Output received.
										abort_output_received = 1;
										abort_output_index = 0;
										for (int j=0; j<recv_buffer_len-1; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
										}
										--recv_buffer_len;
										i=-1;
										carry_DOWILL = 0;
									}
									else if ( (first_data_segment->recv_buffer)[0] == EC) {
										// Erase Character received
										if (output_buffer_no_of_bytes == 0) {
											carry_DOWILL = 0;
											for (int j=0; j<recv_buffer_len-1; ++j) {
                                                                                        	(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
                                                                                	}	
											--recv_buffer_len;
											--i;	
										}
										else {
											carry_DOWILL = 0;
											(first_data_segment->recv_buffer)[0] = 8;
										}
									}
									else if ( (first_data_segment->recv_buffer)[0] == EL) {
										// Erase Line received
										if (output_buffer_no_of_bytes == 0) {
                                                                                        carry_DOWILL = 0;
                                                                                        for (int j=0; j<recv_buffer_len-1; ++j) {
                                                                                                (first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
                                                                                        }
                                                                                        --recv_buffer_len;
                                                                                        --i;
                                                                                }	
										else {
											unsigned char *new_recv_buffer = (unsigned char *)malloc(output_buffer_no_of_bytes
													                                         +recv_buffer_len-1);
											if (new_recv_buffer == NULL) {
												printf("Error allocating memory for new received buffer.\n");
												exit(EXIT_FAILURE);		
											}
											memset(new_recv_buffer, 8, output_buffer_no_of_bytes);
											memcpy(&(new_recv_buffer[output_buffer_no_of_bytes]),
											       &((first_data_segment->recv_buffer)[1]),
											       recv_buffer_len-1);
											free(first_data_segment->recv_buffer);
											first_data_segment->recv_buffer = new_recv_buffer;
											first_data_segment->recv_buffer_len += (output_buffer_no_of_bytes-1);
											--i;
											carry_DOWILL = 0;
										}
									}
									else if ((first_data_segment->recv_buffer)[0] = GA) {
										// GA command received as first byte.
										ga_received = 1;
										for (int j=0; j<recv_buffer_len-1; ++j) {
                                                                                        (first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
                                                                                }
                                                                                --recv_buffer_len;
                                                                                i=-1;
                                                                                carry_DOWILL = 0;
									}
									else if ( ((first_data_segment->recv_buffer)[0] == DO) ||
										  ((first_data_segment->recv_buffer)[0] == WILL) ) {
										if (first_data_segment->recv_buffer_len>1) {
											options_buffer[0] = IAC;
											if ((first_data_segment->recv_buffer)[0] == DO) {
												options_buffer[1] = WONT;
											}
											else {
												options_buffer[1] = DONT;
											}
											options_buffer[2] = (first_data_segment->recv_buffer)[1];
											++NoOfOptions;
											for (int j=0; j<recv_buffer_len-2; ++j) {
                                                                                        	(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+2];
                                                                                	}
                                                                                	recv_buffer_len -= 2;
                                                                                	i=-1;
                                                                               	 	carry_DOWILL = 0;
										}	
										else {
											carry_OPT = 1;
											carry_DOWILL = 0;
											options_buffer[0] = IAC;
											if ((first_data_segment->recv_buffer)[0] == DO) {
                                                                                                options_buffer[1] = WONT;
                                                                                        }
                                                                                        else {
                                                                                                options_buffer[1] = DONT;
                                                                                        }
											--recv_buffer_len;												
										}	
									}
									else  {
										carry_DOWILL = 0;
										for (int j=0; j<recv_buffer_len-1; ++j) {
                                                                                        (first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
                                                                                }
                                                                                --recv_buffer_len;
                                                                                i=-1;
									}
								}
								else {
									options_buffer[2] = (first_data_segment->recv_buffer)[0];
									++NoOfOptions;
									for (int j=0; j<recv_buffer_len-1; ++j) {
                                                                               (first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+1];
                                                                        }
                                                                        --recv_buffer_len;
                                                                        i=-1;
									carry_OPT = 0;
								}
							}
							else if ((first_data_segment->recv_buffer)[i] == IAC) {
								if (i==(recv_buffer_len - 1)) {
									carry_DOWILL = 1;
									--recv_buffer_len;	
								}
								else {
									if ((first_data_segment->recv_buffer)[i+1] == IP) {
										printf("Received Interrupt Process command from server. Client terminating.\n");
										exit(EXIT_SUCCESS);			
									}	
									else if ( (first_data_segment->recv_buffer)[i+1] == AO) {
										if (!abort_output_received) {
											abort_output_received = 1;
											abort_output_index = i;
										}
										for (int j=i; j<recv_buffer_len-2; ++j) {
											(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+2];
										}
										recv_buffer_len -= 2;
										--i;
									}
									else if ( (first_data_segment->recv_buffer)[i+1] == IAC) {
										for (int j=i; j<recv_buffer_len-2; ++j) {
                                                                                        (first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+2];
                                                                                }
                                                                                recv_buffer_len -= 2;
                                                                                --i;
									}
									else if ( (first_data_segment->recv_buffer)[i+1] == EC) {
										int print_position_starting_index = get_previous_print_position(first_data_segment->recv_buffer,
												                                                i+1);
																		
										if (print_position_starting_index == -1) {
											// previous print position either lies in output_buffer or before output_buffer,
											// if it lies before output buffer we output backspaces until we reach the first character
											// after the newline in output_buffer
											unsigned char *previous_and_current_output = (unsigned char *)malloc(output_buffer_no_of_bytes+i+2);
											if (previous_and_current_output == NULL) {
												printf("Error allocating memory with malloc. Program terminating.\n");
												exit(EXIT_FAILURE);
											}
											memcpy(previous_and_current_output, output_buffer,output_buffer_no_of_bytes);
											memcpy(&(previous_and_current_output[output_buffer_no_of_bytes]),
											       first_data_segment->recv_buffer, i+2);
											int EC_index = output_buffer_no_of_bytes+i+1;
											int no_of_backspaces = -1;
											print_position_starting_index = get_previous_print_position(previous_and_current_output, EC_index);
											if (print_position_starting_index == -1) {
												// last print position lises before  newline in output_buffer
												// print backspaces to get back to character after output_buffer newline
												if (output_buffer_no_of_bytes < 2) {
													no_of_backspaces = get_no_of_bs_from_curr_output(first_data_segment->recv_buffer,
															               i+1);
												}
												else {		
													no_of_backspaces = get_no_of_bs_to_output(previous_and_current_output,
														       			 	  EC_index, 
																		  1);	
												}
											}
											else {
												no_of_backspaces = get_no_of_bs_to_output(previous_and_current_output, EC_index,
																	  print_position_starting_index);		
											}
											unsigned char *new_recv_buffer = (unsigned char *)malloc(recv_buffer_len + no_of_backspaces -2);
											if (new_recv_buffer == NULL) {
												printf("Error allocating memory to insert backspaces after IAC EC.\n");
												exit(EXIT_FAILURE);
											}	
											memcpy(new_recv_buffer, first_data_segment->recv_buffer, i);
											memset(&(new_recv_buffer[i]), 8, no_of_backspaces);
											memcpy(&(new_recv_buffer[i+no_of_backspaces]), 
											       &((first_data_segment->recv_buffer)[i+2]), recv_buffer_len - (i+2));
											free(first_data_segment->recv_buffer);
											first_data_segment->recv_buffer = new_recv_buffer;
											first_data_segment->recv_buffer_len = recv_buffer_len - 2 + no_of_backspaces;
											recv_buffer_len = first_data_segment->recv_buffer_len;
											--i;	   	
										}
										else {
											int gap = i+2 - print_position_starting_index;
											for (int j=print_position_starting_index; j<recv_buffer_len-gap; ++j) {
												(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[j+gap];
											}
											recv_buffer_len -= gap;
											--i;
										}
										
									}
									else if ( (first_data_segment->recv_buffer)[i+1] == EL) {
										int sol_index = get_start_of_current_line_index(first_data_segment->recv_buffer, i+1);
										if (sol_index < 0 ) {
											int no_of_backspaces = -1;
											if (output_buffer[0] == '\n') {
												unsigned char *temp_buffer = (unsigned char *)malloc(output_buffer_no_of_bytes +
																		     i+2);
												if (temp_buffer == NULL) {
													printf("Error allocating buffer for previous line and currently received data.\n");
													exit(EXIT_FAILURE);
												}			
												memcpy(temp_buffer, output_buffer, output_buffer_no_of_bytes);
												memcpy(&(temp_buffer[output_buffer_no_of_bytes]), 
												       first_data_segment->recv_buffer,
												       i+2);
												int EL_index = output_buffer_no_of_bytes + (i+1);
												no_of_backspaces = get_no_of_bs_to_output(temp_buffer,
                                                                                                                                          EL_index,
                                                                                        						  1);
												free(temp_buffer);
												unsigned char *new_recv_buffer = (unsigned char *)malloc(first_data_segment->recv_buffer_len - 2 + no_of_backspaces);
												memcpy(new_recv_buffer, first_data_segment->recv_buffer, i);
												memset(&(new_recv_buffer[i]), 8, no_of_backspaces);
												memcpy(&(new_recv_buffer[i+8]), 
												       &((first_data_segment->recv_buffer)[i+2]),
												       recv_buffer_len -(i+2));
												free(first_data_segment->recv_buffer);
											       	first_data_segment->recv_buffer = new_recv_buffer;
												first_data_segment->recv_buffer_len = recv_buffer_len - 2 + no_of_backspaces;	
												recv_buffer_len = first_data_segment->recv_buffer_len;
												--i;
											}
											else {
												for (int j=0; j<recv_buffer_len-(i+2); ++j) {
													(first_data_segment->recv_buffer)[j] = (first_data_segment->recv_buffer)[i+2+j];
												}
												first_data_segment->recv_buffer_len = recv_buffer_len - (i+2);
												recv_buffer_len -= (i+2);
												i=-1;
											}
										}
										else {
											for (int j=0; j<recv_buffer_len-(i+2); ++j) {
												(first_data_segment->recv_buffer)[sol_index+j] = (first_data_segment->recv_buffer)[i+2+j];
											}	
											first_data_segment->recv_buffer_len -= (i+1 - sol_index) + 1;
											recv_buffer_len = first_data_segment->recv_buffer_len;
											i = sol_index - 1;
										}
									}
									else if ( ((first_data_segment->recv_buffer)[i+1] == DO) ||
										  ((first_data_segment->recv_buffer)[i+1] == WILL) ) {
										if ((i+1)==(recv_buffer_len-1)) {
											int options_buffer_index = NoOfOptions*3;
											if ((first_data_segment->recv_buffer)[i+1] == DO) {
												options_buffer[options_buffer_index++] = IAC;
												options_buffer[options_buffer_index++] = WONT;		
											}
											else {
												options_buffer[options_buffer_index++] = IAC;
                                                                                                options_buffer[options_buffer_index++] = DONT;
											}
											carry_OPT = 1;
											recv_buffer_len -= 2;
										}
										else {
											int options_buffer_index = NoOfOptions*3;
                                                                                        if ((first_data_segment->recv_buffer)[i+1] == DO) {
                                                                                                options_buffer[options_buffer_index++] = IAC;
                                                                                                options_buffer[options_buffer_index++] = WONT;
                                                                                        }
                                                                                        else {
                                                                                                options_buffer[options_buffer_index++] = IAC;
                                                                                                options_buffer[options_buffer_index++] = DONT;
                                                                                        }
											options_buffer[options_buffer_index] = (first_data_segment->recv_buffer)[i+2];
											for (int j=i+3; j<recv_buffer_len; ++j) {
												(first_data_segment->recv_buffer)[j-3] =(first_data_segment->recv_buffer)[j];
											}
											recv_buffer_len -= 3;
											--i;
											first_data_segment->recv_buffer_len = recv_buffer_len;
											++NoOfOptions;
										}
									}
									else if ( (first_data_segment->recv_buffer)[i+1] == GA) {
										for (int j=i+2; j<recv_buffer_len; ++j) {
											(first_data_segment->recv_buffer)[j-2] =(first_data_segment->recv_buffer)[j];
										}
										recv_buffer_len -= 2;
										--i;
										first_data_segment->recv_buffer_len = recv_buffer_len;
									}
									else {
										// command unsupported or has no effect
										for (int j=i+2; j<recv_buffer_len; ++j) {
                                                                                        (first_data_segment->recv_buffer)[j-2] =(first_data_segment->recv_buffer)[j];
                                                                                }
                                                                                recv_buffer_len -= 2;
                                                                                --i;
                                                                                first_data_segment->recv_buffer_len = recv_buffer_len;
									}
								}
							}	
						}
						first_data_segment->recv_buffer_len = recv_buffer_len;
						int no_of_option_bytes_to_send = NoOfOptions*3;
						if (send(tsd.client_socket_fd, 
							 options_buffer, 
							 no_of_option_bytes_to_send, 0) != no_of_option_bytes_to_send) {
							printf("Error sending option responses.\n");
							exit(EXIT_FAILURE);
						}
						if (carry_OPT) {
							output_buffer[0] = IAC;
							output_buffer[1] = output_buffer[no_of_option_bytes_to_send+1];
						}			
						NoOfOptions = 0;
						if (abort_output_received) {
							unsigned char newline_present = 0;
							int newline_index = -1;
							for (int i=abort_output_index-1; i>=0; --i) {
								if ((first_data_segment->recv_buffer)[i] == '\n') {
									newline_present = 1;
									newline_index = i;
									break;
								}
							}
							if (newline_present) {
								output_buffer_no_of_bytes = abort_output_index - newline_index;
								memcpy(output_buffer,
								       &((first_data_segment->recv_buffer)[newline_index]),
								       output_buffer_no_of_bytes);
							}
							else {
								memcpy(&(output_buffer[output_buffer_no_of_bytes]),
								       first_data_segment->recv_buffer,
								       recv_buffer_len);
								output_buffer_no_of_bytes += recv_buffer_len;
							}
							char *password_str = "Password: ";
							int passwd_str_len = strlen(password_str);
							for (int i=0; i<abort_output_index-(passwd_str_len-1); ++i) {
								if (strncmp(&((first_data_segment->recv_buffer)[i]),
									    password_str,
									    passwd_str_len)==0) {
									password_prompt_from_server = 1;
									// turn local echo off
									struct termios curr_termattr;
									if (tcgetattr(0, &curr_termattr)==-1) {
										char *terminal_error1 = "Error getting current terminal attributes.\n";
										write(1, terminal_error1, strlen(terminal_error1));
										exit(EXIT_FAILURE);
									}
									curr_termattr.c_lflag &= (~ECHO);
									if (tcsetattr(0, TCSANOW, &curr_termattr)==-1) {
										char *terminal_error2 = "Error setting terminal attributes.\n";
										write(1, terminal_error2, strlen(terminal_error2));
										exit(EXIT_FAILURE);
									}
									break;
								}
							}

							write(1, first_data_segment->recv_buffer, abort_output_index);
                                       		}
						else {
							unsigned char newline_present = 0;
							int newline_index = -1;
							for (int i=recv_buffer_len-1; i>=0; --i) {
								if ( (first_data_segment->recv_buffer)[i] == '\n') {
									newline_present = 1;
									newline_index = i;
									break;
								}
							}
							if (newline_present) {
								memcpy(output_buffer, 
								       &((first_data_segment->recv_buffer)[newline_index]),
								       recv_buffer_len-newline_index);
								output_buffer_no_of_bytes = recv_buffer_len-newline_index;	
							}
							else {
								memcpy(&(output_buffer[output_buffer_no_of_bytes]),
									first_data_segment->recv_buffer,
									recv_buffer_len);
								output_buffer_no_of_bytes += recv_buffer_len;
							}
							char *password_str = "Password: ";
							int passwd_str_len = strlen(password_str);
							for (int i=0; i<recv_buffer_len-(passwd_str_len-1); ++i) {
								if (strncmp(&((first_data_segment->recv_buffer)[i]),
									    password_str,
									    passwd_str_len)==0) {
									password_prompt_from_server = 1;
									// turn local echo off
									struct termios curr_termattr;
									if (tcgetattr(0, &curr_termattr)==-1) {
										char *terminal_error1 = "Error getting current terminal attributes.\n";
										write(1, terminal_error1, strlen(terminal_error1));
										exit(EXIT_FAILURE);
									}
									curr_termattr.c_lflag &= (~ECHO);
									if (tcsetattr(0, TCSANOW, &curr_termattr)==-1) {
										char *terminal_error2 = "Error setting terminal attributes.\n";
										write(1, terminal_error2, strlen(terminal_error2));
										exit(EXIT_FAILURE);
									}
									break;
								}
							}
							write(1, first_data_segment->recv_buffer, recv_buffer_len); // write received (processed) data to terminal for user to view	
						}	
						free(first_data_segment->recv_buffer);
						vector_remove(tsd.input_data, 0);
					}	
				}
			}
		}
	}
}	

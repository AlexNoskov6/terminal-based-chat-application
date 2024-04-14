/**
 * @app.c
 * @author  Alex Noskov
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 *
 */

#define _POSIX_C_SOURCE 200112L
#define _POSIX_C_SOURCE 200809L
#define BACKLOG 5
#define STDIN 0
#define TRUE 1
#define CMD_SIZE 100
#define BUFFER_SIZE 1024
#define MSG_SIZE 256
#define MSG_NUMBER 10

#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "../include/global.h"
#include "../include/logger.h"


/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */

int server(char *port);
int client(char *port);
int connect_to_host(char *server_ip, char* server_port, char *local_port);
int ports_comp(void *a, void *b);

struct client_ent
	{
		int idx;
		int active_clients;
		int socket_d;
		char hostn[50];
		char ip[INET_ADDRSTRLEN];
		int port;
		int msg_sent;
		int msg_rcvd;
		int msg_buffered;
		int blocked;
		char *status;
		char msg_buf[MSG_NUMBER][MSG_SIZE];
		char blocked_ips[5][INET_ADDRSTRLEN];
	};



int main(int argc, char **argv)
{
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));
	

	if (argc != 3) {
		printf("Usage:%s [port]\n", argv[0]);
		exit(-1);
	}

	if (strcmp(argv[1], "s") == 0) {
		server(argv[2]);
	}

	if (strcmp(argv[1], "c") == 0) {
		client(argv[2]);
	}

	return 0;
}

int server(char* port) {
	int entry_idx = 0;
	struct client_ent client_list[5];
	int server_socket, head_socket, selret, sock_index, fdaccept=0, caddr_len;
	struct sockaddr_in client_addr;
	struct addrinfo hints, *res;
	fd_set master_list, watch_list;

	/* Set up hints structure */
	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/* Fill up address structures */
	if (getaddrinfo(NULL, port, &hints, &res) != 0) {
		perror("getaddrinfo failed");
	}
	/* Socket */
	server_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(server_socket < 0) {
		perror("Cannot create socket");
	}
	/* Bind */
	if(bind(server_socket, res->ai_addr, res->ai_addrlen) < 0 ) {
		perror("Bind failed");
	}
	freeaddrinfo(res);
	
	/* Listen */
	if(listen(server_socket, BACKLOG) < 0) {
		perror("Unable to listen on port");
	}
	/* ---------------------------------------------------------------------------- */
	
	/* Zero select FD sets */
	FD_ZERO(&master_list);
	FD_ZERO(&watch_list);
	
	/* Register the listening socket */
	FD_SET(server_socket, &master_list);
	/* Register STDIN */
	FD_SET(STDIN, &master_list);
	
	head_socket = server_socket;
	
	while(TRUE){
		memcpy(&watch_list, &master_list, sizeof(master_list));
		
		printf("\n[Server@CSE489]$ ");
		fflush(stdout);
		
		/* select() system call. This will BLOCK */
		selret = select(head_socket + 1, &watch_list, NULL, NULL, NULL);
		if(selret < 0) {
			perror("select failed.");
		}

		/* Check if we have sockets/STDIN to process */
		if(selret > 0){
			/* Loop through socket descriptors to check which ones are ready */
			for(sock_index=0; sock_index<=head_socket; sock_index+=1){
				
				if(FD_ISSET(sock_index, &watch_list)){
					
					/* Check if new command on STDIN */
					if (sock_index == STDIN){
						char *cmd = (char*) malloc(sizeof(char)*CMD_SIZE);
						
						memset(cmd, '\0', CMD_SIZE);
						if(fgets(cmd, CMD_SIZE-1, stdin) == NULL) { 
							exit(-1);
						}

						if (strncmp(cmd, "PORcse4589T", 4) == 0) {
							printf("[%.*s:SUCCESS]\n", 4, cmd);
							printf("PORT:%s\n", port);
							printf("[%.*s:END]\n", 4, cmd);
						}

						if (strncmp(cmd, "LIST", 4) == 0) {

								struct client_ent copy[5];
								memcpy(copy, client_list, sizeof(struct client_ent) * 5);
								qsort(copy, entry_idx, sizeof(struct client_ent), ports_comp);
								int minus_val = 0;

								printf("[%.*s:SUCCESS]\n", 4, cmd);
								for (int i = 1; i <= entry_idx; i++) {
									if (strncmp(client_list[i-1].status, "logged-in", 9) == 0){
										printf("%-5d%-35s%-20s%-8d\n", i-minus_val, copy[i-1].hostn, copy[i-1].ip, copy[i-1].port);
									} else {
										minus_val++;
									}
								}
								printf("[%.*s:END]\n", 4, cmd);
						}

						if (strncmp(cmd, "BLOCKED", 7) == 0) {

							char ip[INET_ADDRSTRLEN];
							memset(ip, '\0', INET_ADDRSTRLEN);
							int count = 0;
							int idx_in_copy = 0;

							char *ptr = cmd + 8;
							char c = *ptr;
							while (c != '\n') {
								ptr++;
								count++;
								c = *ptr;
							}
							strncpy(ip, cmd + 8, count);

							struct client_ent copy[5];

							for (int i = 0; i < entry_idx; i++) {
								if (strcmp(client_list[i].ip, ip) == 0) {
									for (int j = 0; j < client_list[i].blocked; j++) {
										for (int k = 0; k < entry_idx; k++) {
											if (strcmp(client_list[i].blocked_ips[j], client_list[k].ip) == 0) {
												memcpy(&copy[idx_in_copy], &client_list[k], sizeof(struct client_ent));
												idx_in_copy++;
											}
										}
									}
								}
							}

							qsort(copy, idx_in_copy, sizeof(struct client_ent), ports_comp);

							printf("[%.*s:SUCCESS]\n", 7, cmd);
							for (int i = 1; i <= idx_in_copy; i++) {
								printf("%-5d%-35s%-20s%-8d\n", i, copy[i-1].hostn, copy[i-1].ip, copy[i-1].port);
							}
							printf("[%.*s:END]\n", 7, cmd);
						}

						if (strncmp(cmd, "STATISTICS", 10) == 0) {

								struct client_ent copy[5];
								memcpy(copy, client_list, sizeof(struct client_ent) * 5);
								qsort(copy, entry_idx, sizeof(struct client_ent), ports_comp);

								printf("[%.*s:SUCCESS]\n", 10, cmd);
								for (int i = 1; i <= entry_idx; i++) {
									printf("%-5d%-35s%-8d%-8d%-8s\n", i, copy[i-1].hostn, copy[i-1].msg_sent, copy[i-1].msg_rcvd, copy[i-1].status);
								}
								printf("[%.*s:END]\n", 10, cmd);
						}

						if (strncmp(cmd, "IP", 2) == 0) {
							int fdsocket;
							struct addrinfo hints, *res;
							struct sockaddr_in addr;
							socklen_t addrlen = sizeof(addr);

							memset(&hints, 0, sizeof(hints));
							hints.ai_family = AF_INET;
							hints.ai_socktype = SOCK_DGRAM;

							if (getaddrinfo("8.8.8.8", "53", &hints, &res) != 0) {
								perror("getaddrinfo failed");
							}

							fdsocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
							if(fdsocket < 0) {
								perror("Failed to create socket");
							}

							if(connect(fdsocket, res->ai_addr, res->ai_addrlen) < 0) {
								perror("Connect failed");
							}

							if(getsockname(fdsocket, (struct sockaddr *)&addr, &addrlen) < 0) {
								perror("Getsockname failed");
							}

							freeaddrinfo(res);

							char *ip_addr = inet_ntoa(addr.sin_addr);

							printf("[%.*s:SUCCESS]\n", 2, cmd);
							printf("IP:%s\n", ip_addr);
							printf("[%.*s:END]\n", 2, cmd);
						}
						
						free(cmd);
					}
					/* Check if new client is requesting connection */
					else if(sock_index == server_socket){
						caddr_len = sizeof(client_addr);
						fdaccept = accept(server_socket, (struct sockaddr *)&client_addr, &caddr_len);
						if(fdaccept < 0)
							perror("Accept failed.");
						
						printf("\nRemote Host connected!\n");                        
						
						/* Add to watched socket list */
						FD_SET(fdaccept, &master_list);
						if(fdaccept > head_socket) head_socket = fdaccept;

						/*----- Populating a new struct -------*/

						char *port_buff = (char *) malloc(sizeof(char) * 10);
						memset(port_buff, '\0', 10);
						recv(fdaccept, port_buff, 8, 0);
						int port_to_store = atoi(port_buff);
						free(port_buff);

						struct sockaddr_storage info;
						socklen_t len = sizeof(info);
						char serv_name[30];
						getpeername(fdaccept, (struct sockaddr *)&info, &len);
						struct sockaddr_in *info_ptr = (struct sockaddr_in *)&info;

						int found = 0;
						int client_int = -1;
						
						for (int i = 0; i < entry_idx; i++) {
							if (client_list[i].port == port_to_store && (strncmp(client_list[i].ip, inet_ntoa(info_ptr->sin_addr), INET_ADDRSTRLEN) == 0)) {
								client_list[i].socket_d = fdaccept;
								client_list[i].status = "logged-in";
								found++;
								for (int j = 0; j < client_list[i].msg_buffered; j++) {
									char from_ip[INET_ADDRSTRLEN];
									char msg_recv[MSG_SIZE];
									memset(msg_recv, '\0', MSG_SIZE);
									strncpy(from_ip, client_list[i].msg_buf[j], INET_ADDRSTRLEN);
									char *ptr_msg = &client_list[i].msg_buf[j];
									ptr_msg += INET_ADDRSTRLEN;
									strcpy(msg_recv, ptr_msg);

									printf("\n[RELAYED:SUCCESS]\n");
									printf("msg from:%s, to:%s\n[msg]:%s\n", from_ip, client_list[i].ip, msg_recv);
									printf("[RELAYED:END]\n");
								}
								client_int = i;
								break;
							} 
						}
						if (found == 0) {
							entry_idx++;
							struct client_ent new_client;
							getnameinfo((struct sockaddr *)info_ptr, len, new_client.hostn, sizeof(new_client.hostn), serv_name, sizeof(serv_name), 0);
							strcpy(new_client.ip, inet_ntoa(info_ptr->sin_addr));
							new_client.idx = entry_idx;
							new_client.port = port_to_store;
							new_client.socket_d = fdaccept;
							new_client.msg_sent = 0;
							new_client.msg_rcvd = 0;
							new_client.msg_buffered = 0;
							new_client.blocked = 0;
							new_client.status = "logged-in";
							client_list[entry_idx - 1] = new_client;
						}

						/*------ Sending the array information ------*/

						for (int i = 0; i < entry_idx; i++) {
							client_list[i].active_clients = entry_idx;
						}
						if (send(fdaccept, client_list, sizeof(struct client_ent) * 5, 0) < 0) {
        					perror("Send failed");
        					exit(-1);
    					}

						if (client_int != -1) {
							client_list[client_int].msg_buffered = 0;
						}

					}
					/* Read from existing clients */
					else{
						/* Initialize buffer to receieve response */
						char *buffer = (char*) malloc(sizeof(char)*BUFFER_SIZE);
						memset(buffer, '\0', BUFFER_SIZE);
						
						if(recv(sock_index, buffer, BUFFER_SIZE, 0) <= 0){

							for (int i = 0; i < entry_idx; i++) {
								if (client_list[i].socket_d == sock_index) {

									if (strncmp(client_list[i].status, "exit", 4) == 0) {
										int move = (entry_idx - 1) - i;
										for (int j = i; j <= move; j++) {
											client_list[j] = client_list[j+1];
											client_list[j].idx = client_list[j].idx - 1;
										}
										entry_idx--;

										printf("Remote Host terminated connection!\n");
									}

									close(sock_index);
									/* Remove from watched list */
									FD_CLR(sock_index, &master_list);
								}
							}

							
						}
						else {
							/*-------- Sending updated list of clients -------*/
							if (strncmp(buffer, "REFRESH", 7) == 0) {
								for (int i = 0; i < entry_idx; i++) {
									client_list[i].active_clients = entry_idx;
								}
								if (send(sock_index, client_list, sizeof(struct client_ent) * 5, 0) < 0) {
        							perror("Send failed");
        							exit(-1);
    							}
							}

							if (strncmp(buffer, "EXIT", 4) == 0) {
								for (int i = 0; i < entry_idx; i++) {
									if (client_list[i].socket_d == sock_index) {
										client_list[i].status = "exit";
									}
								}
							}

							if (strncmp(buffer, "LOGOUT", 6) == 0) {
								for (int i = 0; i < entry_idx; i++) {
									if (client_list[i].socket_d == sock_index) {
										client_list[i].status = "logged-out";
									}
								}
							}

							if (strncmp(buffer, "BLOCK", 5) == 0) {
								char to_ip[INET_ADDRSTRLEN];
								memset(to_ip, '\0', INET_ADDRSTRLEN);
								int count = 0;

								char *ptr = buffer + 6;
								char c = *ptr;
								while (c != ' ') {
									ptr++;
									count++;
									c = *ptr;
								}
								strncpy(to_ip, buffer + 6, count);

								for (int i = 0; i < entry_idx; i++) {
									if (client_list[i].socket_d == sock_index) {
										memset(client_list[i].blocked_ips[client_list[i].blocked], '\0', INET_ADDRSTRLEN);
										strcpy(client_list[i].blocked_ips[client_list[i].blocked], to_ip);
										client_list[i].blocked++;
									}
								}

							}

							if (strncmp(buffer, "UNBLOCK", 7) == 0) {
								char to_ip[INET_ADDRSTRLEN];
								memset(to_ip, '\0', INET_ADDRSTRLEN);
								int count = 0;
								int removed = 0;

								char *ptr = buffer + 8;
								char c = *ptr;
								while (c != ' ') {
									ptr++;
									count++;
									c = *ptr;
								}
								strncpy(to_ip, buffer + 8, count);

								for (int i = 0; i < entry_idx; i++) {
									if (client_list[i].socket_d == sock_index) {
										for (int j = 0; j < client_list[i].blocked; j++) {
											if (strcmp(client_list[i].blocked_ips[j], to_ip) == 0) {
												memset(client_list[i].blocked_ips[j], '\0', INET_ADDRSTRLEN);
												removed++;
											}
										}
										client_list[i].blocked -= removed;
									}
								}

							}

							if (strncmp(buffer, "SEND", 4) == 0) {
								char to_ip[INET_ADDRSTRLEN];
								memset(to_ip, '\0', INET_ADDRSTRLEN);
								char from_ip[INET_ADDRSTRLEN];
								memset(from_ip, '\0', INET_ADDRSTRLEN);
								int count = 0;
								int msg_len = 0;
								int msg_idx = -1;
								int client_idx = 0;
								int blocked = 0;

								for (int i = 0; i < entry_idx; i++) {
									if (client_list[i].socket_d == sock_index) {
										strcpy(from_ip, client_list[i].ip);
									}
								}
								
								char *ptr = buffer + 5;
								char c = *ptr;
								while (c != ' ') {
									ptr++;
									count++;
									c = *ptr;
								}
								strncpy(to_ip, buffer + 5, count);

								for (int i = 0; i < entry_idx; i++) {
									if (strcmp(client_list[i].ip, to_ip) == 0) {
										for (int j = 0; j < client_list[i].blocked; j++) {
											if (strcmp(client_list[i].blocked_ips[j], from_ip) == 0) {
												blocked++;
											}
										}
									}	
								}

								if (blocked == 0) {
									for (int i = 0; i < entry_idx; i++) {
										if (strcmp(client_list[i].ip, to_ip) == 0) {
											client_list[i].msg_buffered++;
											client_list[i].msg_rcvd++;
											msg_idx = client_list[i].msg_buffered - 1;
											client_idx = i;
											break;
										}
									}
								}

								if (blocked != 0) {
									for (int i = 0; i < entry_idx; i++) {
										if (strcmp(client_list[i].ip, from_ip) == 0) {
											client_list[i].msg_sent++;
										}
									}
								}

								ptr++;
								if (msg_idx != -1) {
									strncpy((client_list[client_idx].msg_buf[msg_idx]), from_ip, INET_ADDRSTRLEN);
									client_list[client_idx].msg_buf[msg_idx][INET_ADDRSTRLEN - 1] = '\0';
									strcpy(&(client_list[client_idx].msg_buf[msg_idx][INET_ADDRSTRLEN]), ptr);
									if (strcmp(client_list[client_idx].status, "logged-in") == 0) {
										char to_send[MSG_SIZE];
										memset(to_send, '\0', MSG_SIZE);
										memcpy(to_send, client_list[client_idx].msg_buf[msg_idx], MSG_SIZE);
										send(client_list[client_idx].socket_d, to_send, MSG_SIZE, 0);
										printf("[RELAYED:SUCCESS]\n");
										printf("msg from:%s, to:%s\n[msg]:%s\n", from_ip, to_ip, &client_list[client_idx].msg_buf[msg_idx][INET_ADDRSTRLEN]);
										printf("[RELAYED:END]\n");
										memset(client_list[client_idx].msg_buf[msg_idx], '\0', MSG_SIZE);
										client_list[client_idx].msg_buffered--;
									}

									for (int i = 0; i < entry_idx; i++) {
										if (strcmp(client_list[i].ip, from_ip) == 0) {
											client_list[i].msg_sent++;
										}
									}
								}


							}

							if (strncmp(buffer, "BROADCAST", 9) == 0) {
								char to_ip[INET_ADDRSTRLEN];
								memset(to_ip, '\0', INET_ADDRSTRLEN);
								memcpy(to_ip, "255.255.255.255", INET_ADDRSTRLEN);
								char from_ip[INET_ADDRSTRLEN];
								memset(from_ip, '\0', INET_ADDRSTRLEN);
								int count = 0;
								int msg_idx = -1;

								for (int i = 0; i < entry_idx; i++) {
									if (client_list[i].socket_d == sock_index) {
										strcpy(from_ip, client_list[i].ip);
									}
								}
								
								char *ptr = buffer + 10;

								for (int i = 0; i < entry_idx; i++) {
									if ((strcmp(client_list[i].status, "logged-in") == 0) && (strcmp(client_list[i].ip, from_ip) != 0)) {
										int blocked = 0;
										for (int j = 0; j < client_list[i].blocked; j++) {
											if (strcmp(client_list[i].blocked_ips[j], from_ip) == 0) {
												blocked++;
											}
										}
										if (blocked == 0) {
											client_list[i].msg_rcvd++;
											msg_idx++;
										}
									}
								}

								if (msg_idx != -1) {
									for (int i = 0; i < entry_idx; i++) {
										if ((strcmp(client_list[i].status, "logged-in") == 0) && (strcmp(client_list[i].ip, from_ip) != 0)) {

											int blocked = 0;
											for (int j = 0; j < client_list[i].blocked; j++) {
												if (strcmp(client_list[i].blocked_ips[j], from_ip) == 0) {
													blocked++;
												}
											}

											if (blocked == 0) {
												strncpy((client_list[i].msg_buf[client_list[i].msg_buffered]), from_ip, INET_ADDRSTRLEN);
												client_list[i].msg_buf[client_list[i].msg_buffered][INET_ADDRSTRLEN - 1] = '\0';
												strcpy(&(client_list[i].msg_buf[client_list[i].msg_buffered][INET_ADDRSTRLEN]), ptr);
												char to_send[MSG_SIZE];
												memset(to_send, '\0', MSG_SIZE);
												memcpy(to_send, client_list[i].msg_buf[client_list[i].msg_buffered], MSG_SIZE);
												send(client_list[i].socket_d, to_send, MSG_SIZE, 0);
												printf("[RELAYED:SUCCESS]\n");
												printf("msg from:%s, to:%s\n[msg]:%s\n", from_ip, to_ip, &client_list[i].msg_buf[client_list[i].msg_buffered][INET_ADDRSTRLEN]);
												printf("[RELAYED:END]\n");
												memset(client_list[i].msg_buf[client_list[i].msg_buffered], '\0', MSG_SIZE);
											}

										}
									}
									for (int i = 0; i < entry_idx; i++) {
										if (strcmp(client_list[i].ip, from_ip) == 0) {
											client_list[i].msg_sent++;
										}
									}
								}

							}
						}
						
						free(buffer);
					}
				}
			}
		}
	}
	return 0;
}







int client(char *port) {

	struct client_ent client_lst[5];
	int active_socket = -1;
	void *buff = calloc(5, sizeof(struct client_ent));
	char my_ip[INET_ADDRSTRLEN];
	memset(my_ip, '\0', INET_ADDRSTRLEN);

	if (my_ip[0] == '\0') {
		int fdsocket;
		struct addrinfo hints, *res;
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;

		if (getaddrinfo("8.8.8.8", "53", &hints, &res) != 0) {
			perror("getaddrinfo failed");
		}

		fdsocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(fdsocket < 0) {
			perror("Failed to create socket");
		}

		if(connect(fdsocket, res->ai_addr, res->ai_addrlen) < 0) {
			perror("Connect failed");
		}

		if(getsockname(fdsocket, (struct sockaddr *)&addr, &addrlen) < 0) {
			perror("Getsockname failed");
		}

		freeaddrinfo(res);

		char *ip_addr = inet_ntoa(addr.sin_addr);
		strcpy(my_ip, ip_addr);
	}
	
	while(TRUE){
		printf("\n[Client@CSE489]$ ");
		fflush(stdout);
		char *msg = (char*) malloc(sizeof(char)*MSG_SIZE);
		char *msg_in = (char*) malloc(sizeof(char)*BUFFER_SIZE);
		memset(msg, '\0', MSG_SIZE);
		memset(msg_in, '\0', BUFFER_SIZE);

		fd_set master_list;
		FD_ZERO(&master_list);
		FD_SET(STDIN, &master_list);
		if (active_socket != -1) {
			FD_SET(active_socket, &master_list);
		}

		int highest_sock;
		if (active_socket > STDIN) {
			highest_sock = active_socket;
		} else {
			highest_sock = STDIN;
		}

		select(highest_sock + 1, &master_list, NULL, NULL, NULL);
		
		if (FD_ISSET(STDIN, &master_list)) {
			if(fgets(msg, MSG_SIZE-1, stdin) == NULL) { 
				printf("Nothing was entered, exiting");
				exit(-1);
			}

			if (strncmp(msg, "PORT", 4) == 0) {
				printf("[%.*s:SUCCESS]\n", 4, msg);
				printf("PORT:%s\n", port);
				printf("[%.*s:END]\n", 4, msg);
			}

			if (strncmp(msg, "IP", 2) == 0) {
				int fdsocket;
				struct addrinfo hints, *res;
				struct sockaddr_in addr;
				socklen_t addrlen = sizeof(addr);

				memset(&hints, 0, sizeof(hints));
				hints.ai_family = AF_INET;
				hints.ai_socktype = SOCK_DGRAM;

				if (getaddrinfo("8.8.8.8", "53", &hints, &res) != 0) {
					perror("getaddrinfo failed");
				}

				fdsocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
				if(fdsocket < 0) {
					perror("Failed to create socket");
				}

				if(connect(fdsocket, res->ai_addr, res->ai_addrlen) < 0) {
					perror("Connect failed");
				}

				if(getsockname(fdsocket, (struct sockaddr *)&addr, &addrlen) < 0) {
					perror("Getsockname failed");
				}

				freeaddrinfo(res);

				char *ip_addr = inet_ntoa(addr.sin_addr);
				memset(my_ip, '\0', INET_ADDRSTRLEN);
				strcpy(my_ip, ip_addr);


				printf("[%.*s:SUCCESS]\n", 2, msg);
				printf("IP:%s\n", ip_addr);
				printf("[%.*s:END]\n", 2, msg);
			}

			if (strncmp(msg, "LOGIN", 5) == 0) {
				char cmd[CMD_SIZE];
				char ipadr[INET_ADDRSTRLEN];
				char prt[6];
				if (sscanf(msg, "%s %s %s", cmd, ipadr, prt) != 3) {
					printf("Failed to parse LOGIN input\n");
					exit(-1);
				}
				
				int server;
				server = connect_to_host(ipadr, prt, port);
				if (server == -50) {
					continue;
				}
				active_socket = server; 

				void *buf = calloc(5, sizeof(struct client_ent));
				recv(server, buf, 5 * sizeof(struct client_ent), 0);

				memcpy(&client_lst, buf, sizeof(struct client_ent) * 5);

				printf("[%.*s:SUCCESS]\n", 5, msg);
				printf("[%.*s:END]\n", 5, msg);

				for (int i = 0; i < client_lst[0].active_clients; i++) {
					if ((strncmp(my_ip, client_lst[i].ip, INET_ADDRSTRLEN) == 0) && atoi(port) == client_lst[i].port) {
						for (int j = 0; j < client_lst[i].msg_buffered; j++) {
							char from_ip[INET_ADDRSTRLEN];
							char msg_recv[MSG_SIZE];
							memset(msg_recv, '\0', MSG_SIZE);
							strncpy(from_ip, client_lst[i].msg_buf[j], INET_ADDRSTRLEN);
							char *ptr_msg = &client_lst[i].msg_buf[j];
							ptr_msg += INET_ADDRSTRLEN;
							strcpy(msg_recv, ptr_msg);

							printf("\n[RECEIVED:SUCCESS]\n");
							printf("msg from:%s\n[msg]:%s\n", from_ip, msg_recv);
							printf("[RECEIVED:END]\n");
						}
						break;
					}
				}

				free(buf);
			}

			if (strncmp(msg, "EXIT", 4) == 0) {
				printf("[%.*s:SUCCESS]\n", 4, msg);
				printf("[%.*s:END]\n", 4, msg);
				if (active_socket == -1) {
					free(buff);
					exit(0);
				}
				if (send(active_socket, "EXIT", sizeof(char) * 4, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}
				close(active_socket);
				free(buff);
				exit(0);
			}

			if (strncmp(msg, "LOGOUT", 6) == 0) {
				if (send(active_socket, "LOGOUT", sizeof(char) * 6, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}
				printf("[%.*s:SUCCESS]\n", 6, msg);
				printf("[%.*s:END]\n", 6, msg);
				close(active_socket);
				active_socket = -1;
			}

			if (strncmp(msg, "REFRESH", 7) == 0) {

				if (send(active_socket, "REFRESH", sizeof(char) * 7, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}

				if (recv(active_socket, buff,  5 * sizeof(struct client_ent), 0) < 0) {
					printf("recv failed");
				}
				
				memcpy(&client_lst, buff, sizeof(struct client_ent) * 5);

				printf("[%.*s:SUCCESS]\n", 7, msg);
				printf("[%.*s:END]\n", 7, msg);
			}

			if (strncmp(msg, "LIST", 4) == 0) {

				int minus_val = 0;
				qsort(client_lst, client_lst[0].active_clients, sizeof(struct client_ent), ports_comp);

				printf("[%.*s:SUCCESS]\n", 4, msg);
				for (int i = 1; i <= client_lst[0].active_clients; i++) {
					if (strncmp(client_lst[i-1].status, "logged-in", 9) == 0) {
						printf("%-5d%-35s%-20s%-8d\n", i-minus_val, client_lst[i-1].hostn, client_lst[i-1].ip, client_lst[i-1].port);
					} else {
						minus_val++;
					}
				}
				printf("[%.*s:END]\n", 4, msg);
			}

			if (strncmp(msg, "SEND", 4) == 0) {

				char cmd[CMD_SIZE];
				char ipadr[INET_ADDRSTRLEN];
				char *ptr = msg;
				int ind = 0;
				if (sscanf(msg, "%s %s", cmd, ipadr) != 2) {
					printf("Failed to parse LOGIN input\n");
					exit(-1);
				}

				if (send(active_socket, msg, MSG_SIZE, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}

				printf("[%.*s:SUCCESS]\n", 4, msg);
				printf("[%.*s:END]\n", 4, msg);
			}

			if (strncmp(msg, "BROADCAST", 9) == 0) {

				if (send(active_socket, msg, MSG_SIZE, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}

				printf("[%.*s:SUCCESS]\n", 9, msg);
				printf("[%.*s:END]\n", 9, msg);
			}

			if (strncmp(msg, "BLOCK", 5) == 0) {

				char cmd[CMD_SIZE];
				char ipadr[INET_ADDRSTRLEN];
				int ind = 0;
				if (sscanf(msg, "%s %s", cmd, ipadr) != 2) {
					printf("Failed to parse LOGIN input\n");
					exit(-1);
				}

				if (send(active_socket, msg, MSG_SIZE, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}

				printf("[%.*s:SUCCESS]\n", 5, msg);
				printf("[%.*s:END]\n", 5, msg);

			}

			if (strncmp(msg, "UNBLOCK", 7) == 0) {

				char cmd[CMD_SIZE];
				char ipadr[INET_ADDRSTRLEN];
				int ind = 0;
				if (sscanf(msg, "%s %s", cmd, ipadr) != 2) {
					printf("Failed to parse LOGIN input\n");
					exit(-1);
				}

				if (send(active_socket, msg, MSG_SIZE, 0) < 0) {
					perror("Send failed");
					exit(-1);
				}

				printf("[%.*s:SUCCESS]\n", 7, msg);
				printf("[%.*s:END]\n", 7, msg);
			}
		}

		if (active_socket != -1 && FD_ISSET(active_socket, &master_list)) {

			recv(active_socket, msg_in, MSG_SIZE, 0);
			char from_ip[INET_ADDRSTRLEN];
			char msg_recv[MSG_SIZE];
			memset(msg_recv, '\0', MSG_SIZE);
			strncpy(from_ip, msg_in, INET_ADDRSTRLEN);
			char *ptr_msg = msg_in;
			ptr_msg += INET_ADDRSTRLEN;
			strcpy(msg_recv, ptr_msg);

			printf("[RECEIVED:SUCCESS]\n");
			printf("msg from:%s\n[msg]:%s\n", from_ip, msg_recv);
			printf("[RECEIVED:END]\n");
		}

	}
	return 0;
}

int connect_to_host(char *server_ip, char* server_port, char *local_port) {
		int fdsocket;
		struct addrinfo hints, *res;
		
		/* Set up hints structure */	
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		
		/* Fill up address structures */	
		if (getaddrinfo(server_ip, server_port, &hints, &res) != 0) {
			printf("[%s:ERROR]\n", "LOGIN");
			printf("[%s:END]\n", "LOGIN");
			return -50;
		}

		/* Socket */
		fdsocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(fdsocket < 0)
			perror("Failed to create socket");
		
		/* Connect */
		if(connect(fdsocket, res->ai_addr, res->ai_addrlen) < 0)
			perror("Connect failed");
		
		char port_to_send[8];
		sprintf(port_to_send, "%s%c", local_port, '\0');
		if (send(fdsocket, port_to_send, sizeof(char) * 8, 0) < 0) {
        		perror("Send failed");
        		exit(-1);
    	}

		freeaddrinfo(res);

		return fdsocket;
	}

int ports_comp(void *a, void *b) {

	struct client_ent *a_conv = a;
	struct client_ent *b_conv = b;

	int port1 = a_conv->port;
	int port2 = b_conv->port;

	return port1 - port2;
}
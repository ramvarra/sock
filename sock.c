#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>


#include "sock.h"

void fatal(char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    perror(buf);
    va_end(args);
    exit(1);
}

/*
** Messages are NULL terminated strings.
*/
void send_msg(int fd, char *buf) {
    int n = strlen(buf)+1;
    int n_sent = 0;
    int current_sent;
    while (n_sent < n) {
        if ((current_sent = write(fd, buf+n_sent, n-n_sent)) < 0) {
            if (current_sent == 0) {
                fatal("Unexpected return of 0 sent");
            }
            fatal("Failed to write to sock %d bytes", n-n_sent);
        }
        n_sent += current_sent;
    }
    printf("SND: '%s'\n", buf);
}

void receive_msg(int fd, char *buf, int buf_size) {
    int n_received = 0;
    int c;
    while (n_received < buf_size-1) {
        if ((c = read(fd, buf+n_received, 1)) <= 0) {
            if (c == 0) {
                break;
            }
            fatal("Sock read failed");
        }
        n_received += 1;
        if (buf[n_received - 1] == 0) {
            break;
        }
    }
    buf[n_received] = 0;
}

void conversation_client(int sock) {
    char buf[1024];
    for (int i = 0; i < 10; i++) {
        sprintf(buf, "%d HELLO %d", getpid(), i+1);
        send_msg(sock, buf);
        receive_msg(sock, buf, sizeof(buf));
        printf("RCV: '%s'\n", buf);
        sleep(5);
    }
}

void conversation_server(int sock, char *client) {
    char buf[1024];
    int n_read = 0;
    while (1) {
        receive_msg(sock, buf, sizeof(buf));
        printf("RCV from %s: '%s'\n", client, buf);
        if (strlen(buf) == 0) {
            break;
        }
        send_msg(sock, buf);
    }
}


int accept_client_connection(int server_sock, char *client_address) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    int sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (sock < 0) {
        fatal("Accept failed");
    }    
    sprintf(client_address, "%s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    printf("Got connection from: %s\n", client_address);
    return sock;
}

void multiplexing_conversation_server(int listen_sock) {
    printf("Start serving on port %d - multiplexing\n", SERVER_PORT);
    fd_set in_read_fds, except_fds;
    FD_ZERO(&in_read_fds);
    // FD_ZERO(&except_fds);

    FD_SET(listen_sock, &in_read_fds);
    // FD_SET(server_sock, except_fds);
    int nfds = listen_sock + 1;
    while (1) {
        fd_set out_read_fds = in_read_fds;
        fd_set out_except_fds = in_read_fds;
        printf("Waiting for new connection or some activity from clients: \n");
        int ret = select(nfds, &out_read_fds, NULL, &out_except_fds, NULL);
        if (ret < 0) {
            fatal("Select failed");
        }
        for (int fd  = 0; fd < nfds; fd++) {
            if (FD_ISSET(fd, &out_read_fds)) {
                if (fd == listen_sock) {
                    printf("listen_sock ready to read... calling accept\n");
                    char client_address[128];
                    int client_sock = accept_client_connection(listen_sock, client_address);
                    // add this for future tracking
                    FD_SET(client_sock, &in_read_fds);
                    if (client_sock >= nfds) {
                        nfds = client_sock + 1;
                    }
                } else {
                    char buf[1024];
                    // some client has input to read
                    receive_msg(fd, buf, sizeof(buf));
                    printf("RCV from %d: '%s'\n", fd, buf);
                    if (strlen(buf) == 0) {
                        close(fd);
                        // client done
                        FD_CLR(fd, &in_read_fds);
                    } else {
                        send_msg(fd, buf);
                    }
                }
            } else {
                if (FD_ISSET(fd, &out_except_fds)) {
                    if (fd == listen_sock) {
                        // we can loose listen_sock
                        fatal("exception on listen_sock");
                    } else {
                        // one of the client went bust. clear it.
                        close(fd);
                        FD_CLR(fd, &in_read_fds);
                        printf("Cleared client sock %d\n", fd);
                    }
                }
            }
        }
    }
}

void traditional_server(int listen_sock, int forking) {
    printf("Start serving on port %d - traditional %s\n", SERVER_PORT, forking? "forking" : "non-forking");
    char client_address[128];
    int client_sock;
    while ((client_sock = accept_client_connection(listen_sock, client_address)) >= 0) {
        if (forking) {
            if (fork() == 0) {
                close(listen_sock);
                conversation_server(client_sock, client_address);
                close(client_sock);
            }            
        } else {
            conversation_server(client_sock, client_address);            
        }
        close(client_sock);
    }
}
    
void sock_client(char *server) {
    int client_sock;
    struct sockaddr_in server_address;

    printf("I am sock client\n");
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fatal("Failed to client sock");
    }
    MEM_ZERO(server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
 
    struct hostent *he = gethostbyname(server);
    if (he == NULL) {
        fatal("Can not resolve server %s", server);
    }
    memcpy(&server_address.sin_addr, he->h_addr_list[0], sizeof(server_address.sin_addr));
    
    printf("Connecting to server: %s port %d\n", server, SERVER_PORT);
    if (connect(client_sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fatal("Failed to connect to server");
    } 
    conversation_client(client_sock);
    close(client_sock);
}


void sock_server() {
    printf("I am sock server\n");
    int listen_sock;
    struct sockaddr_in server_address;

    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fatal("Failed to create server sock");
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);
    if (bind(listen_sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fatal("Failed to bind server address to sock");
    }
    listen(listen_sock, 5);

    multiplexing_conversation_server(listen_sock);
}

int main(int argc, char *argv[]) {
    for (int i = 0; i < argc; i++) {
        printf("Hello arg = %s\n", argv[i]);
    }

    if (argc == 2) {
        sock_client(argv[1]);
    } else {
        sock_server();
    } 

    return(0);
}
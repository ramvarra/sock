#include <stdio.h>
#include <string.h>
#include <linux/socket.h>
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
        sprintf(buf, "HELLO %d", i+1);
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

void forking_conversation_server(int sock, char *client) {
    if (fork() == 0) {
        // child
        conversation_server(sock, client);
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
    int server_sock;
    struct sockaddr_in server_address;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fatal("Failed to create server sock");
    }
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(SERVER_PORT);
    if (bind(server_sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fatal("Failed to bind server address to sock");
    }
    listen(server_sock, 5);
    int sock;
    printf("Start serving on port %d\n", SERVER_PORT);
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    while ((sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) >= 0) {
        char client_address[128];
        sprintf(client_address, "%s:%d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        //conversation_server(sock, client_address);
        forking_conversation_server(sock, client_address);
        close(sock);
    }
    fatal("Failed to accept a connection");
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

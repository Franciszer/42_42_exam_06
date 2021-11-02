#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct s_client {
	int fd;
	int id;
	struct s_client *next;
} t_client;

enum	e_err {
	ERR_FATAL,
	ERR_ARGS
};

t_client*	g_clients = NULL;
int 		max_fd;
fd_set all_set, read_set, write_set;

int sockfd, g_id = 0;
char server_msg[42];
char client_msg[4096 * 42], buffer[4096 * 42 + 42];

const char *errs[2] = {
	"Fatal error\n",
	"Wrong number of arguments"
};

void fatal() {
	write(2, errs[ERR_FATAL], strlen(errs[ERR_FATAL]));

	t_client*	nav = g_clients;

	while(nav) {
		t_client*	tmp = nav->next;
		free(nav);
		nav = tmp;
	}
	exit(1);
}

void send_message(const int fd, char* msg, const int len) {
	t_client*	nav = g_clients;
	while (nav) {
		if (nav->fd != -1 && nav->fd != fd && FD_ISSET(nav->fd, &write_set)) {
			send(nav->fd, msg, len, 0);
		}
		nav = nav->next;
	}
	bzero(msg, len);
}

void add_client() {
	struct sockaddr_in	cli;
	socklen_t len = sizeof(cli);
	int new_fd = accept(sockfd, (struct sockaddr*)&cli, &len);
	if (new_fd < 0)
		fatal();
	else if (new_fd > max_fd)
		max_fd = new_fd;
	t_client*	new = malloc(sizeof(t_client));

	if (!new)
		fatal();

	new->fd = new_fd;
	new->id = g_id++;

	if (g_clients)
		new->next = g_clients;
	else
		new->next = NULL;
	g_clients = new;

	sprintf(server_msg, "server: client %d just arrived\n", new->id);
	const int msg_len = strlen(server_msg);
	send_message(new->fd, server_msg, msg_len);
	FD_SET(new->fd, &all_set);
}

void del_client(t_client* client) {
	FD_CLR(client->fd, &all_set);
	close(client->fd);
	client->fd = -1;
	sprintf(server_msg, "server: client %d just left\n", client->id);
	send_message(sockfd, server_msg, strlen(server_msg));

}

void handle_message(t_client* client) {
	char*	begin = client_msg;
	char*	end = client_msg;
	while ( *begin ) {
		const char curr = *end;
		if (curr == '\n' || curr == '\0') {
			*end = '\0';
			sprintf(buffer, "client %d: %s\n", client->id, begin);
			send_message(client->fd, buffer, strlen(buffer));
			bzero(client_msg, end - begin);
			if (curr == '\n')
				++end;
			begin = end;
		}
		else
			end++;
	}
}

void handle_request() {
	t_client*	nav = g_clients;
	if ( FD_ISSET(sockfd, &read_set) ) {
		add_client();
	}
	while (nav) {
		if (FD_ISSET(nav->fd, &read_set)) {
			int ret = recv(nav->fd, client_msg, sizeof(client_msg), 0);

			if (ret == -1)
				fatal();
			// case client leaves
			else if (ret == 0)
				del_client(nav);
			// case client_message
			else
				handle_message(nav);
		}
		nav = nav->next;
	}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		write(2, errs[ERR_ARGS], strlen(errs[ERR_ARGS]));
		exit(1);
	}

	struct sockaddr_in servaddr;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		fatal();
	max_fd = sockfd;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	const int port = atoi(argv[1]);
	servaddr.sin_port = htons(port);

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();

	if (listen(sockfd, 0) != 0)
		fatal();

	bzero(server_msg, sizeof(server_msg));
	bzero(client_msg, sizeof(client_msg));
	bzero(buffer, sizeof(buffer));
	FD_ZERO(&all_set);
	FD_SET(sockfd, &all_set);
	while (1) {
		write_set = read_set = all_set;
		if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) <= 0) {
			continue;
		}
		else {
			handle_request();
		}
	}
}

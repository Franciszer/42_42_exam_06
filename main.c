#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct  s_server {
	/* data */
	int		*ids; 	// client ids indexed by fd
	int		sockfd; // server fd
	int		max_fd;
	int		max_id;
	fd_set	rd;
	fd_set	wr;
	fd_set	fds;
}               t_server;

const char*	errs[2] = {
		"Fatal\n",
		"Wrong number of arguments\n"
};

void			fatal(t_server* server) {

	// if server is initialized
	if (server->sockfd > 0) {
		for (int fd = 0 ; fd < server->max_fd + 1 ; fd++) {
			if (server->ids[fd] != -1)
				close(fd);
		}
		free(server->ids);
	}

	write(2, errs[0], strlen(errs[0]));

	exit(1);
}


void		init_server(t_server* server, int sockfd) {
	server->sockfd = sockfd;
	server->max_fd = sockfd;
	server->max_id = 0;
	server->ids = malloc(sizeof(int) * (server->sockfd + 1));

	if (!server->ids)
		fatal(server);

	for (int i = 0 ; i < sockfd + 1; i++)
		server->ids[i] = -1;
	FD_ZERO(&server->fds);
	FD_SET(sockfd, &server->fds);
}

void		send_message(const char* msg, const int len, const int cli_fd, t_server* server) {

	for (int fd = 0 ; fd < server->max_fd + 1 ; fd++) {
		if (fd != server->sockfd && fd != cli_fd &&
			server->ids[fd] != -1 &&
			FD_ISSET(fd, &server->wr)) {
			send(fd, msg, len, 0);
		}
	}
}

void		add_client(t_server* server) {
	struct sockaddr_in	cli;
	socklen_t			cli_len;

	bzero(&cli, sizeof(cli));

	const int	cli_fd = accept(server->sockfd, (struct sockaddr*)&cli, &cli_len);
	if (cli_fd == -1)
		fatal(server);

	FD_SET(cli_fd, &server->fds);

	if (cli_fd > server->max_fd) {
		server->ids = realloc(server->ids, sizeof(int) * (cli_fd + 1));
		if (!server->ids)
			fatal(server);
		for (int i = server->max_fd + 1; i < cli_fd ; i++) {
			server->ids[i] = -1;
		}
		server->max_fd = cli_fd;
	}

	char	server_msg[42];
	int len = sprintf(server_msg, "server: client %d just arrived\n", server->max_id);
	server->ids[cli_fd] = server->max_id++;
	send_message(server_msg, len, cli_fd, server);
}

void		del_client(int fd, t_server* server) {
	const int	client_id = server->ids[fd];
	char		server_msg[42];

	server->ids[fd] = -1;
	close(fd);
	FD_CLR(fd, &server->fds);
	int len = sprintf(server_msg, "server: client %d just left\n", client_id);
	send_message(server_msg, len, server->sockfd, server);

}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

void		handle_client_message(int fd, t_server* server) {
	char	buffer[42*4096];
	char*	cli_msg = NULL;

	bzero(buffer, sizeof(buffer));
	int		red = recv(fd, buffer, sizeof(buffer), 0);
	if (red == -1)
		fatal(server);
	else if (red == 0)
		del_client(fd, server);
	else {
		char*	line = NULL;
		int		ret;
		char* bptr = buffer;
		int ct = 0;
		while ((ret = extract_message(&bptr, &line))) {
			cli_msg = malloc(sizeof(char) * 20);
			sprintf(cli_msg, "client %d: ", server->ids[fd]);
			cli_msg = str_join(cli_msg, line);
			send_message(cli_msg, strlen(cli_msg), fd, server);
			if (ct++ > 0)
				free(line);
			free(cli_msg);
		}
	}
}

void		handle_clients(t_server* server) {
	for (int fd = 0 ; fd < server->max_fd + 1; fd++) {
		if (FD_ISSET(fd, &server->rd)) {
			if (fd == server->sockfd)
				add_client(server);
			else
				handle_client_message(fd, server);
		}
	}
}

void		run_server(t_server* server) {
	int	n_fds;

	while (1) {
		server->rd = server->wr = server->fds;
		n_fds = select(	server->max_fd + 1,
						   &server->rd, &server->wr,
						   NULL, NULL);
		if (n_fds <= 0)
			continue ;

		handle_clients(server);
	}
}

int main(int argc, char** argv) {
	if (argc != 2) {
		write(2, errs[1], strlen(errs[1]));
		exit(1);
	}

	int 				sockfd;
	struct sockaddr_in 	servaddr;
	t_server			server;
	int					port = atoi(argv[1]);

	server.sockfd = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
		fatal(&server);

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal(&server);
	if (listen(sockfd, 10) != 0)
		fatal(&server);

	init_server(&server, sockfd);

	run_server(&server);
}
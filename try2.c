#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

#define MAX_CLIENT 4 + 1024
#define BUFF_SIZE 1025

typedef struct s_client
{
  int id;
  int fd;
  char *msg;
} t_client;

typedef struct s_srv
{
  int sockfd;
  int maxfd;
  size_t nextid;
  char w_buff[42], r_buff[BUFF_SIZE];
  t_client clients[MAX_CLIENT];
  fd_set fds, w_fds, r_fds;
} t_srv;

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

void	exiterr(char* msg) {
	if (msg)
		write(2, msg, strlen(msg));
	else
		write(2, "Fatal Error", 11);
	write(2, "\n", 1);
	exit(1);
}

void	init_srv(t_srv *srv, int port) {
	struct sockaddr_in servaddr;

	// socket create and verification
	srv->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (srv->sockfd == -1)
		exiterr(NULL);
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port);

	// Binding newly created socket to given IP and verification
	if ((bind(srv->sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		exiterr(NULL);
	if (listen(srv->sockfd, 10) != 0)
		exiterr(NULL);
	srv->maxfd = srv->sockfd + 1;
	FD_ZERO(&srv->fds);
	FD_SET(srv->sockfd, &srv->fds);
	bzero(&srv->clients, sizeof(srv->clients));
}

void send_message(int fd, const char *msg, size_t len) {
	size_t sent = 0;
	while (sent < len) {
		ssize_t bytes_sent = send(fd, msg + sent, len - sent, 0);
		if (bytes_sent <= 0)
			break;
		sent += bytes_sent;
	}
}

int main(int ac, char **av) {
	if (ac != 2)
		exiterr("Wrong number of arguments");

	t_srv srv;
	init_srv(&srv, atoi(av[1]));

	int selected;
	while (1) {
		srv.r_fds = srv.fds;
		srv.w_fds = srv.fds;
		selected = select(srv.maxfd, &srv.r_fds, &srv.w_fds, NULL, NULL);
		if (selected == -1)
			continue;

		for (int fd = 0; fd < srv.maxfd; fd++) {
			if (FD_ISSET(fd, &srv.r_fds) == 0)
				continue;
			else if (fd == srv.sockfd) {
				int connfd = accept(srv.sockfd, NULL, NULL);
				if (connfd < 0)
					exiterr(NULL);
				srv.clients[connfd].fd = connfd;
				srv.clients[connfd].id = srv.nextid++;
				srv.clients[connfd].msg = NULL;
				srv.maxfd = connfd > srv.maxfd ? connfd : srv.maxfd;
				FD_SET(connfd, &srv.fds);
				if (connfd >= srv.maxfd)
					srv.maxfd = connfd + 1;
				sprintf(srv.w_buff, "server: client %d just arrived\n", srv.clients[connfd].id);
				for (int wfd = 0; wfd < srv.maxfd; wfd++) {
					if (wfd != connfd && FD_ISSET(wfd, &srv.w_fds))
						send_message(wfd, srv.w_buff, strlen(srv.w_buff));
				}
			} else {
				int rec = recv(fd, srv.r_buff, BUFF_SIZE - 1, 0);
				if (rec <= 0) {
					close(fd);
					srv.clients[fd].fd = 0;
					free(srv.clients[fd].msg);
					srv.clients[fd].msg = NULL;
					FD_CLR(fd, &srv.fds);
					sprintf(srv.w_buff, "server: client %d just left\n", srv.clients[fd].id);
					for (int wfd = 0; wfd < srv.maxfd; wfd++) {
						if (wfd != fd && FD_ISSET(wfd, &srv.w_fds))
							send_message(wfd, srv.w_buff, strlen(srv.w_buff));
					}
				} else {
					srv.r_buff[rec] = 0;
					srv.clients[fd].msg = str_join(srv.clients[fd].msg, srv.r_buff);
					sprintf(srv.w_buff, "client %d: ", srv.clients[fd].id);
					char *msg = NULL;
					while (extract_message(&srv.clients[fd].msg, &msg)) {
						for (int wfd = 0; wfd < srv.maxfd; wfd++) {

							if (wfd != fd && FD_ISSET(wfd, &srv.w_fds)) {
								send_message(wfd, srv.w_buff, strlen(srv.w_buff));
								send_message(wfd, msg, strlen(msg));
							}
						}
						free(msg);
					}
				}
			}
		}
	}

}

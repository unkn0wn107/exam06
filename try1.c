#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096

typedef struct
{
	int fd;
	int id;
} Client;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Wrong number of arguments\n");
		exit(1);
	}

	int server_fd, max_fd, activity, i, j, valread, sd;
	struct sockaddr_in address;
	char buffer[BUFFER_SIZE];
	fd_set readfds;
	Client *clients = NULL;
	int client_count = 0;

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
	{
		fprintf(stderr, "Fatal error\n");
		exit(1);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr("127.0.0.1");
	address.sin_port = htons(atoi(argv[1]));

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		fprintf(stderr, "Fatal error\n");
		exit(1);
	}

	if (listen(server_fd, 3) < 0)
	{
		fprintf(stderr, "Fatal error\n");
		exit(1);
	}

	while (1)
	{
		FD_ZERO(&readfds);
		FD_SET(server_fd, &readfds);
		max_fd = server_fd;

		for (i = 0; i < client_count; i++)
		{
			sd = clients[i].fd;
			if (sd > 0)
				FD_SET(sd, &readfds);
			if (sd > max_fd)
				max_fd = sd;
		}

		activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
		if (activity < 0)
			continue;

		if (FD_ISSET(server_fd, &readfds))
		{
			int new_socket;
			if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&address)) < 0)
			{
				fprintf(stderr, "Fatal error\n");
				exit(1);
			}

			fcntl(new_socket, F_SETFL, O_NONBLOCK);

			Client *temp = realloc(clients, (client_count + 1) * sizeof(Client));
			if (!temp)
			{
				fprintf(stderr, "Fatal error\n");
				exit(1);
			}
			clients = temp;
			clients[client_count].fd = new_socket;
			clients[client_count].id = client_count;

			sprintf(buffer, "server: client %d just arrived\n", client_count);
			for (i = 0; i < client_count; i++)
				send(clients[i].fd, buffer, strlen(buffer), 0);

			client_count++;
		}

		for (i = 0; i < client_count; i++)
		{
			sd = clients[i].fd;
			if (FD_ISSET(sd, &readfds))
			{
				if ((valread = recv(sd, buffer, BUFFER_SIZE, 0)) == 0)
				{
					sprintf(buffer, "server: client %d just left\n", clients[i].id);
					for (j = 0; j < client_count; j++)
						if (j != i)
							send(clients[j].fd, buffer, strlen(buffer), 0);
					close(sd);
					clients[i] = clients[client_count - 1];
					client_count--;
					i--;
				}
				else
				{
					buffer[valread] = '\0';
					char *line = strtok(buffer, "\n");
					while (line)
					{
						char message[BUFFER_SIZE];
						sprintf(message, "client %d: %s\n", clients[i].id, line);
						for (j = 0; j < client_count; j++)
							if (j != i)
								send(clients[j].fd, message, strlen(message), 0);
						line = strtok(NULL, "\n");
					}
				}
			}
		}
	}

	return 0;
}
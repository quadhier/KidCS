#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>


void write_header(int sockfd)
{
	char msg[200];
	strcpy(msg, "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n");
	write(sockfd, msg, strlen(msg));
	strcpy(msg, "accept-encoding: gzip, deflate, br\r\n");
	write(sockfd, msg, strlen(msg));
	strcpy(msg, "accept-language: zh-CN,zh;q=0.9,en;q=0.8,la;q=0.7\r\n");
	write(sockfd, msg, strlen(msg));
	strcpy(msg, "cache-control: no-cache\r\n");
	write(sockfd, msg, strlen(msg));
	strcpy(msg, "pragma: no-cache\r\n");
	write(sockfd, msg, strlen(msg));
	strcpy(msg, "user-agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/66.0.3359.181 Safari/537.36\r\n");
	write(sockfd, msg, strlen(msg));
	write(sockfd, "\r\n", 2);
}


void print_recv(int sockfd)
{
	int len = 0;
	char msg[1024];
	while((len = recv(sockfd, msg, 200, 0)) != 0)
	{
		write(STDOUT_FILENO, msg, len == -1 ? 0 : len);
	}
}

int main(int argc, const char **argv)
{
	if(argc < 3 || argc > 4 || 
			(strcmp(argv[1], "get") != 0 && strcmp(argv[1], "put") != 0) || 
			0)
//			(strcmp(argv[1], "put") == 0 && argc == 3) )
	{
		fprintf(stderr, "Usage: get|put url [key1=value1{&key2=value2}]\n");
		exit(EXIT_FAILURE);
	}

	// prepare server socket address
	const char *server_ip = "172.16.181.111";
	int server_port = 4321;
	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, server_ip, &server_address.sin_addr);
	server_address.sin_port = htons(server_port);

	// create client socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd > 0);
	if(connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0)
	{
		perror("connection failed");
	}
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);


	if(strcmp(argv[1], "get") == 0)
	{
		// write request line
		write(sockfd, "GET ", 4);
		write(sockfd, argv[2], strlen(argv[2]));
		write(sockfd, " HTTP/1.1", 9);
		write(sockfd, "\r\n", 2);

		write_header(sockfd);

		print_recv(sockfd);
	}
	else if(strcmp(argv[1], "put") == 0)
	{
		// write request line
		write(sockfd, "PUT ", 4);
		write(sockfd, argv[2], strlen(argv[2]));
		write(sockfd, " HTTP/1.1", 9);
		write(sockfd, "\r\n", 2);

		write_header(sockfd);

		if(argc > 3)
		{
			write(sockfd, argv[3], strlen(argv[3]));
		}

		print_recv(sockfd);
	}
	close(sockfd);
}

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
#include <pthread.h>

#define MULTI_REQ 0

struct conn_arg
{
	const struct sockaddr *pserver_address;
	const char *uri;
	const char *content;
};

int write_header(int sockfd)
{
	int wcnt = 0;
	char msg[200];
	strcpy(msg, "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "accept-encoding: gzip, deflate, br\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "accept-language: zh-CN,zh;q=0.9,en;q=0.8,la;q=0.7\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "cache-control: no-cache\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "pragma: no-cache\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "user-agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/66.0.3359.181 Safari/537.36\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	write(sockfd, "\r\n", 2);
	return wcnt + 2;
}

void print_client_port(int sockfd)
{
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	if(getsockname(sockfd, (struct sockaddr *)&client, &len) != 0)
	{
		perror("get client port failed");
	}
	else
	{
		printf("client port: %d\n", ntohs(client.sin_port));
	}

}

int print_recv(int sockfd)
{
	int rcnt = 0;
	int len = 0;
	char msg[1024];
	while((len = recv(sockfd, msg, 200, 0)) != 0)
	{
		write(STDOUT_FILENO, msg, len == -1 ? 0 : len);
		rcnt += len > 0 ? len : 0;
	}
	return rcnt;
}

void * get(void *arg)
{

	const struct sockaddr *pserver_address = ((struct conn_arg *)arg)->pserver_address;
	const char *uri = ((struct conn_arg *)arg)->uri;

	// create client socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd > 0);
	if(connect(sockfd, pserver_address, sizeof(*pserver_address)) != 0)
	{
		perror("connection failed");
	}
	else
	{
		print_client_port(sockfd);
	}
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	int wcnt = 0;
	// write request line
	write(sockfd, "GET ", 4);
	wcnt += 4;
	write(sockfd, uri, strlen(uri));
	wcnt += strlen(uri);
	write(sockfd, " HTTP/1.1", 9);
	wcnt += 9;
	write(sockfd, "\r\n", 2);
	wcnt += 2;

	wcnt += write_header(sockfd);
	printf("write %d bytes totally.\n", wcnt);

	int rcnt = print_recv(sockfd);
	printf("read %d bytes totally.\n", rcnt);

	close(sockfd);
	return NULL;
}

void * put(void *arg)
{

	const struct sockaddr *pserver_address = ((struct conn_arg *)arg)->pserver_address;
	const char *uri = ((struct conn_arg *)arg)->uri;
	const char *content = ((struct conn_arg *)arg)->content;

	// create client socket
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(sockfd > 0);
	if(connect(sockfd, pserver_address, sizeof(*pserver_address)) != 0)
	{
		perror("connection failed");
	}
	else
	{
		print_client_port(sockfd);
	}
	int flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);


	int wcnt = 0;
	// write request line
	write(sockfd, "PUT ", 4);
	wcnt += 4;
	write(sockfd, uri, strlen(uri));
	wcnt += strlen(uri);
	write(sockfd, " HTTP/1.1", 9);
	wcnt += 9;
	write(sockfd, "\r\n", 2);
	wcnt += 2;

	wcnt += write_header(sockfd);

	if(content != NULL)
	{
		write(sockfd, content, strlen(content));
		wcnt += strlen(content);
	}
	printf("write %d bytes totally.\n", wcnt);

	int rcnt = print_recv(sockfd);
	printf("read %d bytes totally.\n", rcnt);

	close(sockfd);
	return NULL;
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

	if(strcmp(argv[1], "get") == 0)
	{

		struct conn_arg arg = { (struct sockaddr *)&server_address, argv[2], NULL };
#if MULTI_REQ
		pthread_t tid;
		int tnum = 10;
		pthread_t tids[tnum];
		for(int i = 0; i < tnum; i++)
		{
			pthread_create(&tid, NULL, get, &arg);
			tids[i] = tid;
		}

		for(int i = 0; i < tnum; i++)
		{
			pthread_join(tids[i], NULL);
		}
#else
		get(&arg);
#endif

	}
	else if(strcmp(argv[1], "put") == 0)
	{
		const char *body = argc > 3 ? argv[3] : NULL;
		struct conn_arg arg = { (struct sockaddr *)&server_address, argv[2], body };
		put(&arg);
	}
}

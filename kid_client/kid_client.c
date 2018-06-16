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
#include <netdb.h>

#define MULTI_REQ 0
#define IPSIZE 16

struct conn_arg
{
	const struct sockaddr *pserver_address;
	const char *host;
	const char *rlurl;
	const char *content;
};

int write_header(int sockfd, const char *host)
{
	int wcnt = 0;
	char msg[200];
	sprintf(msg, "Host: %s\r\n", host);
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_4) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/66.0.3359.181 Safari/537.36\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

//	strcpy(msg, "Accept-Encoding: gzip, deflate, br\r\n");
//	write(sockfd, msg, strlen(msg));
//	wcnt += strlen(msg);

	strcpy(msg, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,la;q=0.7\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "Cache-Control: no-cache\r\n");
	write(sockfd, msg, strlen(msg));
	wcnt += strlen(msg);

	strcpy(msg, "Pragma: no-cache\r\n");
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
	const char *host = ((struct conn_arg*)arg)->host;
	const char *rlurl = ((struct conn_arg *)arg)->rlurl;

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
	write(sockfd, rlurl, strlen(rlurl));
	wcnt += strlen(rlurl);
	write(sockfd, " HTTP/1.1", 9);
	wcnt += 9;
	write(sockfd, "\r\n", 2);
	wcnt += 2;

	wcnt += write_header(sockfd, host);
	printf("write %d bytes totally.\n", wcnt);

	int rcnt = print_recv(sockfd);
	printf("read %d bytes totally.\n", rcnt);

	close(sockfd);
	return NULL;
}

void * put(void *arg)
{

	const struct sockaddr *pserver_address = ((struct conn_arg *)arg)->pserver_address;
	const char *host = ((struct conn_arg*)arg)->host;
	const char *rlurl = ((struct conn_arg *)arg)->rlurl;
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
	write(sockfd, rlurl, strlen(rlurl));
	wcnt += strlen(rlurl);
	write(sockfd, " HTTP/1.1", 9);
	wcnt += 9;
	write(sockfd, "\r\n", 2);
	wcnt += 2;

	wcnt += write_header(sockfd, host);

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

// TODO add mutex to make it thread-safe
int get_ip_addr(const char *domain, char *ip)
{
	struct hostent *phtent;
	char **paddr = NULL;
	phtent = gethostbyname(domain);
	if(phtent == NULL)
	{
		perror("get IP address failed");
		return -1;
	}
	for(paddr = phtent->h_addr_list; *paddr != NULL; paddr++)
	{
		if(inet_ntop(phtent->h_addrtype, *paddr, ip, IPSIZE))
		{
			return 0;
		}
	}
	return -1;
}

int parse_url(const char *url, char *server_ip, int *server_port, char **host, char **rlurl)
{
	int urllen = strlen(url);
	char *rooturl = NULL;
	const char *slash_end = strchr(url, '/');
	const char *qmark_end = strchr(url, '?');

	int len = 0;
	// with slash in url
	if(slash_end != NULL)
	{
		len = slash_end - url;
		rooturl = (char *)malloc(len + 1);
		strncpy(rooturl, url, len);
		rooturl[len] = '\0';
		*rlurl = (char *)malloc(urllen - len + 1);
		strcpy(*rlurl, slash_end);
	}
	// without slash but with question mark in url
	else if(qmark_end != NULL)
	{
		len = qmark_end - url;
		rooturl = (char *)malloc(len + 1);
		strncpy(rooturl, url, len);
		rooturl[len] = '\0';
		*rlurl = (char *)malloc(urllen - len + 2);
		**rlurl = '/';
		strcpy(*rlurl + 1, qmark_end);
	}
	else 
	{
		rooturl = (char *)malloc(urllen + 1);
		strcpy(rooturl, url);
		*rlurl = (char *)malloc(2);
		(*rlurl)[0] = '/';
		(*rlurl)[1] = '\0';
	}
	
	const char *colon_end = strchr(rooturl, ':');
	if(colon_end != NULL)
	{
		*server_port = atoi(colon_end + 1);
		len = colon_end - rooturl;
	}
	else
	{
		*server_port = 80;
		len = strlen(rooturl);
	}

	*host = (char *)malloc(len + 1);
	strncpy(*host, rooturl, len);
	(*host)[len] = '\0';

	if(get_ip_addr(*host, server_ip) != 0)
	{
		free(rooturl);
		return -1;
	}
	
	free(rooturl);
	return 0;
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

	// parse ip and port of the server
	char server_ip[IPSIZE];
	int server_port;
	char *host = NULL;
	char *rlurl = NULL;
	if(parse_url(argv[2], server_ip, &server_port, &host, &rlurl) != 0)
	{
		exit(EXIT_FAILURE);
	}

//	printf("%s %d %s\n", server_ip, server_port, rlurl);
//	exit(0);

	// prepare server socket address
//	const char *server_ip = "172.16.181.111";
//	int server_port = 4321;
	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	inet_pton(AF_INET, server_ip, &server_address.sin_addr);
	server_address.sin_port = htons(server_port);

	if(strcmp(argv[1], "get") == 0)
	{

		struct conn_arg arg = { (struct sockaddr *)&server_address, host, rlurl, NULL };
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
		struct conn_arg arg = { (struct sockaddr *)&server_address, host, rlurl, body};
		put(&arg);
	}
	free(host);
	free(rlurl);
}

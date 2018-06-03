//
// kid server
//
// author: quadhier
//
 

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>	
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "kid_map.h"

static const int MAX_REQ_LINE_LEN = 1024;
static const int MAX_HEADER_PARAM_LEN = 1024;
static const int MAX_QUERY_LEN = 256;

static const int MTD_GET     = 0x13;
static const int MTD_HEAD    = 0x24;
static const int MTD_POST    = 0x34;
static const int MTD_PUT     = 0x43;
static const int MTD_DELETE  = 0x56;
static const int MTD_CONNECT = 0x67;
static const int MTD_OPTIONS = 0x77;
static const int MTD_TRACE   = 0x85;

// to store request line information
struct req_line
{
	int len; // does not contain \r\n
	int method;
	char *uri;
	char version[10];
};

//
// static data struct for http query to operate on
//
struct kuser 
{
	char name[20];
	int age;
};
static struct kid_map user_data;
// TODO thread-safe operations on the static data
const struct kuser * get_kuser(const char *userid)
{
	return kid_map_get(&user_data, userid);
}

void add_kuser(char *userid, struct kuser *user)
{
	kid_map_put(&user_data, userid, user);
}


//
// equals function for map with string key
//
int equals(const void *key1, const void *key2)
{
	char *strkey1 = (char *)key1;
	char *strkey2 = (char *)key2;
	if(strcmp(strkey1, strkey2) == 0)
	{
		return 1;
	}
	return 0;
}

//
// get method name string by method code
//
static const char * get_method_name(int mtd)
{
	switch(mtd)
	{
		case 0x13:
			return "GET";
		case 0x24:
			return "HEAD";
		case 0x34:
			return "POST";
		case 0x43:
			return "PUT";
		case 0x56:
			return "DELETE";
		case 0x67:
			return "CONNECT";
		case 0x77:
			return "OPTIONS";
		case 0x85:
			return "TRACE";
		default:
			return NULL;
	}
}

// 
// get method code by method name string
// 
static int get_method_code(const char * mtd_str)
{
	if(strcmp(mtd_str, "GET") == 0)
	{
		return MTD_GET;
	}
	else if(strcmp(mtd_str, "HEAD") == 0)
	{
		return MTD_HEAD;
	}
	else if(strcmp(mtd_str, "POST") == 0)
	{
		return MTD_POST;
	}
	else if(strcmp(mtd_str, "PUT") == 0)
	{
		return MTD_PUT;
	}
	else if(strcmp(mtd_str, "DELETE") == 0)
	{
		return MTD_DELETE;
	}
	else if(strcmp(mtd_str, "CONNECT") == 0)
	{
		return MTD_CONNECT;
	}
	else if(strcmp(mtd_str, "OPTIONS") == 0)
	{
		return MTD_OPTIONS;
	}
	else if(strcmp(mtd_str, "TRACE") == 0)
	{
		return MTD_TRACE;
	}
	else
	{
		return -1;
	}
}

//
// parse the request line
//
struct req_line parse_req_line(const char * req_str)
{
	struct req_line req;
	const char *req_end = strstr(req_str, "\r\n");
	// invalid request line without \r\n
	if(req_end == NULL)
	{
		req.method = -1;
		return req;
	}
	req.len = req_end - req_str;
	
	const char *mtd_start = req_str;
	const char *mtd_end;
	const char *uri_start;
	const char *uri_end;
	const char *ver_start;
	const char *ver_end = req_end;
	
	// split method, uri and version
	// while ensure the correctness of the format
	mtd_end = strchr(req_str, ' ');
	if(mtd_end == NULL)
	{
		req.method = -1;
		return req;
	}
	uri_start = mtd_end + 1;
	uri_end = strchr(mtd_end + 1, ' ');
	if(uri_end == NULL)
	{
		req.method = -1;
		return req;
	}
	ver_start = uri_end + 1;
	const char *next_space = strchr(ver_start, ' ');
	if(next_space != NULL && req_end - next_space >= 0)
	{
		req.method = -1;
		return req;
	}
	
	char mtd[10];
	strncpy(mtd, mtd_start, mtd_end - mtd_start);
	mtd[mtd_end - mtd_start] = '\0'; // make it a C-style string
	req.method = get_method_code(mtd);
	req.uri = (char *)malloc(uri_end - uri_start + 1);
	strncpy(req.uri, uri_start, uri_end - uri_start);
	req.uri[uri_end - uri_start] = '\0'; // make it a C-style string
	strncpy(req.version, ver_start, ver_end - ver_start);
	req.version[ver_end - ver_start] = '\0'; // make it a C-style string

	return req;
}

//
// parse the request header
//
int parse_req_header(const char *hdr_str, struct kid_map *hdr_params)
{
	const char *param_end = strstr(hdr_str, "\r\n");
	if(param_end == NULL)
	{
		return -1;
	}
	else if(param_end == hdr_str)
	{
		return 0;
	}

	const char *name_start = hdr_str;
	const char *name_end = strstr(hdr_str, ":");
	if(name_end == NULL)
	{
		return -1;
	}
		
	const char *value_start = name_end + 1;
	const char *value_end = param_end;
	
	char *name = (char *)malloc(name_end - name_start + 1);
	char *value  = (char *)malloc(value_end - value_start + 1);
	strncpy(name, name_start, name_end - name_start);
	strncpy(value, value_start, value_end - value_start);
	name[name_end - name_start] = '\0';
	value[value_end - value_start] = '\0';
	
	kid_map_put(hdr_params, name, value);
	return param_end - hdr_str;
}

//
// process the query
//
char * process_query(int method, const char * query)
{
	struct kid_map req_params;
	kid_map_init(&req_params, 10, equals, free, free);
	const char *param_start = query;
	const char *param_end = NULL;
	while(1)
	{
		param_end = strchr(param_start, '&');
		const char *pname_start = param_start;
		const char *pname_end = strchr(pname_start, '=');
		if(pname_end == NULL)
		{
			kid_map_free(&req_params);
			return NULL;
		}
		const char *pvalue_start = pname_end + 1;
		const char *pvalue_end = param_end;
		
		char *pname = (char *)malloc(pname_end - pname_start + 1);
		strncpy(pname, pname_start, pname_end - pname_start);
		pname[pname_end - pname_start] = '\0';

		char *pvalue = NULL;
		if(param_end == NULL)
		{
			pvalue = (char *)malloc(strlen(pvalue_start) + 1);	
			strcpy(pvalue, pvalue_start);
			kid_map_put(&req_params, pname, pvalue);
			break;
		}
		else
		{
			pvalue = (char *)malloc(pvalue_end - pvalue_start + 1);
			strncpy(pvalue, pvalue_start, pvalue_end - pvalue_start);
			pvalue[pvalue_end - pvalue_start] = '\0';
			param_start = param_end + 1;
		}

		kid_map_put(&req_params, pname, pvalue);
	}

	char *result = (char *)malloc(100);

	const char *userid = kid_map_get(&req_params, "userId");
	if(userid == NULL)
	{
		sprintf(result, "Insufficient Query Parameters\n");
		kid_map_free(&req_params);
		return result;
	}

	if(method == MTD_GET)
	{
		const struct kuser *user = get_kuser(userid);
		if(user == NULL)
		{
			sprintf(result, "No Such User\n");
		}
		else
		{
			sprintf(result, "UserId: %s\nName: %s\nAge: %d\n", userid, user->name, user->age);
		}

	}
	else if(method == MTD_PUT)
	{
		const char *name = kid_map_get(&req_params, "name");
		const char *age_str = kid_map_get(&req_params, "age");
		if(name == NULL || age_str == NULL)
		{
			sprintf(result, "Insufficient Query Parameters\n");
		}
		else
		{
			struct kuser *user = (struct kuser *)malloc(sizeof(struct kuser));
			strcpy(user->name, name);
			user->age = (int)strtol(age_str, NULL, 10);
			char *new_userid = (char *)malloc(strlen(userid) + 1);
			strcpy(new_userid, userid);
			add_kuser(new_userid, user);
			sprintf(result, "PUT SUCCESS\n");
		}
	}

	kid_map_free(&req_params);
	return result;
}

void process_connection(int connfd)
{
	// set timeout for receiving message
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 10 * 1000;
	setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));

	//
	// parse the request line
	//
	char req_str[MAX_REQ_LINE_LEN + 1];
	bzero(req_str, MAX_REQ_LINE_LEN + 1);
	// read until buffer is full without moving file pointer
	int recvlen = recv(connfd, req_str, MAX_REQ_LINE_LEN,  MSG_PEEK | MSG_WAITALL); 
	if(recvlen == -1 && errno == EAGAIN)
	{
		recvlen = 0;
		perror("Timeout");
	}
	req_str[recvlen] = '\0'; // make it a C-style string
	struct req_line rline = parse_req_line(req_str); 
	// rline.uri won't be allocated unless req_str if successfully parsed
	if(rline.method == -1)
	{
		fprintf(stderr, "Invalid Request Line\n");
		close(connfd);
		return;
	}
	// set the file pointer to the start of the request header
	// lseek(connfd, rline.len + 2, SEEK_CUR); // lseek is invalid to socket
	recv(connfd, req_str, rline.len + 2, MSG_WAITALL);

	//
	// parse the request header
	//
	char hdr_str[MAX_HEADER_PARAM_LEN + 1];
	bzero(hdr_str, MAX_HEADER_PARAM_LEN + 1);
	// to store the request header parameters
	struct kid_map hdr_params;
	kid_map_init(&hdr_params, 10, equals, free, free);
	// read until buffer is full without moving file pointer
	int hasError = 0;
	while(1)
	{
		recvlen = recv(connfd, hdr_str, MAX_REQ_LINE_LEN, MSG_PEEK | MSG_WAITALL);
		recvlen = recvlen == -1 ? 0 : recvlen;
		hdr_str[recvlen] = '\0'; // make it a C-style string
		int len = parse_req_header(hdr_str, &hdr_params);
		if(len == -1)
		{
			hasError = 1;
			break;
		}
		else if(len == 0)
		{
			// lseek(connfd, 2, SEEK_CUR);
			recv(connfd, hdr_str, 2, MSG_WAITALL);
			break;
		}
		// lseek(connfd, len + 2, SEEK_CUR);
		recv(connfd, hdr_str, len + 2, MSG_WAITALL);
	}
	if(hasError == 1)
	{
		fprintf(stderr, "Invalid Request Header Parameter\n");
		free(rline.uri);
		kid_map_free(&hdr_params);
		close(connfd);
		return;
	}

	printf("request method: %s\nrequest uri: %s\nhttp verion: %s\n",
			get_method_name(rline.method),
			rline.uri,
			rline.version);

	printf("request header:\n");
	for(int i = 0; i < hdr_params.size; i++)
	{
		printf("%s:%s\n", hdr_params.keys[i], kid_map_get(&hdr_params, hdr_params.keys[i]));
	}

	
	char *proc_res = NULL;
	if(rline.method == MTD_GET)	
	{
		char *query = strstr(rline.uri, "?");
		if(query != NULL)
		{
			query++;
			proc_res = process_query(MTD_GET, query);
		}
		else
		{
			proc_res = malloc(1);
			proc_res[0] = '\0';
		}
	}
	else if(rline.method == MTD_PUT)
	{
		char query[MAX_QUERY_LEN];
		recvlen = recv(connfd, query, MAX_QUERY_LEN, MSG_WAITALL);	
		recvlen = recvlen == -1 ? 0 : recvlen;
		query[recvlen] = '\0';
		proc_res = process_query(MTD_PUT, query);
	}
	
	if(proc_res == NULL)
	{
		proc_res = (char *)malloc(20);
		strcpy(proc_res, "ERROR_IN_QUERY\n");
	}

	char resp_line[100];
	sprintf(resp_line, "HTTP/1.1 200 OK\r\n\r\n");
	send(connfd, resp_line, strlen(resp_line), 0);
	send(connfd, proc_res, strlen(proc_res), 0);
	sprintf(resp_line, "Hello Friend%d\n", rand());
	send(connfd, resp_line, strlen(resp_line), 0);

	free(rline.uri);
	kid_map_free(&hdr_params);
	free(proc_res);
	close(connfd);
}


int main()
{
	// initialize global data structure
	kid_map_init(&user_data, 10, equals, free, free);
	struct kuser *user = malloc(sizeof(struct kuser));
	strcpy(user->name, "haha");
	user->age = 12;
	char *userid = malloc(5);
	strcpy(userid, "xixi");
	add_kuser(userid, user);



	// prepare for the server address 
	const char *ip = "172.16.181.111";
	int port = 4321;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &address.sin_addr);
	address.sin_port = htons(port);

	// create socket 
	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if(sockfd <= 0)
	{
		perror("creating socket failed");
		assert(sockfd > 0);
	}

	// for ease of debug
	// let port reusable immediately without TIME_WAIT to wait
	int flag = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

	// bind the socket with the address prepared
	int ret = bind(sockfd, (struct sockaddr *)&address, sizeof(address));
	if(ret == -1)
	{
		perror("binding failed");
		assert(ret != -1);
	}
	// listen on the socket
	ret = listen(sockfd, 5);
	if(ret == -1)
	{
		perror("listening failed");
		assert(ret != -1);
	}


	// to store the client address
	struct sockaddr_in client;
	socklen_t client_addrlength = sizeof(client);

	int connfd = -1;
	while(1)
	{
		// accept a client connection
		printf("waiting\n");
		connfd = accept(sockfd, (struct sockaddr *)&client, &client_addrlength);
		if(connfd < 0)
		{
			perror("connection failed");
		}
		else
		{
			char remote[INET_ADDRSTRLEN];
			printf("connected with ip: %s, and port %d\n", 
					inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), 
					ntohs(client.sin_port));

			process_connection(connfd);
		}
	}
	close(sockfd);
	return 0;
}

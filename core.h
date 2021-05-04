#ifndef HTTP_CORE_H_
#define HTTP_CORE_H_

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEBUG 1

#define CRLF "\r\n"

#define MAX_URI_LEN 1024
#define DEFAULT_RES_LEN 1024

extern const char *kHttpMethods[];

struct server_config {
  int fd;
  char *root;
};

struct http_request {
  int fd;
  char *request;  // points to request
  char *method;
  char *request_uri;
  char *query_string;
  char *http_version;
  char *content_length;
  char *content_type;
  char *path_info;  // for cgi
  char *authorization;
  char *body;
};

int parse_http_request(int client_fd, char *request, struct http_request *req);
int response_with_data(int client_fd, struct http_request *req);

#endif
#include "core.h"

extern struct server_config server_configuration;

const char *kHttpMethods[] = {
    "GET",
    "POST",
};

char *const kOkReponse = "HTTP/1.1 200 OK\r\n";
char *const kServer = "Server: TEAM DROP TABLE\r\n";
char *const kConnectionClose = "Connection: close\r\n";
char *const kContentTypeHtml = "Content-Type: text/html\r\n";

// parse_http_request()
//   parses http request and store the result in struct http_request
//   returns 0 if successful, -1 otherwise
int parse_http_request(int client_fd, char *request, struct http_request *req) {
  // could be implemented more in detail
  char *p;

  req->fd = client_fd;

  p = req->request = strdup(request);
  req->method = NULL;
  for (int i = 0; i < sizeof(kHttpMethods) / sizeof(kHttpMethods[0]); ++i) {
    if (!strncasecmp(request, kHttpMethods[i], strlen(kHttpMethods[i]))) {
      req->method = req->request;
      p += strlen(kHttpMethods[i]);
      break;
    }
  }

  if (!req->method) return -1;
  req->request_uri = ++p;

  if (p = strchr(p, ' '), !p) return -1;
  req->http_version = ++p;

  if (strncasecmp(req->http_version, "HTTP/1.1", strlen("HTTP/1.1"))) return -1;

  if (p = strstr(p, "\r\n\r\n"), !p) return -1;
  req->body = p + 4;

  return 0;
}

int response_with_data(int client_fd, struct http_request *req) {
  int is_cgi = 0;
  char buffer[128];
  char uri[MAX_URI_LEN];
  char *res;
  char *p;
  time_t rawtime;
  struct tm *timeinfo;

  strncpy(uri, server_configuration.root, sizeof(uri));
  if (!strcmp(req->request_uri, "/")) {
    strncat(uri, "/index.html", sizeof(uri));
  } else if (p = strrchr(req->request_uri, '.'), p) {
    // only supports html and cgi
    if (is_cgi = strcmp(p, ".html"), is_cgi && strcmp(p, ".cgi")) return -1;
    strncat(uri, req->request_uri, sizeof(uri));
  } else {
    return -1;
  }

  if (access(uri, R_OK | (is_cgi ? X_OK : 0)) < 0) return -1;

  res = (char *)malloc(DEFAULT_RES_LEN);
  if (!res) return -1;

  int fd;
  int len, header_len;
  struct stat sb;

  if (fd = open(uri, O_RDONLY), fd < 0) goto fd_fail;

  strcpy(res, kOkReponse);

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof(buffer), "Date: %a, %d %b %G %T %Z\r\n", timeinfo);
  strcat(res, buffer);

  strcat(res, kServer);

  if (fstat(fd, &sb) < 0) goto fail;
  sprintf(res + strlen(res), "Content-Length: %ld\r\n", sb.st_size);

  strcat(res, kConnectionClose);

  strcat(res, kContentTypeHtml);
  strcat(res, CRLF);

  header_len = strlen(res);
  if (header_len + sb.st_size >= DEFAULT_RES_LEN) {
    res = realloc(res, strlen(res) + sb.st_size + 1);
    if (!res) goto fail;
  }

  if (len = read(fd, res + header_len, sb.st_size), len < 0) goto fail;

  write(client_fd, res, header_len + len);
  close(fd);
  free(res);
  return 0;

fail:
  close(fd);
fd_fail:
  free(res);
  return -1;
}
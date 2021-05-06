#include "core.h"

extern struct server_config server_configuration;

const char *kHttpMethods[] = {
    "GET",
    "POST",
};

char *const kCgiGatewayInterface = "GATEWAY_INTERFACE=CGI/1.1";
char *const kCgiServerPort = "SERVER_PORT=8080";
char *const kCgiServerProtocol = "SERVER_PROTOCOL=HTTP/1.1";
char *const kCgiServerSoftware = "SERVER_SOFTWARE=TEAM DROP TABLE";

char *const kCgiRequestMethodGet = "REQUEST_METHOD=GET";
char *const kCgiRequestMethodPost = "REQUEST_METHOD=POST";

char *const kOkReponse = "HTTP/1.1 200 OK\r\n";
char *const kBadRequest = "HTTP/1.1 400 Bad Request\r\n";
char *const kNotFound = "HTTP/1.1 404 Not Found\r\n";
char *const kInternalServerError = "HTTP/1.1 500 Internal Server Error\r\n";

char *const kServer = "Server: TEAM DROP TABLE\r\n";
char *const kConnectionClose = "Connection: close\r\n";
char *const kContentTypeHtml = "Content-Type: text/html\r\n";

char *const kBadRequestHtml =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
    "<html>"
    "<head><title> 400 Bad Request</title></head><body><h1> Bad Request</h1>"
    "<p> Your browser sent a request that this server could not "
    "understand.<br/>"
    "</p><hr></body></html> ";

char *const kNotFoundHtml =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
    "<html>"
    "<head><title> 404 Not Found</title></head><body><h1> Not Found</h1>"
    "<p> The requested URL was not found on this server.</p><hr></body>"
    "</html>";

char *const kInternalServerErrorHtml =
    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">"
    "<html>"
    "<head><title> 500 Internal Server Error</title></head><body>"
    "<h1> Internal Server Error</h1>"
    "<p> Ther server encountered an internal error or misconfiguration and was "
    "unable to complete your request.</p><hr></body>"
    "</html>";

void exploit() { printf("[Team Drop Table] Dummy Function for PoC\n"); }

char *strcasestr(const char *s, const char *find) {
  char c, sc;
  size_t len;
  if ((c = *find++) != 0) {
    c = (char)tolower((unsigned char)c);
    len = strlen(find);
    do {
      do {
        if ((sc = *s++) == 0) return (NULL);
      } while ((char)tolower((unsigned char)sc) != c);
    } while (strncasecmp(s, find, len) != 0);
    s--;
  }
  return ((char *)s);
}

// from https://github.com/abejfehr/URLDecode/blob/master/urldecode.c
char *url_decode(const char *str) {
  int d = 0; /* whether or not the string is decoded */

  char *dStr = malloc(strlen(str) + 1);
  char eStr[] = "00"; /* for a hex code */

  strcpy(dStr, str);

  while (!d) {
    d = 1;
    int i; /* the counter for the string */

    for (i = 0; i < strlen(dStr); ++i) {
      if (dStr[i] == '%') {
        if (dStr[i + 1] == 0) return dStr;

        if (isxdigit(dStr[i + 1]) && isxdigit(dStr[i + 2])) {
          d = 0;

          /* combine the next to numbers into one */
          eStr[0] = dStr[i + 1];
          eStr[1] = dStr[i + 2];

          /* convert it to decimal */
          long int x = strtol(eStr, NULL, 16);

          /* remove the hex */
          memmove(&dStr[i + 1], &dStr[i + 3], strlen(&dStr[i + 3]) + 1);

          dStr[i] = x;
        }
      }
    }
  }

  return dStr;
}

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

  if (!strncmp(req->method, "POST", 4)) {
    req->content_length = strcasestr(req->request, "\r\nContent-Length: ");
    // if "\r\nContent-Length" comes after "\r\n\r\n", ignore it
    if (req->content_length) {
      req->content_length += 2;  // strlen("\r\n");
      if (req->content_length > p) req->content_length = NULL;
    }

    req->content_type = strcasestr(req->request, "\r\nContent-Type: ");
    if (req->content_type) {
      req->content_type += 2;
      if (req->content_type > p) req->content_type = NULL;
    }
  } else {
    req->content_type = req->content_length = NULL;
  }

  req->authorization = strcasestr(req->request, "\r\nAuthorization: ");
  if (req->authorization) {
    req->authorization += 2;
    if (req->authorization > p) req->authorization = NULL;
  }

  // insert NULL bytes between tokens
  p = req->request;
  while (p = strstr(p, "\r\n"), p) *p++ = '\x00';
  p = req->request;
  while (p = strchr(p, ' '), p) *p++ = '\x00';

  req->query_string = strchr(req->request_uri, '?');
  if (req->query_string) {
    *req->query_string++ = '\x00';
    req->query_string = url_decode(req->query_string);
    if (!req->query_string) return -1;
  }

  if (p = strstr(req->request_uri, ".cgi"), p) {
    p += 4;  // strlen(".cgi");
    if (*p == '/') {
      req->path_info = strdup(p);
      *p = '\x00';  // this makes req->request_uri end with ".cgi",
                    // which makes using it more easier in other funcs
    } else {
      req->path_info = NULL;
    }
  } else {
    req->path_info = NULL;
  }

  return 0;
}

char *read_cgi_output(int r_pipe, int *content_length) {
  char *cgi_out = (char *)malloc(DEFAULT_RES_LEN);
  if (!cgi_out) return NULL;

  int cgi_out_len = DEFAULT_RES_LEN;
  int len = 0;

  // MAYBE TODO: do we need url-encode here?
  while (read(r_pipe, cgi_out + len++, 1) > 0) {
    if (len >= cgi_out_len) {
      cgi_out = realloc(cgi_out, 2 * cgi_out_len);  // cgi_out buffer doubles
      cgi_out_len *= 2;

      if (!cgi_out) return NULL;
    }
  }

  cgi_out[--len] = '\x00';
  if (!strstr(cgi_out, "\r\n\r\n")) {
    free(cgi_out);
    return NULL;
  }

  // cgi output may include headers,
  // so we get the length of data only.
  *content_length = strlen(cgi_out);
  *content_length -= (strstr(cgi_out, "\r\n\r\n") - cgi_out) + 4;

  return cgi_out;
}

int get_cgi_environment_vars(struct http_request *req, char **envp) {
  struct sockaddr_in addr;
  socklen_t addr_size = sizeof(struct sockaddr_in);

  envp[kGatewayInterface] = kCgiGatewayInterface;

  envp[kScriptName] =
      (char *)malloc(strlen("SCRIPT_NAME=") + strlen(req->request_uri) + 1);
  if (!envp[kScriptName]) goto script_name_fail;
  sprintf(envp[kScriptName], "SCRIPT_NAME=%s", req->request_uri);

  if (req->path_info) {
    envp[kPathInfo] =
        (char *)malloc(strlen("PATH_INFO=") + strlen(req->path_info) + 1);
    if (!envp[kPathInfo]) goto path_info_fail;
    sprintf(envp[kPathInfo], "PATH_INFO=%s", req->path_info);
  } else {
    envp[kPathInfo] = strdup("PATH_INFO=");
    if (!envp[kPathInfo]) goto path_info_fail;
  }

  // MAYBE FIXME: dummy server name
  envp[kServerName] = strdup("SERVER_NAME=team.drop.table");
  if (!envp[kServerName]) goto server_name_fail;

  envp[kServerPort] = kCgiServerPort;
  envp[kServerProtocol] = kCgiServerProtocol;
  envp[kServerSoftware] = kCgiServerSoftware;
  envp[kRequestMethod] = !strcmp(req->method, "GET") ? kCgiRequestMethodGet
                                                     : kCgiRequestMethodPost;
  if (req->query_string) {
    envp[kQueryString] =
        (char *)malloc(strlen("QUERY_STRING=") + strlen(req->query_string) + 1);
    if (!envp[kQueryString]) goto query_string_fail;
    sprintf(envp[kQueryString], "QUERY_STRING=%s", req->query_string);
  } else {
    envp[kQueryString] = strdup("QUERY_STRING=");
    if (!envp[kQueryString]) goto query_string_fail;
  }

  if (getpeername(req->fd, (struct sockaddr *)&addr, &addr_size) < 0)
    goto remote_addr_fail;

  /// IP address is 15-digit at maximum (111.222.333.444)
  envp[kRemoteAddr] = (char *)malloc(strlen("REMOTE_ADDR=") + 16);
  if (!envp[kRemoteAddr]) goto remote_addr_fail;
  sprintf(envp[kRemoteAddr], "REMOTE_ADDR=%s", inet_ntoa(addr.sin_addr));

  // MAYBE FIXME: another option to "strlen(req->body)" is
  //   passing its value from "Content-Length" header from client request,
  //   then CONTENT_LENGTH will be more freely attacker-controllable,
  //   there could be some vulnerabilities..
  //
  //   currently we assume that content-length value doesn't exceed 9 digits
  envp[kContentLength] = (char *)malloc(strlen("CONTENT_LENGTH=") + 10);
  if (!envp[kContentLength]) goto content_length_fail;
  sprintf(envp[kContentLength], "CONTENT_LENGTH=%ld", strlen(req->body));

  if (req->content_type) {
    envp[kContentType] = (char *)malloc(strlen(req->content_type));
    if (!envp[kContentType]) goto content_type_fail;
    sprintf(envp[kContentType], "CONTENT_TYPE=%s",
            req->content_type + strlen("Content-Type: "));
  } else {
    envp[kContentType] = strdup("CONTENT_TYPE=");
    if (!envp[kContentType]) goto content_type_fail;
  }

  if (req->authorization) {
    envp[kAuthType] =
        (char *)malloc(strlen("AUTH_TYPE=") + strlen(req->authorization) + 1);
    if (!envp[kAuthType]) goto auth_type_fail;
    sprintf(envp[kAuthType], "AUTY_TYPE=");
    sscanf(req->authorization + strlen("Authorization: "), "%s",
           envp[kAuthType] + strlen("AUTH_TYPE="));
  } else {
    envp[kAuthType] = strdup("AUTH_TYPE=");
    if (!envp[kAuthType]) goto auth_type_fail;
  }

  envp[NUM_CGI_ENV_VARS - 1] = NULL;

  return 0;

// cascading error handling
auth_type_fail:
  free(envp[kContentType]);

content_type_fail:
  free(envp[kContentLength]);

content_length_fail:
  free(envp[kRemoteAddr]);

remote_addr_fail:
  free(envp[kQueryString]);

query_string_fail:
  free(envp[kServerName]);

server_name_fail:
  free(envp[kPathInfo]);

path_info_fail:
  free(envp[kScriptName]);

script_name_fail:
  return -1;
}

int response_bad_request(int client_fd) {
  time_t rawtime;
  struct tm *timeinfo;
  int header_len;
  char buffer[128];
  char *res = (char *)malloc(DEFAULT_RES_LEN);
  if (!res) {
    fprintf(stderr, "Couldn't handle error");
    exit(-1);
  }

  strcpy(res, kBadRequest);

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof(buffer), "Date: %a, %d %b %G %T %Z\r\n", timeinfo);
  strcat(res, buffer);

  strcat(res, kServer);

  sprintf(res + strlen(res), "Content-Length: %ld\r\n",
          strlen(kBadRequestHtml));

  strcat(res, kConnectionClose);

  strcat(res, kContentTypeHtml);
  strcat(res, CRLF);

  header_len = strlen(res);
  strcat(res, kBadRequestHtml);

  write(client_fd, res, strlen(res));
  free(res);
  return 0;
}

int response_not_found(int client_fd) {
  time_t rawtime;
  struct tm *timeinfo;
  int header_len;
  char buffer[128];
  char *res = (char *)malloc(DEFAULT_RES_LEN);
  if (!res) {
    fprintf(stderr, "Couldn't handle error");
    exit(-1);
  }

  strcpy(res, kNotFound);

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof(buffer), "Date: %a, %d %b %G %T %Z\r\n", timeinfo);
  strcat(res, buffer);

  strcat(res, kServer);

  sprintf(res + strlen(res), "Content-Length: %ld\r\n", strlen(kNotFoundHtml));

  strcat(res, kConnectionClose);

  strcat(res, kContentTypeHtml);
  strcat(res, CRLF);

  header_len = strlen(res);
  strcat(res, kNotFoundHtml);

  write(client_fd, res, strlen(res));
  free(res);
  return 0;
}

int response_internal_server_error(int client_fd) {
  time_t rawtime;
  struct tm *timeinfo;
  int header_len;
  char buffer[128];
  char *res = (char *)malloc(DEFAULT_RES_LEN);
  if (!res) {
    fprintf(stderr, "Couldn't handle error");
    exit(-1);
  }

  strcpy(res, kInternalServerError);

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer, sizeof(buffer), "Date: %a, %d %b %G %T %Z\r\n", timeinfo);
  strcat(res, buffer);

  strcat(res, kServer);

  sprintf(res + strlen(res), "Content-Length: %ld\r\n",
          strlen(kInternalServerErrorHtml));

  strcat(res, kConnectionClose);

  strcat(res, kContentTypeHtml);
  strcat(res, CRLF);

  header_len = strlen(res);
  strcat(res, kInternalServerErrorHtml);

  write(client_fd, res, strlen(res));
  free(res);
  return 0;
}

int response_with_data(int client_fd, struct http_request *req) {
  int is_cgi = 0;
  char buffer[128];  // fixed size is safe because buffer is only filled with
                     // server-side data.
  char uri[MAX_URI_LEN];  // MAYBE FIXME: fixed size
                          // but already safe now,
                          // current all writes to uri use strncpy() and
                          // strncat().
  char *res;
  char *p;
  time_t rawtime;
  struct tm *timeinfo;

  strncpy(uri, server_configuration.root, sizeof(uri));
  if (!strcmp(req->request_uri, "/")) {
    strncat(uri, "/index.html", sizeof(uri));
  } else if (p = strrchr(req->request_uri, '.'), p) {
    // only supports html and cgi
    is_cgi = !strcmp(p, ".cgi");
    strcat(uri, req->request_uri);
  } else {
    return response_bad_request(client_fd);
  }

  if (access(uri, R_OK | (is_cgi ? X_OK : 0)) < 0)
    return response_not_found(client_fd);

  res = (char *)malloc(DEFAULT_RES_LEN);
  if (!res) return response_internal_server_error(client_fd);

  if (is_cgi) {
    int in_pipe[2];
    int out_pipe[2];

    if (pipe(in_pipe) < 0) return response_internal_server_error(client_fd);
    if (pipe(out_pipe) < 0) return response_internal_server_error(client_fd);

    strcpy(res, kOkReponse);

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "Date: %a, %d %b %G %T %Z\r\n", timeinfo);
    strcat(res, buffer);

    strcat(res, kServer);

    strcat(res, kConnectionClose);

    write(in_pipe[1], req->body, strlen(req->body));
    close(in_pipe[1]);

    if (!fork()) {
      // child process
      // set file descriptor and execute scripts
      char *argv[] = {uri, NULL};
      char *envp[NUM_CGI_ENV_VARS];

      close(in_pipe[1]);
      close(out_pipe[0]);
      dup2(in_pipe[0], STDIN_FILENO);
      dup2(out_pipe[1], STDOUT_FILENO);

      if (get_cgi_environment_vars(req, envp) < 0) exit(-1);

#if DEBUG
      for (int i = 0; i < NUM_CGI_ENV_VARS - 1; ++i)
        fprintf(stderr, "[DEBUG] %s\n", envp[i]);
#endif

      execve(uri, argv, envp);

      exit(-1);  // if this statement is executed, it means there's an error
    }

    // parent process
    // wait for child and response to client's request
    close(in_pipe[0]);
    close(out_pipe[1]);
    wait(NULL);

    int content_length;
    char *cgi_out = read_cgi_output(out_pipe[0], &content_length);
    if (!cgi_out) return response_internal_server_error(client_fd);

    sprintf(res + strlen(res), "Content-Length: %d\r\n", content_length);

    write(client_fd, res, strlen(res));
    write(client_fd, cgi_out, strlen(cgi_out));

    free(res);
    free(cgi_out);

    return 0;
  } else {  // pure html mode doesn't parse query strings and post data.
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
    return response_internal_server_error(client_fd);
  }
}

#include <arpa/inet.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "core.h"

#define MAX_LISTEN_QUEUE 5
#define REQ_BUF_SIZE 1024

struct server_config server_configuration;

void handle_error(char *msg) {
  fprintf(stderr, "%s", msg);
  exit(-1);
}

void interrupt_handler(int x) {
  close(server_configuration.fd);
  wait(NULL);
  exit(-1);
}

int main(int argc, char **argv) {
  int server_fd, client_fd;
  struct sockaddr_in server, client;
  char request_buf[REQ_BUF_SIZE];  // FIXME: fixed size
  int c, res;
  struct http_request user_request;
  DIR *root_dir;

  if (argc < 2) handle_error("usage: ./server {root_directory}\n");

  if (signal(SIGINT, interrupt_handler) == SIG_ERR)
    handle_error("signal() failed.\n");

  server_configuration.fd = server_fd = socket(AF_INET, SOCK_STREAM, 0);
  server_configuration.root = argv[1];
  if (server_fd < 0) handle_error("socket() failed.\n");
  if (root_dir = opendir(argv[1]), !root_dir)
    handle_error("root directory does not exist.\n");

  closedir(root_dir);
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(8080);

  if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    handle_error("bind() failed.\n");

  if (listen(server_fd, MAX_LISTEN_QUEUE) < 0)
    handle_error("listen() failed.\n");

  // infinite loop for accepting requests
  while (1) {
    c = sizeof(struct sockaddr_in);
    if (client_fd =
            accept(server_fd, (struct sockaddr *)&client, (socklen_t *)&c),
        client_fd < 0)
      handle_error("bind() failed.\n");

    if (fork()) {
      close(client_fd);
      continue;
    } else {
      break;
    }
  }

  // child process, handle requests
  if (res = read(client_fd, request_buf, REQ_BUF_SIZE), res < 0) goto done;

  request_buf[res] = '\x00';

#if DEBUG
  printf("\n[DEBUG] Received:\n%s\n---\n", request_buf);
#endif

  if (res = parse_http_request(client_fd, request_buf, &user_request),
      res < 0) {
    response_bad_request(client_fd);
    goto done;
  }

#if DEBUG
  fprintf(stderr, "[DEBUG] request: %s\n", user_request.request);
  fprintf(stderr, "[DEBUG] method: %s\n", user_request.method);
  fprintf(stderr, "[DEBUG] request_uri: %s\n", user_request.request_uri);
  if (user_request.query_string)
    fprintf(stderr, "[DEBUG] query_string: %s\n", user_request.query_string);
  if (user_request.content_length)
    fprintf(stderr, "[DEBUG] content_length: %s\n",
            user_request.content_length);
  if (user_request.content_type)
    fprintf(stderr, "[DEBUG] content_type: %s\n", user_request.content_type);
  if (user_request.path_info)
    fprintf(stderr, "[DEBUG] path_info: %s\n", user_request.path_info);
  if (user_request.authorization)
    fprintf(stderr, "[DEBUG] authorization %s\n", user_request.authorization);
  fprintf(stderr, "[DEBUG] http_version: %s\n", user_request.http_version);
  fprintf(stderr, "[DEBUG] body: %s\n", user_request.body);
#endif

  if (res = response_with_data(client_fd, &user_request), res < 0) goto done;

done:
  free(user_request.request);
  close(server_configuration.fd);
  close(client_fd);
  return 0;
}

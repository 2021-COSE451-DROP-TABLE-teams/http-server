#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEBUG

#define TRUE 1
#define FALSE 0

void exploit() { printf("[Team Drop Table] Dummy Function for PoC\n"); }

int send_tsv();
int update_tsv();
int get_next_id();

int is_symlink(char* filename) {
  struct stat p_statbuf;
  // if error, just abort
  if (lstat(filename, &p_statbuf) < 0) return 1;
  return S_ISLNK(p_statbuf.st_mode) == 1;
};

int main() {
  // determine request type
  const char* req_method = getenv("REQUEST_METHOD");

  // respond to GET or POST
  if (strcmp(req_method, "GET") == 0) {
    return send_tsv();
  } else if (strcmp(req_method, "POST") == 0) {
    return update_tsv();
  } else {
    fprintf(stderr, "Unsupported/Unrecognised HTML verb.\n");
    return 0;
  }
}

int send_tsv() {
  // begin output
  printf("Content-type: text/tab-separated-values\r\n\r\n");

  // obtain board name from environment variable
  const char* query_string = getenv("QUERY_STRING");
  if (strcmp(query_string, "") == 0) {
    fprintf(stderr, "Empty query string.\n");
    return 0;
  }

  // extract board name from query string
  char* pboard = strstr(query_string, "board=");
  if (!pboard) {
    fprintf(stderr, "no board parameter");
    return 0;
  }
  char* token = strtok(pboard, "=");
  char* board_name = strtok(NULL, "&");
  if (strchr(board_name, '/') || strchr(board_name, '.')) {
    fprintf(stderr, "bad board name");
    return 0;
  }

  // generate file name and open TSV file
  char* filename = strcat(board_name, ".tsv");
  FILE* fp = fopen(filename, "r");

  // generate table if file does not exist
  if (fp == NULL) {
    fprintf(stderr, "File %s does not exist.\n", filename);
    fp = fopen(filename, "w");
    fprintf(fp, "%u\t%s\t%d\t%d\t%s\t%s\n", 0, "hash", 0, 0, "username",
            "message");
    fclose(fp);
    return 0;
  } else if (is_symlink(filename)) {
    fprintf(stderr, "invalid file");
    return 0;
  }

  char hash[52];
  unsigned int unix_time;
  int post_id;
  int visible;
  char author[52];
  char* message = (char*)malloc(1000);
  if (!message) {
    fprintf(stderr, "failed to allocate message.\n");
    return 0;
  }

  while (fscanf(fp, "%u\t%s\t%d\t%d\t%s\t", &unix_time, hash, &post_id,
                &visible, author) == 5) {
    fgets(message, 1000, fp);
    if (visible == 0) continue;

    printf("%u\t%d\t%s\t%s", unix_time, post_id, author, message);
  }

  free(message);
  fclose(fp);
  return 0;
}

#define MAX_NUM_PARAMS 8

char** parse_parameters(const char* params) {
  int num_params = 0;
  char** parsed = (char**)malloc(sizeof(char*) * (MAX_NUM_PARAMS + 1));
  if (!parsed) return NULL;

  char* p = strdup(params);
  if (!p) {
    free(parsed);
    return NULL;
  }

  // split into token with "&";
  parsed[num_params++] = strtok(p, "&");
  while (p = strtok(NULL, "&"), num_params < MAX_NUM_PARAMS && p)
    parsed[num_params++] = p;

  parsed[num_params] = NULL;  // end of parsed parameters
  return parsed;
}

char* get_param(char** params, char* key) {
  for (int i = 0; params[i]; ++i) {
    if (!strstr(params[i], "=")) continue;
    if (strncmp(params[i], key, strlen(key))) continue;

    char* name = strtok(params[i], "=");
    char* value = strtok(NULL, "=");
    return value;
  }

  return NULL;
}

int filter(char* str) {
  if (strchr(str, '\t')) return -1;
  if (strchr(str, '\n')) return -1;
  return 0;
}

int update_tsv() {
  printf("Content-type: application/json\r\n\r\n");

#define error_response(msg)             \
  {                                     \
    fprintf(stderr, msg);               \
    printf("{ \"result\": \"error\"}"); \
  }

  // obtain board name from environment variable
  const char* query_string = getenv("QUERY_STRING");
  if (strcmp(query_string, "") == 0) {
    error_response("Empty query string.\n");
    return 0;
  }

  // extract board name from query string
  char** parsed_query_string = parse_parameters(query_string);
  char* board_name = get_param(parsed_query_string, "board");
  if (!board_name) {
    error_response("no board parameter");
    return 0;
  }

  // check if board exists
  char* filename = malloc(strlen(board_name) + strlen(".tsv") + 1);
  if (!filename) {
    error_response("memory allocation failed.");
    return 0;
  }
  strcpy(filename, board_name);
  strcat(filename, ".tsv");
  if (access(filename, R_OK) < 0) {
    error_response("board doesn't exist");
    return 0;
  }

  if (is_symlink(filename)) {
    error_response("invalid file");
    return 0;
  }

  // check operation type
  char* op = get_param(parsed_query_string, "operation");
  if (!op) {
    error_response("no operation parameter");
    return 0;
  }

  int do_create;
  if (strcmp(op, "create") == 0) {
    do_create = TRUE;
  } else if (strcmp(op, "delete") == 0) {
    do_create = FALSE;
  } else {
    error_response("Unrecognised operation.");
    return 0;
  }

  // free query_string memory
  free(*parsed_query_string);
  free(parsed_query_string);

  // extract parameters from stdin
  int content_len = atoi(getenv("CONTENT_LENGTH"));
  char buffer[1024] = {
      0,
  };
  read(0, buffer, content_len);

  // extract relevant fields
  char** parsed_parameters = parse_parameters(buffer);

  char* message;
  char* id;
  char* password = get_param(parsed_parameters, "password");
  if (!password) {
    error_response("no password parameter");
    return 0;
  }
  if (filter(password) < 0) {
    error_response("bad password parameter");
    return 0;
  }
  if (strlen(password) > 50) {
    error_response("too long password");
    return 0;
  }
#ifdef DEBUG
  fprintf(stderr, "[DEBUG] password: %s\n", password);
#endif

  char* author = get_param(parsed_parameters, "name");
  if (!author) {
    error_response("no author parameter");
    return 0;
  }
  if (filter(author) < 0) {
    error_response("bad author parameter");
    return 0;
  }
  if (strlen(author) > 150) {
    error_response("too long author");
    return 0;
  }
#ifdef DEBUG
  fprintf(stderr, "[DEBUG] author: %s\n", author);
#endif

  if (do_create) {
    message = get_param(parsed_parameters, "comment");
    if (!message) {
      error_response("no message parameter");
      return 0;
    }
    if (filter(message) < 0) {
      error_response("bad message parameter");
      return 0;
    }
    if (strlen(message) > 500) {
      error_response("too long message");
      return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] message: %s\n", message);
#endif
  } else {
    id = get_param(parsed_parameters, "post-id");
    if (!id) {
      error_response("no message parameter");
      return 0;
    }
    if (filter(id) < 0) {
      error_response("bad id parameter");
      return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] post-id: %s\n", id);
#endif
  }

  if (do_create) {  // create operation
    // generate the rest of the message fields
    unsigned int unix_time = (unsigned)time(NULL);
    int post_id = get_next_id(filename);
    int visible = 1;

    // append message to file
    FILE* fp = fopen(filename, "a+");
    fprintf(fp, "%u\t%s\t%d\t%d\t%s\t%s\n", unix_time, password, post_id,
            visible, author, message);  // TODO: hash
    printf("{ \"result\": \"success\"}");
    free(*parsed_parameters);
    free(parsed_parameters);
    fclose(fp);
    return 0;
  } else {  // delete operation
    FILE* fp = fopen(filename, "r+");
    char buf[100];

    while (!feof(fp)) {
      fgets(buf, sizeof(buf), fp);

      char* unix_time = strtok(buf, "\t");
      char* hash = strtok(NULL, "\t");
      char* post_id = strtok(NULL, "\t");
      char* visible = strtok(NULL, "\t");
      char* username = strtok(NULL, "\t");
      char* message = strtok(NULL, "\t");

      if (strcmp(post_id, id) != 0) continue;  // id check
      if (strcmp(hash, password)) continue;    // hash check

      fseek(fp, -(strlen(message) + strlen(username) + strlen(visible) + 2),
            SEEK_CUR);
      fputc('0', fp);
      break;
    }

    printf("{ \"result\": \"success\"}");
    free(*parsed_parameters);
    free(parsed_parameters);
    fclose(fp);
    return 0;
  }
}

int get_next_id(char* filename) {
  FILE* p_file = fopen(filename, "r");
  char buffer[1024];
  int next_id = -1;

  while (!feof(p_file)) {
    fgets(buffer, 1024, p_file);
    if (strstr(buffer, "\n")) {
      next_id++;
    }
  }

  fclose(p_file);
  return next_id;
}
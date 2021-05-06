#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEBUG

#define TRUE 1
#define FALSE 0

int send_tsv();
int update_tsv();
int get_next_id();

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
  char* token = strtok(pboard, "=");
  char* board_name = strtok(NULL, "&");

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
  }

  char hash[256];
  unsigned int unix_time;
  int post_id;
  int visible;
  char author[100];
  char message[1000];

  while (fscanf(fp, "%u\t%s\t%d\t%d\t%s\t", &unix_time, hash, &post_id,
                &visible, author) == 5) {
    fgets(message, 1000, fp);
    if (visible == 0) continue;

    printf("%u\t%d\t%s\t%s", unix_time, post_id, author, message);
  }

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
  char* filename = strcat(strdup(board_name), ".tsv");
  if (access(filename, R_OK) < 0) {
    error_response("board doesn't exist");
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
  char* buffer = (char*)calloc(content_len + 1, sizeof(char));
  if (!buffer) {
    error_response("Failed to allocate memory.");
    return 0;
  }
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
#ifdef DEBUG
  fprintf(stderr, "[DEBUG] password: %s\n", password);
#endif

  char* author = get_param(parsed_parameters, "name");
  if (!author) {
    error_response("no author parameter");
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
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] message: %s\n", message);
#endif
  } else {
    id = get_param(parsed_parameters, "post-id");
    if (!id) {
      error_response("no message parameter");
      return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "[DEBUG] post-id: %s\n", id);
#endif
  }

  free(buffer);

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
    fclose(fp);
    return 0;
  } else {  // delete operation
    FILE* fp = fopen(filename, "r+");
    char bf[100];
    int line = 0;
    char* line_ptr;

    while (!feof(fp)) {
      fgets(bf, sizeof(bf), fp);
      line_ptr = strstr(bf, password);  // TODO: hash

      if (line_ptr != NULL) {
        char* hash = strtok(line_ptr, "\t");
        char* post_id = strtok(NULL, "\t");
        if (strcmp(post_id, id) != 0) {
          continue;
        }
        char* visible = strtok(NULL, "\t");
        char* username = strtok(NULL, "\t");
        char* message = strtok(NULL, "\t");

        // TODO: find the right offset
        fseek(fp, -(strlen(message) + strlen(username) + strlen(visible) + 3),
              SEEK_CUR);
        fputc('0', fp);
        break;
      }
      line++;
    }

    printf("{ \"result\": \"success\"}");
    fclose(fp);
    return 0;
  }
}

int get_next_id(char* filename) {
  FILE* p_file = fopen(filename, "r");
  char buffer[100];
  int next_id = -1;

  while (!feof(p_file)) {
    fgets(buffer, 100, p_file);
    if (strstr(buffer, "\n")) {
      next_id++;
    }
  }

  fclose(p_file);
  return next_id;
}
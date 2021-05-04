#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

int send_tsv();
int update_tsv();

int main() {
	
	//determine request type
	const char* req_method = getenv("REQUEST_METHOD");
	
	//respond to GET or POST
	if (strcmp(req_method, "GET") == 0) {
		return send_tsv();
	}
	else if (strcmp(req_method, "POST") == 0) {
		return update_tsv();
	}
	else {
		fprintf(stderr, "Unsupported/Unrecognised HTML verb.\n");
	}
	
	return 0;
}


int send_tsv() {

	//obtain board name from environment variable
	const char* query_string = getenv("QUERY_STRING");

	if (strcmp(query_string, "") == 0) {
		fprintf(stderr, "Empty query string.\n");
		return 0;
   	}

	//extract board name from query string
	char* pboard = strstr(query_string, "board=");
	char* token = strtok(pboard, "=");
	char* board_name = strtok(NULL, "&");
	
	//begin output
	printf("Content-type: text/tab-separated-values\r\n\r\n");

	//generate file name and open TSV file
	char* filename = strcat(board_name, ".tsv");
	FILE* fp = fopen(filename, "r");
	
	// generate table if file does not exist
	if (fp == NULL){
		fprintf(stderr, "File %s does not exist.\n", filename);
		fp = fopen(filename, "w");
		fprintf(fp, "%s\t%u\t%d\t%d\t%s\t%s\n",
					"hash", 0, 0, 0, "username", "message");
	}

	// read from TSV file
	fp = fopen(filename, "r");

	char hash[256];
	unsigned int unix_time;
	int post_id;
	int visible;
	char author[100];
	char message[1000];

	while(fscanf(fp, "%s\t%u\t%d\t%d%s\t",
			hash, &unix_time, &post_id, &visible, author) == 5) {

		fgets(message, 1000, fp);
		if (visible == 0)
			continue;

		printf("%u\t%d\t%s\t%s", unix_time, post_id, author, message);
	}

	fclose(fp);
	return 0;
}

int update_tsv() {
	return 1;
}

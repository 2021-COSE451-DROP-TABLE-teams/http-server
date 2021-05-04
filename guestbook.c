#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

int main() {

	//obtain board name from environment variable
	const char *query_string = getenv("QUERY_STRING");

	if (query_string == NULL) {
		printf("Variable QUERY_STRING does not exist.\n");
		return 0;
   	}
	
	//extract board name from query stringa
	char *pboard = strstr(query_string, "board=");
	char *token = strtok(pboard, "=");
	char *board_name = strtok(NULL, "&");
	
	//begin output
	printf("Content-type: text/html\r\n\r\n");

	//generate file name and open TSV file
	char * filename = strcat(board_name, ".tsv");
	FILE* fp = fopen(filename, "r");
	
	// TODO: generate table if file does not exist
	if(fp == NULL){
		printf("File %s does not exist.\n", filename);
		fp = fopen(filename, "w");
		fprintf(fp, "%u\t%d\t%s\t%s\n", (unsigned)time(NULL), 0, "drop-table", "hello world!");
	}


	// read from TSV file
	int unix_time;
	int post_id;
	char author[100];
	char message[1000];

	while(fscanf(fp, "%u\t%d\t%s\t", &unix_time, &post_id, author) == 3) {
		fgets(message, 1000, fp);
		printf("%u\t%d\t%s\t%s", unix_time, post_id, author, message);
	}

	fclose(fp);
	return 0;
}




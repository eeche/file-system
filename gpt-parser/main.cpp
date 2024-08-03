#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define SECTOR 512

void usage() {
	printf("syntas: gpt-parser <file>\n");
	printf("sample: mbr-parser a.dd\n");
}

void gpt_parser() {


}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    gpt_parser();
    return 0;
}
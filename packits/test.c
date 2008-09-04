#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "packits.h"

int main(int argc, char *argv[])
{
	struct packit tp;
	struct packit_record *r;

	memset(&tp, 0, sizeof(tp));

	packit_add_uint_header(&tp, CLENGTH_KEY, 0);
	packit_add_header(&tp, "Hello", "World");
	packit_add_header(&tp, "Test", "Program");
	packit_add_uint_header(&tp, "UINT", 101);
	packit_add_int_header(&tp, "INT", -12);

	/* print all records */
	forall_packit_headers(&tp, r) {
		printf("%s => %s\n", r->key, r->val);
	}
	printf("\n");

	/* test modifying record */
	packit_add_uint_header(&tp, CLENGTH_KEY, 10);

	/* test getting records */
	r = packit_get_header(&tp, "Hello");
	if(r)
		printf("%s => %s\n", r->key, r->val);
	r = packit_get_header(&tp, "Test");
	if(r)
		printf("%s => %s\n", r->key, r->val);
	r = packit_get_header(&tp, CLENGTH_KEY);
	if(r)
		printf("%s => %s\n", r->key, r->val);
	r = packit_get_header(&tp, "Blah");
	if(r)
		printf("%s => %s\n", r->key, r->val);

	packit_send(&tp);

	/* print all records */
	forall_packit_headers(&tp, r) {
		int i;
		for(i=0;i < (strlen(r->key) + strlen(r->val) + 2); i++) {
			if(r->rec[i] == '\0')
				printf("<NULL>");
			else
				printf("%c",r->rec[i]);
		}
		printf("\n");
	}

	return 0;
}

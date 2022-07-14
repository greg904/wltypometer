#include <stdlib.h>

#include "args.h"

uint32_t args_x;
uint32_t args_y;
uint32_t args_width;
uint32_t args_height;

static int pair(const char *str, char sep, uint32_t *first, uint32_t *second)
{
	char *end;
	long num;

	num = strtol(str, &end, 10);
	if (num > UINT32_MAX)
		return -1;

	*first = (uint32_t)num;

	if (*end != sep)
		return -1;

	num = strtol(end + 1, &end, 10);
	if (num > UINT32_MAX)
		return -1;

	*second = (uint32_t)num;

	if (*end != '\0')
		return -1;

	return 0;
}

int args_parse(int argc, char **argv)
{
	if (argc != 3)
		return -1;

	if (pair(argv[1], ',', &args_x, &args_y) == -1 ||
	    pair(argv[2], 'x', &args_width, &args_height) == -1)
		return -1;

	return 0;
}

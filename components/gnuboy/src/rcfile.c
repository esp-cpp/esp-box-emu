
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gnuboy/gnuboy.h"
#include "gnuboy/defs.h"
#include "gnuboy/rc.h"
#include "gnuboy/hw.h"


char *rcpath;

int rc_sourcefile(char *filename)
{
	FILE *f;
	char *name;
	char line[256], *p;

	name = path_search(filename, "r", rcpath);
	f = fopen(name, "r");
	if (!f) return -1;

	for (;;)
	{
		if (feof(f)) break;
		fgets(line, sizeof line, f);
		if ((p = strpbrk(line, "#\r\n")))
			*p = 0;
		rc_command(line);
	}
	fclose(f);
	return 0;
}


rcvar_t rcfile_exports[] =
{
	RCV_STRING("rcpath", &rcpath),
	RCV_END
};


#include <u.h>
#include <libc.h>
#include <String.h>
#include <thread.h>

#include "html5dom.h"

static char *drpath = "/mnt/dom";
static char *tpath = nil;

void
usage(void)
{
	fprint(2, "usage: %s [-m /mnt/dom] [-n 123]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	Dir *d;
	ARGBEGIN{
	case 'm':
		drpath = EARGF(usage());
		break;
	case 'n':
		tpath = EARGF(usage());
	default:
		usage();
	} ARGEND;
	if (argc != 0) usage();
	
	d = dirstat(drpath);
	if (d==nil) sysfatal("%r");
	if ((d->mode & DMDIR) == 0) sysfatal("%s - not a directory", drpath);
	if (chdir(drpath) != 0) sysfatal("can't chdir to %s, %r", drpath);
	if (tpath == nil) {
		char *buf[128];
		long n;
		int fd;
		fd = open("new", OREAD);
		if (fd < 0) sysfatal("can't open %s/new. %r", drpath);
		n = read(fd, buf, 128);
		if (n <= 0) sysfatal("failed to read from %s/new. %r", drpath);
		tpath = mallocz(n+1, 1);
		memmove(tpath, buf, n);
		if (tpath[n-1] == '\n') tpath[n-1] = '\0';
		close(fd);
	}
	if (chdir(tpath) != 0) sysfatal("can't chdir to %s, %r", tpath);
	
	Tokctl *tc;
	Treeconstrctl *trc;
	tc = malloc(sizeof(Tokctl));
	tc->c = chancreate(sizeof(Token*), 1024);
	trc = malloc(sizeof(Treeconstrctl));
	trc->treeroot = ".";
	trc->in = tc->c;


	threadcreate(threadtokenize, tc, 64 * 1024);
	threadcreate(threadtreeconstr, trc, 64 * 1024);
}

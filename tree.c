#include <u.h>
#include <libc.h>
#include <String.h>
#include <thread.h>

#include "html5dom.h"

Treeconstrctl *tctl;
String *tstr;
char *tnode;

void
nwrite(char *strnode, char *strfile, char *data, long n)
{
	int fd;
	char *path;
	path = smprint("%s/%s/%s", tctl->treeroot, strnode, strfile);
	fd = create(path, OWRITE, 0);
	if (fd < 0) sysfatal("failed to create %s, %r", path);
	free(path);
	write(fd, data, n);
	write(fd, "\n", 1);
	close(fd);
}

char*
newnode(void)
{
	int fd;
	long n;
	char *strnew, *strnode;
	strnode = mallocz(64, 1);
	strnew = smprint("%s/new", tctl->treeroot);
	fd = open(strnew, OREAD);
	if (fd < 0) sysfatal("failed to open %s, %r", strnew);
	free(strnew);
	n = read(fd, strnode, 64);
	close(fd);
	if (strnode[n-1] == '\n') strnode[n-1] = '\0';
	return strnode;
}

void
pushchar(Rune c)
{
	if (tnode == nil) {
		tnode = newnode();
		nwrite(tnode, "type", "text", 4);
		tstr = s_new();
	}
	s_putc(tstr, c);	
}

void
pushtext(void)
{
	s_terminate(tstr);
	nwrite(tnode, "text", s_to_c(tstr), strlen(s_to_c(tstr)));
	s_free(tstr);
	tstr = nil;
	tnode = nil;
}

void
threadtreeconstr(void *v)
{
	char *strnode;
	int teof;
	Token *tok;
	teof = 0;
	tctl = v;
	tok = nil;
	threadsetname("treeconstr");
	while(teof == 0){
		recv(tctl->in, &tok);
		switch(tok->type){
		case TDOCT:
			strnode = newnode();
			nwrite(strnode, "type", "doctype", 7);
			nwrite(strnode, "name", s_to_c(tok->name),
				strlen(s_to_c(tok->name)));
			free(strnode);
			break;
		case TSTART:
			if (tnode != nil) pushtext();
			strnode = newnode();
			nwrite(strnode, "type", "element", 7);
			nwrite(strnode, "name", s_to_c(tok->name),
				strlen(s_to_c(tok->name)));
			free(strnode);
			break;
		case TEND:
			if (tnode != nil) pushtext();
			break;
		case TCHAR:
			pushchar(tok->c);
			break;
		case TEOF:
			teof = 1;
		}
		t_free(tok);
	}
}

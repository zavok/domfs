#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

static long qcount;

typedef struct Stack Stack;
typedef struct Dnode Dnode;

struct Stack {
	long length;
	Dnode *list;
};

struct Dnode {
	long id;
	long parent;
	Stack children;
	char *type;
	char *attr;
	char *text;
	Qid q;
};

void
stackinit(Stack *S)
{
	S->length = 0;
	S->list = nil;
	return;
}

Dnode *
stackpush(Stack *stack, Dnode *new)
{
	stack->list = realloc(stack->list, sizeof(Dnode) * (stack->length + 1));
	stack->list[stack->length] = *new;
	stack->length++;
	return &stack->list[stack->length - 1];
}


static Stack nodes;
static long ncount;

long
newnode(long parent)
{
	Dnode *ref;
	Dnode new = {
		.id = ncount,
		.parent = parent,
		.type = nil,
		.attr = nil,
		.text = nil,
		.q = (Qid){qcount, 0, QTDIR},
	};
	stackinit(&new.children);
	ncount++;
	qcount++;
	ref = stackpush(&nodes, &new);
	stackpush(&nodes.list[parent].children, ref);
	return ref->id;
}

Dnode *
getnode(long n)
{
	Dnode *np;
	for (np = nodes.list; np < nodes.list + nodes.length; np++)
		if (np->id == n) return np;
	return nil;
}

void
fsattach(Req *r)
{
	r->fid->qid = getnode(0)->q;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

int
dirgen(int n, Dir *dir, void* aux)
{
	Dnode *N;
	if (aux == nil) return -1;
	N = aux;
	// TODO: do it properly

	if (n >= N->children.length) return -1;

	dir->uid = strdup("domfs");
	dir->gid = strdup("domfs");
	dir->name = smprint("%ld", N->children.list[n].id);
	dir->mode = 0555|DMDIR;
	dir->qid = getnode(n)->q;

	return 0;
}

void
fsread(Req *r)
{
	dirread9p(r, dirgen, getnode(0));
	respond(r, nil);
}

char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	fprint(2, "fswalk1 fid->qid.path=%ulld name=%s\n", fid->qid.path, name);

	*qid = (Qid){0, 0, QTDIR};
	fid->qid = *qid;
	return nil;
}

void
fsstat(Req *r)
{
	nulldir(&r->d);
	r->d.type = L'M';
	r->d.dev = 1;
	r->d.length = 0;
	r->d.muid = strdup("");
	r->d.atime = time(0);
	r->d.mtime = time(0);
	r->d.uid = strdup("domfs");
	r->d.gid = strdup("domfs");
	switch(r->fid->qid.path){
	case 0:
		r->d.qid = (Qid){0, 0, QTDIR};
		r->d.name = strdup("/");
		break;
	default:
		r->d.qid = (Qid){r->fid->qid.path, 0, QTDIR};
		r->d.name = smprint("%ulld", r->fid->qid.path);
	};
	r->d.mode = 0777|DMDIR;
	respond(r, nil);
}

void
usage(void)
{
	fprint(2, "usage %s [-D][-m /n/dom][-s service]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *srv, *mtpt;
	srv = nil;
	mtpt = "/n/dom";

	ARGBEGIN {
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srv = EARGF(usage());
		break;
	case 'D':
		chatty9p++;
		break;
	default:
		usage();
	} ARGEND
	
	stackinit(&nodes);
	getnode(newnode(0))->type = "domroot";
	getnode(newnode(0))->type = "A";
	getnode(newnode(0))->type = "B";

	Srv fs = {
		.attach = fsattach,
		.read = fsread,
		.walk1 = fswalk1,
		.stat = fsstat,
	};

	postmountsrv(&fs, srv, mtpt, MREPL);
}

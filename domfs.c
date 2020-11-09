#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>


typedef struct Stack Stack;
typedef struct Dnode Dnode;

struct Stack {
	long length;
	Dnode **list;
};

struct Dnode {
	long id;
	Dnode *parent;
};

static Stack nodes;
static long qcount;
static long ncount;

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
	stack->list = realloc(stack->list, sizeof(Dnode*) * (stack->length + 1));
	stack->list[stack->length] = new;
	stack->length++;
	return stack->list[stack->length - 1];
}


long
newnode(Dnode *parent)
{
	Dnode *ref;
	Dnode *new;
	new = malloc(sizeof(Dnode));

	new->id = ncount,
	new->parent = parent,
	new->type = nil,
	new->attr = nil,
	new->text = nil,
	new->q = (Qid){qcount, 0, QTDIR},

	stackinit(&new->children);
	ncount++;
	qcount += 0x10;
	ref = stackpush(&nodes, new);
	stackpush(&parent->children, ref);
	return ref->id;
}

Dnode *
getnode(long n)
{
	Dnode **np;
	for (np = nodes.list; np < nodes.list + nodes.length; np++) {
		if ((*np)->id == n) {
			return *np;
		}
	}
	return nil;
}

void
fsattach(Req *r)
{
	r->fid->qid = getnode(0)->q;
	r->fid->aux = getnode(0);
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
	dir->name = smprint("%ld", N->children.list[n]->id);
	dir->mode = 0555|DMDIR;
	dir->qid = getnode(n)->q;

	return 0;
}

void
fsread(Req *r)
{
	Dnode *node;
	node = r->fid->aux;
	dirread9p(r, dirgen, node);
	respond(r, nil);
}

char*
fsclone(Fid *oldfid, Fid *newfid)
{
	//fprint(2, "fsclone oldfid=%p newfid=%p\n", oldfid->aux, newfid->aux);
	newfid->aux = oldfid->aux;
	return nil;
}

char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	long id;
	char *chp;
	Dnode *node;
	// TODO: check if name is one of control files
	id = strtol(name, &chp, 10);
	if (chp == name) return "not found";
	node = getnode(id);
	*qid = node->q;
	fid->qid = *qid;
	fid->aux = getnode(id);
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
		r->fid->aux = getnode(0);
		break;
	default:
		r->d.qid = r->fid->qid;
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
	newnode(nodes.list[0]);
	newnode(nodes.list[0]);
	newnode(nodes.list[0]);
	newnode(getnode(1));

	Srv fs = {
		.attach = fsattach,
		.read = fsread,
		.clone = fsclone,
		.walk1 = fswalk1,
		.stat = fsstat,
	};

	postmountsrv(&fs, srv, mtpt, MREPL);
}

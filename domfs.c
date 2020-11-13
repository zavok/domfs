#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

typedef struct Finf Finf; /* file info */
typedef struct DTree DTree;
typedef struct Node Node;

enum { /* file types */
	ROOT,
	TREE,
	NODE,
	CTRL,
	USER,
};

struct Finf {
	uvlong id;
	Qid qid;
	int type;
	void *aux;
};

struct DTree {
	Node **nodes;
	uvlong id;
	uvlong nidcount;
	Finf *finf;
};

struct Node {
	Node *parent;
	Node **children;
	uvlong id;
	Finf *finf;
};

Finf **files, *rootf;

DTree **trees;
static uvlong tcount;

long
stacksize(void **v)
{
	long n;
	if (v == nil) return 0;
	for (n = 0; *v != nil; v++) n++;
	return n;
}

void
stackpush(void ***v, void *new)
{
	long n;
	n = stacksize(*v);
	*v = realloc(*v, (n + 2) * sizeof(void*));
	(*v)[n] = new;
	(*v)[n+1] = nil;
}

Finf*
newfinf(int type, void *aux)
{
	static uvlong id;
	Finf *new;
	new = malloc(sizeof(Finf));
	switch (type) {
	case ROOT:
	case TREE:
	case NODE:
			new->qid = (Qid){id, 0, QTDIR};
			break;
	case CTRL:
	case USER:
			new->qid = (Qid){id, 0, QTFILE};
			break;
	default:
			sysfatal("newfinf: unknown file type %d", type);
	}
	new->type = type;
	new->aux = aux;
	id++;
	return new;
}

DTree*
newtree(void)
{
	static uvlong id;
	DTree *new;
	new = malloc(sizeof(DTree));
	new->nodes = nil;
	new->id = ++id;
	new->nidcount = 0;
	new->finf = newfinf(TREE, new);
	stackpush(&files, new->finf);
	return new;
}

DTree*
gettree(DTree **trees, uvlong id)
{
	DTree *tp;
	for (tp = trees[0]; tp != nil; tp++)
		if (tp->id == id) return tp;
	return nil;
}

Node*
newnode(DTree *T)
{
	Node *new;
	new = malloc(sizeof(Node));
	new->parent = nil;
	new->children = nil;
	new->id = ++T->nidcount;
	new->finf = newfinf(NODE, new);
	stackpush(&files, new->finf);
	return new;
}

Node*
getnode(DTree *T, uvlong n)
{
	Node **np;
	for (np = T->nodes; np != nil; np++) {
		if ((*np)->id == n) {
			return *np;
		}
	}
	return nil;
}

void
fsattach(Req *r)
{
	r->fid->qid = rootf->qid;
	r->fid->aux = rootf;
	r->ofcall.qid = r->fid->qid;
	respond(r, nil);
}

int
dirgenroot(int n, Dir *dir, void*)
{
	DTree *t;
	t = trees[n];
	if (t == nil) return -1;
	nulldir(dir);
	dir->qid = t->finf->qid;
	dir->mode = 0777 | DMDIR;
	dir->atime = time(0);
	dir->mtime = time(0);
	dir->length = 0;
	dir->name = smprint("%ulld", t->id);
	dir->uid = strdup("domfs");
	dir->gid = strdup("domfs");
	dir->muid = strdup("");
	return 0;
}

int
dirgentree(int n, Dir *dir, void *aux)
{
	DTree *tree;
	Node *node;
	tree = aux;
	node = tree->nodes[n];
	if (node == nil) return -1;
	nulldir(dir);
	dir->qid = node->finf->qid;
	dir->mode = 0777 | DMDIR;
	dir->atime = time(0);
	dir->mtime = time(0);
	dir->length = 0;
	dir->name = smprint("%ulld", node->id);
	dir->uid = strdup("domfs");
	dir->gid = strdup("domfs");
	dir->muid = strdup("");
	return 0;
}

void
fsread(Req *r)
{
	Finf *f;
	f = r->fid->aux;
	switch(f->type){
	case ROOT:
		dirread9p(r, dirgenroot, nil);
		break;
	case TREE:
		dirread9p(r, dirgentree, f->aux);
		break;
	case NODE:
	case CTRL:
	case USER:
	default:
		sysfatal("fsread: unknown file type: %d", f->type);
	}

	//dirread9p(r, dirgen, nil);
	respond(r, nil);
}

char*
fsclone(Fid *oldfid, Fid *newfid)
{
	newfid->aux = oldfid->aux;
	return nil;
}

char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	char *chp;
	uvlong id;
	Finf *f, *nf;
	f = fid->aux;
	nf = nil;
	switch (f->type){
	case ROOT:
		id = strtoull(name, &chp, 10);
		// TODO: check if parsed correctly
		nf = gettree(trees, id)->finf;
		break;
	case TREE:
		id = strtoull(name, &chp, 10);
		nf = getnode(f->aux, id)->finf;
		break;
	case NODE:
	case CTRL:
	case USER:
	default:
		sysfatal("fswalk1: unknown file type %d", f->type);
	}
	if (nf == nil) return "fswalk1: nf = nil";
	*qid = nf->qid;
	fid->qid = *qid;
	fid->aux = nf;
	return nil;
}

void
fsstat(Req *r)
{
	Finf *f;
	f = r->fid->aux;
	nulldir(&r->d);
	r->d.type = L'M';
	r->d.dev = 1;
	r->d.length = 0;
	r->d.atime = time(0);
	r->d.mtime = time(0);
	r->d.uid = strdup("domfs");
	r->d.gid = strdup("domfs");
	r->d.muid = strdup("");
	r->d.qid = f->qid;
	
	switch (f->type) {
	case ROOT:
		r->d.name = strdup("/");
		r->d.mode = 0777|DMDIR;
		break;
	case TREE:
		r->d.name = smprint("%ulld", ((DTree*)f->aux)->id);
		r->d.mode = 0777|DMDIR;
		break;
	case NODE:
	case CTRL:
	case USER:
	default:
		sysfatal("fsstat: unknown file type %d", f->type);
	}
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
	
	rootf = newfinf(ROOT, &trees);
	stackpush(&files, rootf);

	stackpush(&trees, newtree());
	stackpush(&trees, newtree());
	stackpush(&trees, newtree());

	stackpush(&(trees[0]->nodes), newnode(trees[0]));
	stackpush(&(trees[0]->nodes), newnode(trees[0]));
	
	Srv fs = {
		.attach = fsattach,
		.read = fsread,
		.clone = fsclone,
		.walk1 = fswalk1,
		.stat = fsstat,
	};

	postmountsrv(&fs, srv, mtpt, MREPL);
}

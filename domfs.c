#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

typedef struct Finf Finf; /* file info */
typedef struct DTree DTree;
typedef struct Node Node;
typedef struct Fusr Fusr;

enum { /* file types */
	ROOT,
	TREE,
	NODE,
	CTRL,
	USER,

	REMV = -1,
};

struct Finf {
	uvlong id;
	Qid qid;
	int type;
	void *aux;
};

struct DTree {
	uvlong id;
	Node **nodes;
	uvlong nidcount;
	Finf *finf;
};

struct Node {
	uvlong id;
	DTree *tree;
	Node *parent;
	Node **children;
	Fusr **files;
	Finf *finf;
};

struct Fusr {
	Node *node;
	char *name;
	char *data;
	uvlong nsize;
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

void
stackremv(void ***v, void *w)
{
	long i, n;
	int remv;
	remv = 0;
	n = stacksize(*v);
	for (i = 0; i < n; i++) {
		if ((*v)[i] == w) remv = 1;
		if (remv != 0) (*v)[i] = (*v)[i+1];
	}
	if (remv != 0) *v = realloc(*v, (n) * sizeof(void*));
}

Finf*
newfinf(int type, void *aux)
{
	static uvlong id = 0;
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
	new->id = id++;
	new->type = type;
	new->aux = aux;
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
	DTree **tp;
	if (trees == nil) return nil;
	for (tp = trees; *tp != nil; tp++)
		if ((*tp)->id == id) return *tp;
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
	new->tree = T;
	stackpush(&files, new->finf);
	return new;
}

Node*
getnode(DTree *T, uvlong n)
{
	Node **np;
	if (T->nodes == nil) return nil;
	for (np = T->nodes; *np != nil; np++)
		if ((*np)->id == n) return *np;
	return nil;
}

Fusr*
newfile(char *name)
{
	Fusr *new;
	new = malloc(sizeof(Fusr));
	new->node = nil;
	new->name = strdup(name);
	new->data = nil;
	new->nsize = 0;
	new->finf = newfinf(USER, new);
	stackpush(&files, new->finf);
	return new;
}

Fusr*
getfile(Node *node, char *name)
{
	Fusr **fp;
	if (node->files == nil) return nil;
	for (fp = node->files; *fp != nil; fp++)
		if (strcmp(name, (*fp)->name) == 0) return *fp;
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
	if (trees == nil) return -1;
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
	if (tree == nil) return -1;
	if (tree->nodes == nil) return -1;
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

int
dirgennode(int n, Dir *dir, void *aux)
{
	Node *node;
	Fusr *file;
	node = aux;
	if (node == nil) return -1;
	if (node->files == nil) return -1;
	file = node->files[n];
	if (file == nil) return -1;
	nulldir(dir);
	dir->qid = file->finf->qid;
	dir->mode = 0666;
	dir->atime = time(0);
	dir->mtime = time(0);
	dir->length = file->nsize;
	dir->name = strdup(file->name);
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
		dirread9p(r, dirgennode, f->aux);
		break;
	case CTRL:
	case USER:
		readbuf(r, ((Fusr*)f->aux)->data, ((Fusr*)f->aux)->nsize);
		break;
	case REMV:
		respond(r, "file does not exist");
		return;
	default:
		respond(r, "fsread: unknown file type");
		return;
	}
	respond(r, nil);
}

void
fswrite(Req *r)
{
	Finf *file;
	Fusr *f;
	file = r->fid->aux;
	switch (file->type) {
	case USER:
		f = file->aux;
		f->nsize = r->ifcall.count;
		f->data = realloc(f->data, f->nsize);
		memmove(f->data, r->ifcall.data, f->nsize);
		r->ofcall.count = f->nsize;
		respond(r, nil);
		break;
	default:
		respond(r, "permission denied");
	}
}

void
fscreate(Req *r)
{
	Node *node;
	Finf *rf;
	Fusr *f;
	rf = r->fid->aux;
	if (rf->type != NODE) {
		respond(r, "permission denied");
		return;
	}
	node = rf->aux;
	f = newfile(r->ifcall.name);
	f->node = node;
	stackpush(&(node->files), f);
	r->fid->qid = f->finf->qid;
	r->ofcall.qid = r->fid->qid;
	r->fid->aux = f->finf;
	respond(r, nil);
}

void
fsremove(Req *r)
{
	Finf *f;
	f = r->fid->aux;
	if (f->type != USER) {
		respond(r, "remove prohibited");
		return;
	}
	f->type = REMV;
	
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
	void *p;
	char *chp;
	uvlong id;
	Finf *f, *nf;
	f = fid->aux;
	nf = nil;
	switch (f->type){
	case ROOT:
		id = strtoull(name, &chp, 10);
		// TODO: check if parsed correctly
		p = gettree(trees, id);
		if (p == nil) return "file does not exist";
		nf = ((DTree*)p)->finf;
		break;
	case TREE:
		if (strcmp("..", name) == 0) {
			nf = rootf;
			break;
		}
		id = strtoull(name, &chp, 10);
		// TODO: check if parsed correctly
		p = getnode(f->aux, id);
		if (p == nil) return "file does not exist";
		nf = ((Node*)p)->finf;
		break;
	case NODE:
		if (strcmp("..", name) == 0) {
			nf = ((Node*)f->aux)->tree->finf;
			break;
		}
		p = getfile(f->aux, name);
		if (p == nil) return "file does not exist";
		nf = ((Fusr*)p)->finf;
		break;
	case CTRL:
	case USER:
	case REMV:
		return "walk on file, what?";
		break;
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
		r->d.name = smprint("%ulld", ((Node*)f->aux)->id);
		r->d.mode = 0777|DMDIR;
		break;
	case USER:
		r->d.name = strdup(((Fusr*)f->aux)->name);
		r->d.mode = 0666;
		break;
	case REMV:
		r->d.name = strdup(((Fusr*)f->aux)->name);
		r->d.mode = -1;
		break;
	case CTRL:
	default:
		sysfatal("fsstat: unknown file type %d", f->type);
	}
	
	respond(r, nil);
}

void
fsdestroyfid(Fid *fid)
{
	/*
	 * TODO: this whole func is probably not correct;
	 * 9p(2) man page description of remove func recommends tracking fids that
	 * access file to be deleted, but I am not sure how to do it
	 */

	Finf *f;
	Fusr *fu;
	f = fid->aux;
	if(f != nil && f->type == REMV) {
		fu = f->aux;
		free(fu->data);
		free(fu->name);
		fu->nsize = 0;
		stackremv(&(fu->node->files), fu);
		free(fu);
		stackremv(&files, &f);
		free(f);
	}
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
		.write = fswrite,
		.clone = fsclone,
		.walk1 = fswalk1,
		.stat = fsstat,
		.create = fscreate,
		.remove = fsremove,
		.destroyfid = fsdestroyfid,
	};

	postmountsrv(&fs, srv, mtpt, MREPL);
}

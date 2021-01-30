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
	NCTL,
	USER,
	NNEW,
	TNEW,

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
	Finf *fnew;
};

struct Node {
	uvlong id;
	DTree *tree;
	Node *parent;
	Node **children;
	Fusr **files;
	Finf *finf;
	Finf *fctl;
};

struct Fusr {
	Node *node;
	char *name;
	char *data;
	uvlong nsize;
	Finf *finf;
};

Finf **files, *rootf, *fnew;

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
	new = mallocz(sizeof(Finf), 1);
	switch (type) {
	case ROOT:
	case TREE:
	case NODE:
			new->qid = (Qid){id, 0, QTDIR};
			break;
	case NNEW:
	case TNEW:
	case NCTL:
	case USER:
	case REMV:
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
	new = mallocz(sizeof(DTree), 1);
	new->id = ++id;
	new->finf = newfinf(TREE, new);
	stackpush(&files, new->finf);

	new->fnew = newfinf(NNEW, new);
	stackpush(&files, new->fnew);

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
	new = mallocz(sizeof(Node), 1);
	new->id = ++T->nidcount;
	new->finf = newfinf(NODE, new);
	new->tree = T;
	stackpush(&files, new->finf);
	new->fctl = newfinf(NCTL, new);
	stackpush(&files, new->fctl);
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
	new = mallocz(sizeof(Fusr), 1);
	new->name = strdup(name);
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
	if (n == 0) {
		nulldir(dir);
		dir->qid = fnew->qid;
		dir->mode = 0666;
		dir->atime = time(0);
		dir->mtime = time(0);
		dir->length = 0;
		dir->name = strdup("new");
		dir->uid = strdup("domfs");
		dir->gid = strdup("domfs");
		dir->muid = strdup("");
		return 0;
	}
	n--;
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
	if (n == 0) {
		nulldir(dir);
		dir->qid = tree->fnew->qid;
		dir->mode = 0666;
		dir->atime = time(0);
		dir->mtime = time(0);
		dir->length = 0;
		dir->name = strdup("new");
		dir->uid = strdup("domfs");
		dir->gid = strdup("domfs");
		dir->muid = strdup("");
		return 0;
	}
	n--;
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

	if (n == 0) {
		nulldir(dir);
		dir->qid = node->fctl->qid;
		dir->mode = 0666;
		dir->atime = time(0);
		dir->mtime = time(0);
		dir->length = 0;
		dir->name = strdup("ctl");
		dir->uid = strdup("domfs");
		dir->gid = strdup("domfs");
		dir->muid = strdup("");
		return 0;
	}
	n--;

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

char *
nctl2str(Finf *f)
{
	char *str;
	Node *node, **cp;
	uvlong parid;
	long len, n;
	if (f == nil) sysfatal("nctl2str: f == nil"); //return nil;
	if (f->type != NCTL) sysfatal("nctl2str: f->type != NCTL"); //return nil;
	node = f->aux;
	if (node == nil) sysfatal("nctl2str: node == nil"); //return nil;
	parid = (node->parent != nil) ? node->parent->id : 0;

	str = smprint("parent %ulld\nchildren\n", parid);
	len = strlen(str);
	
	if (node->children == nil) return str;
	for (cp = node->children; *cp != nil; cp++) {
		char *buf;
		str[len - 1] = ' ';
		buf = smprint("%ulld\n", (*cp)->id);
		n = strlen(buf);
		str = realloc(str, len + n + 1);
		memmove(str + len, buf, n);
		str[len + n] = 0;
		free(buf);
		len += n;
	}
	return str;
}

char *
nctlparse(Finf *f, char *cmd)
{
	Node *node;
	DTree *tree;
	int argc;
	char **argv;
	if (f->type != NCTL) sysfatal("nctlparse: f->type != NCTL");
	node = f->aux;
	tree = node->tree;
	argv = malloc(sizeof(char*) * 256);
	argc = tokenize(cmd, argv, 256);
	if (argc == 0) return nil;
	if (strcmp(argv[0], "adopt") == 0) {
		char *cpt;
		Node *child;
		uvlong id;
		if (argc == 1) return "not enough args";
		id = strtoull(argv[1], &cpt, 10);
		// TODO: check if parsed correctly
		child = getnode(tree, id);
		if (child == nil) return "no such node";
		if (child == node) return "can't adopt self";
		if (child->parent != nil) stackremv(&child->parent->children, child);
		stackpush(&(node->children), child);
		child->parent = node;
		return nil;
	}
	return "unknown cmd";
}

void
fsread(Req *r)
{
	Finf *f;
	char *buf;
	Node *node;
	DTree *tree;
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
	case USER:
		readbuf(r, ((Fusr*)f->aux)->data, ((Fusr*)f->aux)->nsize);
		break;
	case NCTL:
		// TODO: first line - parent, sec line - children
		buf = nctl2str(f);
		if (buf == nil) break;
		readstr(r, buf);
		free(buf);
		break;
	case TNEW:
		if (r->ifcall.offset == 0) {
			tree  = newtree();
			stackpush(&trees, tree);
		} else tree = gettree(trees, stacksize(trees));
		buf = smprint("%ulld\n", tree->id);
		readstr(r, buf);
		free(buf);
		break;
	case NNEW:
		tree = f->aux;
		if (r->ifcall.offset == 0) {
			node  = newnode(tree);
			stackpush(&(tree->nodes), node);
		} else node = getnode(tree, stacksize(tree->nodes));
		buf = smprint("%ulld\n", node->id);
		readstr(r, buf);
		free(buf);
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
	char *buf, *rstr;
	long off, nsize;
	Finf *file;
	Fusr *f;
	file = r->fid->aux;
	switch (file->type) {
	case USER:
		// TODO: finish this section
		f = file->aux;
		off = r->ifcall.offset;
		if (r->d.mode & DMAPPEND) off = f->nsize;
		nsize = off + r->ifcall.count;
		if (nsize > f->nsize) f->nsize = nsize;
		f->data = realloc(f->data, f->nsize);
		memmove(f->data + off, r->ifcall.data, r->ifcall.count);
		r->ofcall.count = r->ifcall.count;
		rstr = nil;
		break;
	case NCTL:
		buf = mallocz(r->ifcall.count + 1, 1);
		memmove(buf, r->ifcall.data, r->ifcall.count);
		rstr = nctlparse(file, buf);
		free(buf);
		r->ofcall.count = r->ifcall.count;
		break;
	default:
		rstr = "permission denied";
	}
	respond(r, rstr);
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
		if (strcmp("new", name) == 0) {
			nf = fnew;
			break;
		}
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
		if (strcmp("new", name) == 0) {
			nf = ((DTree*)f->aux)->fnew;
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
		if (strcmp("ctl", name) == 0) {
			nf = ((Node*)f->aux)->fctl;
			break;
		}
		p = getfile(f->aux, name);
		if (p == nil) return "file does not exist";
		nf = ((Fusr*)p)->finf;
		break;
	case NNEW:
	case TNEW:
	case NCTL:
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
	case NCTL:
		r->d.name = strdup("ctl");
		r->d.mode = 0666;
		break;
	case TNEW:
		r->d.name = strdup("new");
		r->d.mode = 0666;
		break;
	case NNEW:
		r->d.name = strdup("new");
		r->d.mode = 0666;
		break;
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
	 * 9p(2) manpage's description of remove func recommends tracking fids that
	 * access file to be deleted, but I am not sure how to do it properly.
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
	fprint(2, "usage %s [-D][-m /mnt/dom][-s service]\n", argv0);
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *srv, *mtpt;
	srv = nil;
	mtpt = "/mnt/dom";

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
	fnew = newfinf(TNEW, &rootf);
	stackpush(&files, fnew);
	
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

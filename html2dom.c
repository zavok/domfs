#include <u.h>
#include <libc.h>
#include <String.h>
#include <thread.h>

#define ALPHA(x) ((x >=0x41) && (x <= 0x7a))

static char *drpath = "/n/dom";
static char *tpath = nil;

int gc(void);

/* Tokens code */

typedef struct Attr Attr;

struct Attr{
	String *name;
	String *value;
};

enum { /* Token types */
	TDOCT,
	TSTART,
	TEND,
	TCOMM,
	TCHAR,
	TTAG,
	TEOF = -1,
};

typedef struct Token Token;
struct Token {
	int type;
	Rune c;
	String *name;
	Attr **attr;
};

Token* chartok(Rune);
Token* eoftok(void);
void t_free(Token*);
Attr* tnewattr(Token*);
void attr_free(Attr*);

Token*
eoftok(void)
{
	Token *t;
	t = mallocz(sizeof(Token), 1);
	t->type = TEOF;
	return t;
}

Token*
chartok(Rune c)
{
	Token *t;
	t = mallocz(sizeof(Token), 1);
	t->c = c;
	t->type = TCHAR;
	return t;
}

void
t_free(Token *t)
{
	s_free(t->name);
	free(t);
}


Attr*
tnewattr(Token *t)
{
	int n;
	if (t->attr == nil) t->attr = mallocz(sizeof(Attr*), 1);
	for (n=0; (t->attr)[n] != nil; n++);
	t->attr = realloc(t->attr, (n + 1) * sizeof(Attr*));
	t->attr[n] = mallocz(sizeof(Attr), 1);
	t->attr[n]->name = s_new();
	t->attr[n]->value = s_new();
	t->attr[n+1] = nil;
	return t->attr[n];
}

void
attr_free(Attr *attr)
{
	s_free(attr->name);
	s_free(attr->value);
	free(attr);
}

/*
 * Insertion modes, as defined in
 * https://html.spec.whatwg.org/#the-insertion-mode
 */

enum {
	IMinitial = 0,
	IMbefore_html = 1,
	IMbefore_head = 1 << 1,
	IMin_head = 1 << 2,
	IMin_head_noscript = 1 << 3,
	IMafter_head = 1 << 4,
	IMin_body = 1 << 5,
	IMtext = 1 << 6,
	IMin_table = 1 << 7,
	IMin_table_text = 1 << 8,
	IMin_caption = 1 << 9,
	IMin_column_group = 1 << 10,
	IMin_table_body = 1 << 11,
	IMin_row = 1 << 12,
	IMin_cell = 1 << 13,
	IMin_select = 1 << 14,
	IMin_select_in_table = 1 << 15,
	IMin_template = 1 << 16,
	IMafter_body = 1 << 17,
	IMin_frameset = 1 << 18,
	IMafter_frameset = 1 << 19,
	IMafter_after_body = 1 << 20,
	IMafter_after_frameset = 1 << 21,
};

u32int insertion_mode = IMinitial;

/* Tokenizer vars and funcs */

Rune tc;
int treconsume = 0;
int teof;

Token *ctoken;
Attr *cattr;
String *ctempbuf;

void tconsume(void);
void temit(Token*);
void temitbuf(void);
int talpha(int);

void tsdata(void);
void tsrcdt(void);
void tsrawt(void);
void tsscript(void);
void tsptxt(void);
void tstagopen(void);
void tsetagopen(void);
void tstagname(void);
void tsrcdtless(void);
void tsrcdtendopen(void);
void tsrcdtendname(void);
void tsrawtless(void);
void tsrawtendopen(void);
void tsrawtendname(void);
void tsscriptless(void);
void tsscriptendopen(void);
void tsscriptendname(void);

void tsscriptescstart(void);
void tsscriptescstartdash(void);
void tsscriptesc(void);
void tsscriptescdash(void);
void tsscriptescddash(void);
void tsscriptescless(void);
void tsscriptescendopen(void);
void tsscriptescendname(void);
void tsscriptdescstart(void);
void tsscriptdesc(void);
void tsscriptdescdash(void);
void tsscriptdescddash(void);
void tsscriptdescless(void);
void tsscriptdescend(void);

void tsanamebefore(void);
void tsaname(void);
void tsanameafter(void);
void tsavalbefore(void);
void tsavaldq(void);
void tsavalsq(void);
void tsavaluq(void);
void tsavalafter(void);
void tsscstag(void);
void tsboguscomment(void);
void tsmkupopen(void);
void tscommentstart(void);
void tscommentstartdash(void);
void tscomment(void);
void tscommentless(void);
void tscommentlessbang(void);
void tscommentlessbangdash(void);
void tscommentlessbangddash(void);
void tscommentebddash(void);
void tscommentebd(void);
void tscommentebdbang(void);
void tsdoct(void);
void tsdoctbefore(void);
void tsdoctname(void);
void tsdoctnameafter(void);
void tsdoctpubkafter(void);
void tsdoctpubidbefore(void);
void tsdoctpubiddq(void);
void tsdoctpubidsq(void);
void tsdoctpubidafter(void);
void tsdoctbetween(void);
void tsdoctsyskafter(void);
void tsdoctsysidbefore(void);
void tsdoctsysiddQ(void);
void tsdoctsysidSQ(void);
void tsdoctsysidafter(void);
void tsdoctbogus(void);
void tscdat(void);
void tscdatbrk(void);
void tscdatend(void);
void tscref(void);
void tsncref(void);
void tsamam(void);
void tsnumref(void);
void tshexrefstart(void);
void tsdecrefstart(void);
void tshexref(void);
void tsdecref(void);
void tsnumrefend(void);


#define REPCHAR Runeerror  /* replacement character */

enum {
	TSDATA,                  /* data                                      */
	TSRCDT,                  /* RCDATA                                    */
	TSRAWT,                  /* RAWTEXT                                   */
	TSSCRIPT,                /* script data                               */
	TSPTXT,                  /* PLAINTEXT                                 */
	TSTAG_OPEN,              /* tag open                                  */
	TSETAG_OPEN,             /* end tag open                              */
	TSTAG_NAME,              /* tag name                                  */
	TSRCDT_LESS,             /* RCDATA less-than sign                     */
	TSRCDT_END_OPEN,         /* RCDATA end tag open                       */
	TSRCDT_END_NAME,         /* RCDATA end tag name                       */
	TSRAWT_LESS,             /* RAWTEXT less-than sign                    */
	TSRAWT_END_OPEN,         /* RAWTEXT end tag open                      */
	TSRAWT_END_NAME,         /* RAWTEXT end tag name                      */
	TSSCRIPT_LESS,           /* script data less-than sign                */
	TSSCRIPT_END_OPEN,       /* script data end tag open                  */
	TSSCIRPT_END_NAME,       /* script data end tag name                  */
	TSSCRIPT_ESC_START,      /* scirpt data escape start                  */
	TSSCRIPT_ESC_START_DASH, /* scirpt data escape start dash             */
	TSSCRIPT_ESC,            /* scirpt data escaped                       */
	TSSCRIPT_ESC_DASH,       /* scirpt data escaped dash                  */

	TSSCRIPT_ESC_DDASH,      /* scirpt data escaped dash dash             */
	TSSCRIPT_ESC_LESS,       /* scirpt data escaped less-than sign        */
	TSSCRIPT_ESC_END_OPEN,   /* scirpt data escaped end tag open          */
	TSSCRIPT_ESC_END_NAME,   /* scirpt data escaped end tag name          */
	TSSCRIPT_DESC_START,     /* scirpt data double escape start           */
	TSSCRIPT_DESC,           /* scirpt data double escaped                */
	TSSCRIPT_DESC_DASH,      /* scirpt data double escaped dash           */
	TSSCRIPT_DESC_DDASH,     /* scirpt data double escaped dash dash      */
	TSSCRIPT_DESC_LESS,      /* scirpt data double escaped less-than sign */
	TSSCRIPT_DESC_END,       /* scirpt data double escape end             */

	TSANAME_BEFORE,          /* Before attribute name           */
	TSANAME,                 /* Attribute name                  */
	TSANAME_AFTER,           /* After attribute name            */
	TSAVAL_BEFORE,          /* Before attribute value          */
	TSAVAL_DQ,               /* Attribute value (double-quoted) */
	TSAVAL_SQ,               /* Attribute value (single-quoted) */
	TSAVAL_UQ,               /* Attribute value (unquoted)      */
	TSAVAL_AFTER,            /* After attribute value (quoted)  */
	
	TSSCSTAG,        /* Self-closing start tag  */
	TSBOGUS_COMMENT, /* Bogus comment           */
	TSMKUP_OPEN,     /* Markup declaration open */
	
	TSCOMMENT_START,           /* Comment start                         */
	TSCOMMENT_START_DASH,      /* Comment start dash                    */
	TSCOMMENT,                 /* Comment                               */
	TSCOMMENT_LESS,            /* Comment less-than sign                */
	TSCOMMENT_LESS_BANG,       /* Comment less-than sign bang           */
	TSCOMMENT_LESS_BANG_DASH,  /* Comment less-than sign bang dash      */
	TSCOMMENT_LESS_BANG_DDASH, /* Comment less-than sign bang dash dash */
	TSCOMMENT_END_DASH,        /* Comment end dash                      */
	TSCOMMENT_END,             /* Comment end                           */
	TSCOMMENT_END_BANG,        /* Comment end bang                      */
	
	TSDOCT,              /* DOCTYPE                                       */
	TSDOCT_BEFORE,       /* Before DOCTYPE name                           */
	TSDOCT_NAME,         /* DOCTYPE name                                  */
	TSDOCT_NAME_AFTER,   /* After DOCTYPE name                            */
	TSDOCT_PUBK_AFTER,   /* After DOCTYPE public keyword                  */
	TSDOCT_PUBID_BEFORE, /* Before DOCTYPE public identifier              */
	TSDOCT_PUBID_DQ,     /* DOCTYPE public identifier (double-quoted)     */
	TSDOCT_PUBID_SQ,     /* DOCTYPE public identifier (single-quoted)     */
	TSDOCT_PUBID_AFTER,  /* After DOCTYPE public identifier               */
	TSDOCT_BETWEEN,      /* Between DOCTYPE public and system identifiers */
	TSDOCT_SYSK_AFTER,   /* After DOCTYPE system keyword                  */
	TSDOCT_SYSID_BEFORE, /* Before DOCTYPE system identifier              */
	TSDOCT_SYSID_DQ,     /* DOCTYPE system identifier (double-quoted)     */
	TSDOCT_SYSID_SQ,     /* DOCTYPE system identifier (single-quoted)     */
	TSDOCT_SYSID_AFTER,  /* After DOCTYPE system identifier               */
	TSDOCT_BOGUS,        /* Bogus DOCTYPE                                 */
	
	TSCDAT,     /* CDATA section         */
	TSCDAT_BRK, /* CDATA section bracket */
	TSCDAT_END, /* CDATA section end     */
	
	TSCREF,         /* Character reference                   */
	TSNCREF,        /* Named character reference             */
	TSAMAM,         /* Ambiguous ampersand                   */
	TSNUMREF,       /* Numeric character reference           */
	TSHEXREF_START, /* Hexadecimal character reference start */
	TSDECREF_START, /* Decimal character reference start     */
	TSHEXREF,       /* Hexadecimal character reference       */
	TSDECREF,       /* Decimal character reference           */
	TSNUMREF_END,   /* Numeric character reference end       */
	
	TMAX,
};

void (*tstab[])(void) = {
	[TSDATA]            = tsdata,
	[TSRCDT]            = tsrcdt,
	[TSRAWT]            = tsrawt,
	[TSSCRIPT]          = tsscript,
	[TSPTXT]            = tsptxt,
	[TSTAG_OPEN]        = tstagopen,
	[TSETAG_OPEN]       = tsetagopen,
	[TSTAG_NAME]        = tstagname,
	[TSRCDT_LESS]       = tsrcdtless,
	[TSRCDT_END_OPEN]   = tsrcdtendopen,
	[TSRCDT_END_NAME]   = tsrcdtendname,
	[TSRAWT_LESS]       = tsrawtless,
	[TSRAWT_END_OPEN]   = tsrawtendopen,
	[TSSCRIPT_LESS]     = tsscriptless,
	[TSSCRIPT_END_OPEN] = tsscriptendopen,
	[TSSCIRPT_END_NAME] = tsscriptendname,
	[TSSCRIPT_ESC_START] = tsscriptesc,
	[TSSCRIPT_ESC_START_DASH] = tsscriptesc,
	[TSSCRIPT_ESC] = tsscriptesc,
	[TSSCRIPT_ESC_DASH] = tsscriptescdash,
	[TSSCRIPT_ESC_DDASH] = tsscriptescddash,
	[TSSCRIPT_ESC_LESS] = tsscriptescless,
	[TSSCRIPT_ESC_END_OPEN] = tsscriptescendopen,
	[TSSCRIPT_ESC_END_NAME] = tsscriptescendname,
	[TSSCRIPT_DESC_START] = tsscriptdescstart,
	[TSSCRIPT_DESC] = tsscriptdesc,
	[TSSCRIPT_DESC_DASH] = tsscriptdescdash,
	[TSSCRIPT_DESC_DDASH] = tsscriptdescddash,
	[TSSCRIPT_DESC_LESS] = tsscriptdescless,
	[TSSCRIPT_DESC_END] = tsscriptdescend,

	[TSANAME_BEFORE] = tsanamebefore,
	[TSANAME]        = tsaname,
	[TSANAME_AFTER] = nil,
	[TSAVAL_BEFORE] = nil,
	[TSAVAL_DQ] = nil,
	[TSAVAL_SQ] = nil,
	[TSAVAL_UQ] = nil,
	[TSAVAL_AFTER] = nil,
	[TSSCSTAG] = nil,
	[TSBOGUS_COMMENT] = nil,
	[TSMKUP_OPEN] = nil,
	[TSCOMMENT_START] = nil,
	[TSCOMMENT_START_DASH] = nil,
	[TSCOMMENT] = nil,
	[TSCOMMENT_LESS] = nil,
	[TSCOMMENT_LESS_BANG] = nil,
	[TSCOMMENT_LESS_BANG_DASH] = nil,
	[TSCOMMENT_LESS_BANG_DDASH] = nil,
	[TSCOMMENT_END_DASH] = nil,
	[TSCOMMENT_END] = nil,
	[TSCOMMENT_END_BANG] = nil,
	[TSDOCT] = nil,
	[TSDOCT_BEFORE] = nil,
	[TSDOCT_NAME] = nil,
	[TSDOCT_NAME_AFTER] = nil,
	[TSDOCT_PUBK_AFTER] = nil,
	[TSDOCT_PUBID_BEFORE] = nil,
	[TSDOCT_PUBID_DQ] = nil,
	[TSDOCT_PUBID_SQ] = nil,
	[TSDOCT_PUBID_AFTER] = nil,
	[TSDOCT_BETWEEN] = nil,
	[TSDOCT_SYSK_AFTER] = nil,
	[TSDOCT_SYSID_BEFORE] = nil,
	[TSDOCT_SYSID_DQ] = nil,
	[TSDOCT_SYSID_SQ] = nil,
	[TSDOCT_SYSID_AFTER] = nil,
	[TSDOCT_BOGUS] = nil,
	[TSCDAT] = nil,
	[TSCDAT_BRK] = nil,
	[TSCDAT_END] = nil,
	[TSCREF] = nil,
	[TSNCREF] = nil,
	[TSAMAM] = nil,
	[TSNUMREF] = nil,
	[TSHEXREF_START] = nil,
	[TSDECREF_START] = nil,
	[TSHEXREF] = nil,
	[TSDECREF] = nil,
	[TSNUMREF_END] = nil,
};

int tstate = TSDATA;
int trstate = -1;

void
tsanamebefore(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		break;
	case '/':
	case '>':
	case  -1:
		treconsume = 1;
		tstate = TSANAME_AFTER;
		break;
	case '=':
		fprint(2, "unexpected equals sign before attribute name parse error\n");
		cattr = tnewattr(ctoken);
		s_nappend(cattr->name, (char*)(&tc), 4);
		tstate = TSANAME;
		break;
	default:
		cattr = tnewattr(ctoken);
		treconsume = 1;
		tstate = TSANAME;
	}
}

void
tsaname(void)
{
	char buf[UTFmax];
	int n, err;
	if (ALPHA(tc) != 0) {
		if (tc < 'a') tc += 0x20;
	}
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
	case '/':
	case '>':
	case -1:
		treconsume = 1;
		tstate = TSANAME_AFTER;
		break;
	case '=':
		tstate = TSAVAL_BEFORE;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		err = REPCHAR;
		s_nappend(cattr->name, (char *)(&err), 2);
		break;
	case '"':
	case '\'':
	case '<':
		fprint(2, "unexpected character in attribute name parse error\n");
	default:
		n = runetochar(buf, &tc);
		s_nappend(cattr->name, buf, n);
	}
}

void
tsanameafter(void)
{
}

void
tsavalbefore(void)
{
}

void
tsavaldq(void)
{
}

void
tsavalsq(void)
{
}

void
tsavaluq(void)
{
}

void
tsavalafter(void)
{
}

void
tsscstag(void)
{
}

void
tsboguscomment(void)
{
}

void
tsmkupopen(void)
{
}

void
tscommentstart(void)
{
}

void
tscommentstartdash(void)
{
}

void
tscomment(void)
{
}

void
tscommentless(void)
{
}

void
tscommentlessbang(void)
{
}

void
tscommentlessbangdash(void)
{
}

void
tscommentlessbangddash(void)
{
}

void
tscommentebddash(void)
{
}

void
tscommentebd(void)
{
}

void
tscommentebdbang(void)
{
}

void
tsdoct(void)
{
}

void
tsdoctbefore(void)
{
}

void
tsdoctname(void)
{
}

void
tsdoctnameafter(void)
{
}

void
tsdoctpubkafter(void)
{
}

void
tsdoctpubidbefore(void)
{
}

void
tsdoctpubiddq(void)
{
}

void
tsdoctpubidsq(void)
{
}

void
tsdoctpubidafter(void)
{
}

void
tsdoctbetween(void)
{
}

void
tsdoctsyskafter(void)
{
}

void
tsdoctsysidbefore(void)
{
}

void
tsdoctsysiddQ(void)
{
}

void
tsdoctsysidSQ(void)
{
}

void
tsdoctsysidafter(void)
{
}

void
tsdoctbogus(void)
{
}

void
tscdat(void)
{
}

void
tscdatbrk(void)
{
}

void
tscdatend(void)
{
}

void
tscref(void)
{
}

void
tsncref(void)
{
}

void
tsamam(void)
{
}

void
tsnumref(void)
{
}

void
tshexrefstart(void)
{
}

void
tsdecrefstart(void)
{
}

void
tshexref(void)
{
}

void
tsdecref(void)
{
}

void
tsnumrefend(void)
{
}

void
tsscriptendname(void)
{
	if (talpha(1) != 0) return;
	if (1 /* appropriate end tag token */) {
		switch (tc) {
		case '\t':
		case '\n':
		case '\r':
		case ' ':
			tstate = TSANAME_BEFORE;
			break;
		case '/':
			tstate = TSSCSTAG;
			break;
		case '>':
			tstate = TSDATA;
			break;
		}
	} else {
		temit(chartok('<'));
		temit(chartok('/'));
		temitbuf();
	}
}


void
tsscriptescstart(void)
{
	if (tc == '-') {
		tstate = TSSCRIPT_ESC_START_DASH;
		temit(chartok('-'));
	} else {
		treconsume = 1;
		tstate = TSSCRIPT;
	}
}


void
tsscriptescstartdash(void)
{
	if (tc == '-') {
		tstate = TSSCRIPT_ESC_DDASH;
		temit(chartok('-'));
	} else {
		treconsume = 1;
		tstate = TSSCRIPT;
	}
}


void
tsscriptesc(void)
{
	switch (tc) {
	case '-':
		tstate = TSSCRIPT_ESC_DASH;
		temit(chartok('-'));
		break;
	case '<':
		tstate = TSSCRIPT_ESC_LESS;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		temit(chartok(REPCHAR));
		break;
	case -1: /* EOF */
		fprint(2, "eof in scipt html comment like text parse error\n");
		temit(eoftok());
	default:
		temit(chartok(tc));
	}
}


void
tsscriptescdash(void)
{
	switch (tc) {
	case '-':
		tstate = TSSCRIPT_ESC_DDASH;
		temit(chartok('-'));
		break;
	case '<':
		tstate = TSSCRIPT_ESC_LESS;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		tstate = TSSCRIPT_ESC;
		temit(chartok(REPCHAR));
		break;
	case -1:
		fprint(2, "eof in script html comment like text parse error\n");
		temit(eoftok());
		break;
	default:
		tstate = TSSCRIPT_ESC;
		temit(chartok(tc));
	}
}


void
tsscriptescddash(void)
{
	
}


void
tsscriptescless(void)
{
}


void
tsscriptescendopen(void)
{
}


void
tsscriptescendname(void)
{
}


void
tsscriptdescstart(void)
{
}


void
tsscriptdesc(void)
{
}


void
tsscriptdescdash(void)
{
}


void
tsscriptdescddash(void)
{
}


void
tsscriptdescless(void)
{
}


void
tsscriptdescend(void)
{
}



void
tsscriptendopen(void)
{
	if (ALPHA(tc) != 0) {
		treconsume = 1;
		tstate = TSSCIRPT_END_NAME;
	} else {
		temit(chartok('<'));
		temit(chartok('/'));
		treconsume = 1;
		tstate = TSDATA;
	}
}

void
tsscriptless(void)
{
	switch (tc) {
	case '/':
		s_reset(ctempbuf);
		tstate = TSSCRIPT_END_OPEN;
		break;
	case '!':
		tstate = TSSCRIPT_ESC_START;
		temit(chartok('<'));
		temit(chartok('!'));
		break;
	default:
		temit(chartok('<'));
		treconsume = 1;
		tstate = TSSCRIPT;
	}		
}

void
tsrawtendname(void)
{
	if (ALPHA(tc) != 0) {
		if (tc < 'a') tc+= 0x20;
		
	} else if (1 /* appropriate end tag token */ ) {
		switch (tc) {
		case '\t':
		case '\n':
		case '\r':
		case ' ':
			tstate = TSANAME_BEFORE;
			break;
		case '/':
			tstate = TSSCSTAG;
			break;
		case '>':
			tstate = TSDATA;
			break;
		}		
	} else {
		temit(chartok('<'));
		temit(chartok('/'));
		temitbuf();
		treconsume = 1;
		tstate = TSRAWT;
	}
}

void
tsrawtendopen(void)
{
	if (ALPHA(tc) != 0) {
		//TODO create new end tag token
		treconsume = 1;
		tstate = TSRAWT;
	} else {
		temit(chartok('<'));
		temit(chartok('/'));
		treconsume = 1;
	}
}

void
tsrawtless(void)
{
	if (tc == '/') {
		s_reset(ctempbuf);
		tstate = TSRAWT_END_OPEN;
	} else {
		temit(chartok('<'));
		treconsume = 1;
	}
}

void
tsrcdtendname(void)
{
	if (talpha (1) != 0) return;
	if ( 1 /* appropriate end tag token ??? */) {
		switch (tc) {
		case '\t':
		case '\n':
		case '\r':
		case ' ':
			tstate = TSANAME_BEFORE;
			break;
		case '/':
			tstate = TSSCSTAG;
			break;
		case '>':
			tstate = TSDATA;
			temit(chartok(tc));
		}
	} else {
		temit(chartok('<'));
		temit(chartok('/'));
		temitbuf();
		treconsume = 1;
		tstate = TSRCDT;
	}
}

void
tsrcdtendopen(void)
{
	if (ALPHA(tc) != 0) {
		//TODO create new end tag token
		treconsume = 1;
		tstate = TSRCDT_END_NAME;
	} else {
		treconsume = 1;
		temit(chartok('<'));
		temit(chartok('/'));
	}
}

void
tsrcdtless(void)
{
	switch (tc) {
	case '/':
		s_reset(ctempbuf);
		tstate = TSRCDT_END_OPEN;
		break;
	default:
		treconsume = 1;
		temit(chartok('<'));
	}
}

void
tstagname(void)
{
	uint err;
	err = REPCHAR;
	if (talpha(tc) != 0) return;
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		tstate = TSANAME_BEFORE;
		break;
	case '/':
		tstate = TSSCSTAG;
		break;
	case '>':
		// TODO emit tag
		tstate = TSDATA;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		s_nappend(ctoken->name, (char*)&err, 2);
		break;
	case -1:
		fprint(2, "eof in tag parse error\n");
		teof = 1;
		temit(eoftok());
		break;
	} 
}

void
tsetagopen(void)
{
	if (ALPHA(tc) != 0) {
		// TODO: create new tag token
		treconsume = 1;
		tstate = TSTAG_NAME;
	} else switch (tc) {
	case '>':
		fprint(2, "missing end tag name parse error\n");
		tstate = TSDATA;
		break;
	case -1:
		fprint(2, "eof before tag name parse error\n");
		temit(chartok('<'));
		teof = 1;
		temit(eoftok());
		break;
	default:
		fprint(2, "invalid first character of tag name parse error\n");
		//TODO: create comment token
		treconsume = 1;
		tstate = TSBOGUS_COMMENT;
	}
}

void
tstagopen(void)
{
	if (ALPHA(tc) != 0) {
		// TODO: create new tag token
		treconsume = 1;
		tstate = TSTAG_NAME;
	} else switch (tc) {
	case '!':
		tstate = TSMKUP_OPEN;
		break;
	case '/':
		tstate = TSETAG_OPEN;
		break;
	case '?':
		fprint(2, "unexpected question mark instead of tag name parse error\n");
		// TODO create comment token
		treconsume = 1;
		tstate = TSBOGUS_COMMENT;
		break;
	case -1:
		fprint(2, "eof before tag name parse error");
		temit(chartok('<'));
		teof = 1;
		temit(eoftok());
		break;
	default:
		fprint(2, "invalid first character of tag name parse error\n");
		temit(chartok('<'));
		treconsume = 1;
		tstate = TSDATA;
	}
}

void
tsptxt(void)
{
	switch (tc) {
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		temit(chartok(REPCHAR)); 
		break;
	case -1: /* EOF */
		teof = 1;
		temit(eoftok());
		break;
	default:
		temit(chartok(tc));
	}
}

void
tsscript(void)
{
	switch (tc) {
	case  '<':
		tstate = TSSCRIPT_LESS;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		temit(chartok(REPCHAR));
		break;
	case -1: /* EOF */
		teof = 1;
		temit(eoftok());
		break;
	default:
		temit(chartok(tc));
	}
}

void
tsrawt(void)
{
	switch (tc) {
	case  '<':
		tstate = TSRAWT_LESS;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		temit(chartok(REPCHAR));
		break;
	case -1: /* EOF */
		teof = 1;
		temit(eoftok());
		break;
	default:
		temit(chartok(tc));
	}
}

void
tsrcdt(void)
{
	switch (tc) {
	case '&':
		trstate = TSRCDT;
		tstate = TSCREF;
		break;
	case '<':
		tstate = TSRCDT_LESS;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		temit(chartok(REPCHAR));
		break;
	case -1: /* EOF */
		teof = 1;
		temit(eoftok());
		break;
	default:
		temit(chartok(tc));
	}	
}

void
tsdata(void)
{
	switch (tc) {
	case '&':
		trstate = TSDATA;
		tstate = TSCREF;
		break;
	case '<':
		tstate = TSTAG_OPEN;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		temit(chartok(tc));
		break;
	case -1: /* EOF */
		teof = 1;
		temit(eoftok());
		break;
	default:
		temit(chartok(tc));
	}
}

int
talpha(int tolower)
{
	char buf[UTFmax];
	int n;
	if (ALPHA(tc) == 0) return 0;
	n = runetochar(buf, &tc);
	s_nappend(ctempbuf, buf, n);
	if ((tolower != 0) && (tc < 'a')) tc+=0x20;
	n = runetochar(buf, &tc);
	s_nappend(ctoken->name, buf, n);
	return 1;
}

void
tconsume(void)
{
	if (treconsume == 0) tc = gc();
	treconsume = 0;
}

void
temitbuf(void)
{
	Rune r;
	char *buf;
	int n, len;
	buf = s_to_c(ctempbuf);
	len = strlen(buf);
	for (n = 0; n < len; n += chartorune(&r, buf+n)){
		temit(chartok(r));
	}
	
}

void
temit(Token *t)
{
	switch (t->type){
	case TCHAR:
		if (t->c == '\n') print("TCHAR \\n\n");
		else print("TCHAR %C\n", t->c);
		break;
	case TEOF:
		print("TEOF\n");
		break;
	default:
		print("TYPE %d\n", t->type);
	}
	t_free(t);
}

int
gc(void) /* getchar func name is reserved by stdio.h */
{
	#define GCBUF 1024
	static char buf[GCBUF], *bp=buf+1;
	static long n = 0;
	if (bp > buf+n-1){
		n = read(0, buf, GCBUF);
		if (n <= 0) return -1;
		bp = buf;
	}
	bp++;
	return *(bp-1);
}

void
usage(void)
{
	fprint(2, "usage: %s [-m /n/dom] [-n 123]\n", argv0);
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	//Dir *d;
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
	/*
	d = dirstat(drpath);
	if (d==nil) sysfatal("%r");
	if ((d->mode & DMDIR) == 0) sysfatal("%s - not a directory", drpath);
	if (chdir(drpath) == 0) sysfatal("%r");
	if (tpath == nil) {
		char *buf[128];
		long n;
		int fd;
		fd = open("new");
		if (fd < 0) sysfatal("can't open %s/new. %r", drpath);
		n = read(fd, buf, 128);
		if (n <= 0) sysfatal("failed to read from %s/new. %r", drpath);
		tpath = mallocz(n+1);
		memmove(tpath, buf, n);
		close(fd);
		fprint(1, "%s/%s\n", drpath, tpath);
	}
	if (chdir(tpath) == 0) sysfatal("%r");
	*/

	print("--- START ---\n");
	teof = 0;
	ctempbuf = s_new();
	while(teof == 0){
		if (tstate >= TMAX) {
			fprint(2, "unknown tstate %d\n", tstate);
			break;
		}
		if (tstab[tstate] == nil) {
			fprint(2, "tstate %d not implemented\n", tstate);
			break;
		}
		tconsume();
		tstab[tstate]();
	}
	print("--- OVER ---\n");
}

#include <u.h>
#include <libc.h>
#include <String.h>
#include <thread.h>

#include "html5dom.h"
#include "ncref.h"

#define ALPHA(x) ((x >=0x41) && (x <= 0x7a))
#define DIGIT(x) ((x >=0x30) && (x <= 0x39))

Channel *outchannel;

int gc(void);



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

Token*
newtok(int type)
{
	Token *nt;
	nt = mallocz(sizeof(Token), 1);
	nt->type = type;
	nt->name = s_new();
	nt->attr = nil;
	return nt;
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
	t->attr[n-1] = mallocz(sizeof(Attr), 1);
	t->attr[n-1]->name = s_new();
	t->attr[n-1]->value = s_new();
	t->attr[n] = nil;
	return t->attr[n-1];
}

void
attr_free(Attr *attr)
{
	s_free(attr->name);
	s_free(attr->value);
	free(attr);
}

u32int insertion_mode = IMinitial;

/* Tokenizer vars and funcs */

Rune tc;
int treconsume = 0;
int teof;

Token *ctoken;
Attr *cattr;
String *ctempbuf;
String *clookaheadbuf;

void tconsume(void);
void temit(Token*);
void temitbuf(String*);
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
void tscommentenddash(void);
void tscommentend(void);
void tscommentendbang(void);
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
void tsdoctsysiddq(void);
void tsdoctsysidsq(void);
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
	[TSANAME_AFTER] = tsanameafter,
	[TSAVAL_BEFORE] = tsavalbefore,
	[TSAVAL_DQ] = tsavaldq,
	[TSAVAL_SQ] = tsavalsq,
	[TSAVAL_UQ] = tsavaluq,
	[TSAVAL_AFTER] = tsavalafter,
	[TSSCSTAG] = tsscstag,
	[TSBOGUS_COMMENT] = tsboguscomment,
	[TSMKUP_OPEN] = tsmkupopen,
	[TSCOMMENT_START] = tscommentstart,
	[TSCOMMENT_START_DASH] = tscommentstartdash,
	[TSCOMMENT] = tscomment,
	[TSCOMMENT_LESS] = tscommentless,
	[TSCOMMENT_LESS_BANG] = tscommentlessbang,
	[TSCOMMENT_LESS_BANG_DASH] = tscommentlessbangdash,
	[TSCOMMENT_LESS_BANG_DDASH] = tscommentlessbangddash,
	[TSCOMMENT_END_DASH] = tscommentenddash,
	[TSCOMMENT_END] = tscommentend,
	[TSCOMMENT_END_BANG] = tscommentendbang,
	[TSDOCT] = tsdoct,
	[TSDOCT_BEFORE] = tsdoctbefore,
	[TSDOCT_NAME] = tsdoctname,
	[TSDOCT_NAME_AFTER] = tsdoctnameafter,
	[TSDOCT_PUBK_AFTER] = tsdoctpubkafter,
	[TSDOCT_PUBID_BEFORE] = tsdoctpubidbefore,
	[TSDOCT_PUBID_DQ] = tsdoctpubiddq,
	[TSDOCT_PUBID_SQ] = tsdoctpubidsq,
	[TSDOCT_PUBID_AFTER] = tsdoctpubidafter,
	[TSDOCT_BETWEEN] = tsdoctbetween,
	[TSDOCT_SYSK_AFTER] = tsdoctsyskafter,
	[TSDOCT_SYSID_BEFORE] = tsdoctsysidbefore,
	[TSDOCT_SYSID_DQ] = tsdoctsysiddq,
	[TSDOCT_SYSID_SQ] = tsdoctsysidsq,
	[TSDOCT_SYSID_AFTER] = tsdoctsysidafter,
	[TSDOCT_BOGUS] = tsdoctbogus,
	[TSCDAT] = tscdat,
	[TSCDAT_BRK] = tscdatbrk,
	[TSCDAT_END] = tscdatend,
	[TSCREF] = tscref,
	[TSNCREF] = tsncref,
	[TSAMAM] = tsamam,
	[TSNUMREF] = tsnumref,
	[TSHEXREF_START] = tshexrefstart,
	[TSDECREF_START] = tsdecrefstart,
	[TSHEXREF] = tshexref,
	[TSDECREF] = tsdecref,
	[TSNUMREF_END] = tsnumrefend,
};

int tstate = TSDATA;
int treturn = -1;

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
		fprint(2, "unexpected equals sign before attribute name parse error, tc='%c'\n", tc);
		cattr = tnewattr(ctoken);
		s_putc(cattr->name, tc);
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
		s_terminate(cattr->name);
		tstate = TSANAME_AFTER;
		break;
	case '=':
		tstate = TSAVAL_BEFORE;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
		s_putc(cattr->name, REPCHAR);
		break;
	case '"':
	case '\'':
	case '<':
		fprint(2, "unexpected character in attribute name parse error, tc='%c'\n", tc);
	default:
		s_putc(cattr->name, tc);
	}
	/* TODO check for duplicate attribute names on leaving or emitting */
}

void
tsanameafter(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		break;
	case '/':
		tstate = TSSCSTAG;
		break;
	case '=':
		tstate = TSAVAL_BEFORE;
		break;
	case '>':
		tstate = TSDATA;
		s_terminate(ctoken->name);
		temit(ctoken);
		break;
	case -1: /* EOF */
		fprint(2, "eof in tag parse error\n");
		temit(eoftok());
		break;
	default:
		cattr = tnewattr(ctoken);
		treconsume = 1;
		tstate = TSANAME;
	}
}

void
tsavalbefore(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		break;
	case '"':
		tstate = TSAVAL_DQ;
		break;
	case '\'':
		tstate = TSAVAL_SQ;
		break;
	case '>':
		fprint(2, "missing attribute value parse error\n");
		s_terminate(ctoken->name);
		temit(ctoken);
		tstate = TSDATA;
		break;
	default:
		treconsume = 1;
		tstate = TSAVAL_UQ;
	}
}

void
tsavaldq(void)
{
	switch (tc) {
	case '"':
		tstate = TSAVAL_AFTER;
		break;
	case '&':
		treturn = TSAVAL_DQ;
		tstate = TSCREF;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		s_putc(cattr->value, REPCHAR);
		break;
	case -1: /* EOF */
		fprint(2, "oef in tag parse error\n");
		temit(eoftok());
		break;
	default:
		s_putc(cattr->value, tc);
	}		
}

void
tsavalsq(void)
{
	switch (tc) {
	case '\'':
		tstate = TSAVAL_AFTER;
		break;
	case '&':
		treturn = TSAVAL_SQ;
		tstate = TSCREF;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		s_putc(cattr->value, REPCHAR);
		break;
	case -1: /* EOF */
		fprint(2, "oef in tag parse error\n");
		temit(eoftok());
		break;
	default:
		s_putc(cattr->value, tc);
	}
}

void
tsavaluq(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		s_terminate(cattr->value);
		tstate = TSANAME_BEFORE;
		break;
	case '&':
		treturn = TSAVAL_UQ;
		tstate = TSCREF;
		break;
	case '>':
		s_terminate(ctoken->name);
		s_terminate(cattr->value);
		tstate = TSDATA;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		s_putc(cattr->value, REPCHAR);
		break;
	case -1: /* EOF */
		fprint(2, "oef in tag parse error\n");
		temit(eoftok());
		break;	case '"':
	case '\'':
	case '<':
	case '=':
	case '`':
		fprint(2, "unexpected character in unquoted attribute value parse error\n");
	default:
		s_putc(cattr->value, tc);
	}
}

void
tsavalafter(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		tstate = TSANAME_BEFORE;
		break;
	case '/':
		ctoken->flags |=  TSSCSTAG;
		break;
	case '>':
		s_terminate(ctoken->name);
		s_terminate(cattr->value);
		temit(ctoken);
		tstate = TSDATA;
		break;
	case -1: /* EOF */
		fprint(2, "eof in tag parse error\n");
		temit(eoftok());
		break;
	default:
		fprint(2, "missing whitespace between attributes parse error\n");
		treconsume = 1;
		tstate = TSANAME_BEFORE;
	}
}

void
tsscstag(void)
{
	switch (tc) {
	case '>':
		ctoken->flags |= TF_SELF_CLOSING;
		tstate = TSDATA;
		temit(ctoken);
		break;
	case -1:
		fprint(2, "eof in tag parse error\n");
		temit(eoftok());
		break;
	default:
		fprint(2, "unxpected solidus in tag parse error\n");
		treconsume = 1;
		tstate = TSANAME_BEFORE;
	}
}

void
tsboguscomment(void)
{
	fprint(2, "tsboguscomment not implemented\n");
	tstate = TSDATA;
}

void
tsmkupopen(void)
{
	int i;
	String *mbuf, *lowered;
	mbuf = s_new();
	s_putc(mbuf, tc);
	tconsume();
	s_putc(mbuf, tc);
	if (strncmp(s_to_c(mbuf), "--", 2) == 0) {
		ctoken = newtok(TCOMM);
		tstate = TSCOMMENT_START;
		s_free(mbuf);
		return;
	}
	for (i = 0; i < 5; i++) {
		tconsume();
		s_putc(mbuf, tc);
	}
	if (strncmp(s_to_c(mbuf), "[CDATA[", 7) == 0) {
		/* TODO: check if adjusted current node */
		tstate = TSCDAT;
		s_free(mbuf);
		return;
	}
	lowered = s_copy(s_to_c(mbuf));
	s_tolower(lowered);
	if (strncmp(s_to_c(lowered), "doctype", 7) == 0) {
		tstate = TSDOCT;
		s_free(mbuf);
		s_free(lowered);
		return;
	}
	fprint(2, "incorrectly opened comment parse error, tc='%c'\n", tc);
	ctoken = newtok(TCOMM);
	tstate = TSBOGUS_COMMENT;
	s_append(clookaheadbuf, s_to_c(mbuf));
	s_free(lowered);
	s_free(mbuf);
}

void
tscommentstart(void)
{
	fprint(2, "tscommentstart not implemented\n");
	tstate = TSDATA;
}

void
tscommentstartdash(void)
{
	fprint(2, "tscommentstartdash not implemented\n");
	tstate = TSDATA;
}

void
tscomment(void)
{
	fprint(2, "tscomment not implemented\n");
	tstate = TSDATA;
}

void
tscommentless(void)
{
	fprint(2, "tscommentless not implemented\n");
	tstate = TSDATA;
}

void
tscommentlessbang(void)
{
	fprint(2, "tscommentlessbang not implemented\n");
	tstate = TSDATA;
}

void
tscommentlessbangdash(void)
{
	fprint(2, "tscommentlessbangdash not implemented\n");
	tstate = TSDATA;
}

void
tscommentlessbangddash(void)
{
	fprint(2, "tscommentlessbangddash not implemented\n");
	tstate = TSDATA;
}

void
tscommentenddash(void)
{
	fprint(2, "tscommentenddash not implemented\n");
	tstate = TSDATA;
}

void
tscommentend(void)
{
	fprint(2, "tscommentend not implemented\n");
	tstate = TSDATA;
}

void
tscommentendbang(void)
{
	fprint(2, "tscommentendbang not implemented\n");
	tstate = TSDATA;
}

void
tsdoct(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		tstate = TSDOCT_BEFORE;
		break;
	case '>':
		treconsume = 1;
		tstate = TSDOCT_BEFORE;
		break;
	case -1: /* eof */
		fprint(2, "eof in doctype parse error, tc='%c'\n", tc);
		ctoken = newtok(TDOCT);
		ctoken->flags |= TF_FORCE_QUIRKS;
		s_terminate(ctoken->name);
		temit(ctoken);
		break;
	default:
		fprint(2, "missing whitespace before doctype name parse error, tc='%c'\n", tc);
		treconsume = 1;
		tstate = TSDOCT_BEFORE;
	}
}

void
tsdoctbefore(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		break;
	case '\0':
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
		ctoken = newtok(TDOCT);
		s_putc(ctoken->name, REPCHAR);
		tstate = TSDOCT_NAME;
		break;
	case '>':
		fprint(2, "missing doctype name parse error, tc='%c'\n", tc);
		ctoken = newtok(TDOCT);
		ctoken->flags |= TF_FORCE_QUIRKS;
		s_terminate(ctoken->name);
		temit(ctoken);
		break;
	case -1: /* EOF */
		fprint(2, "eof in doctype parse error, tc='%c'\n", tc);
		ctoken = newtok(TDOCT);
		ctoken->flags |= TF_FORCE_QUIRKS;
		s_terminate(ctoken->name);
		temit(ctoken);
		temit(eoftok());
		break;
	default:
		if (tc < 'a') tc += 0x20;
		ctoken = newtok(TDOCT);
		s_putc(ctoken->name, tc);
		tstate = TSDOCT_NAME;
	}
}

void
tsdoctname(void)
{
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		tstate = TSDOCT_NAME_AFTER;
		break;
	case '>':
		tstate = TSDATA;
		s_terminate(ctoken->name);
		temit(ctoken);
		break;
	case '\0':
		fprint(2, "unexpected null character parse error\n");
		s_putc(ctoken->name, REPCHAR);
		break;
	case -1: /* EOF */
		fprint(2, "eof in doctype parse error\n");
		ctoken->flags |= TF_FORCE_QUIRKS;
		s_terminate(ctoken->name);
		temit(ctoken);
		temit(eoftok());
		break;
	default:
		talpha(1);
	}
}

void
tsdoctnameafter(void)
{
	fprint(2, "tsdoctnameafter not implemented\n");
	tstate = TSDATA;
}

void
tsdoctpubkafter(void)
{
	fprint(2, "tsdoctpubkafter not implemented\n");
	tstate = TSDATA;
}

void
tsdoctpubidbefore(void)
{
	fprint(2, "tsdoctpubidbefore not implemented\n");
	tstate = TSDATA;
}

void
tsdoctpubiddq(void)
{
	fprint(2, "tsdoctpubiddq not implemented\n");
	tstate = TSDATA;
}

void
tsdoctpubidsq(void)
{
	fprint(2, "tsdoctpubidsq not implemented\n");
	tstate = TSDATA;
}

void
tsdoctpubidafter(void)
{
	fprint(2, "tsdoctpubidafter not implemented\n");
	tstate = TSDATA;
}

void
tsdoctbetween(void)
{
	fprint(2, "tsdoctbetween not implemented\n");
	tstate = TSDATA;
}

void
tsdoctsyskafter(void)
{
	fprint(2, "tsdoctsyskafter not implemented\n");
	tstate = TSDATA;
}

void
tsdoctsysidbefore(void)
{
	fprint(2, "tsdoctsysidbefore not implemented\n");
	tstate = TSDATA;
}

void
tsdoctsysiddq(void)
{
	fprint(2, "tsdoctsysiddq not implemented\n");
	tstate = TSDATA;
}

void
tsdoctsysidsq(void)
{
	fprint(2, "tsdoctsysidsq not implemented\n");
	tstate = TSDATA;
}

void
tsdoctsysidafter(void)
{
	fprint(2, "tsdoctsysidafter not implemented\n");
	tstate = TSDATA;
}

void
tsdoctbogus(void)
{
	fprint(2, "tsdoctbogus not implemented\n");
	tstate = TSDATA;
}

void
tscdat(void)
{
	fprint(2, "tscdat not implemented\n");
	tstate = TSDATA;
}

void
tscdatbrk(void)
{
	fprint(2, "tscdatbrk not implemented\n");
	tstate = TSDATA;
}

void
tscdatend(void)
{
	fprint(2, "tscdatend not implemented\n");
	tstate = TSDATA;
}

void
tscref(void)
{
	if ((ALPHA(tc)) || (DIGIT(tc))) {
		treconsume = 1;
		tstate = TSNCREF;
		return;
	}
	switch (tc) {
	case '#':
		s_putc(ctempbuf, tc);
		tstate = TSNUMREF;
		break;
	default:
		treconsume = 1;
		s_terminate(ctempbuf);
		s_append(cattr->value, s_to_c(ctempbuf));
		s_reset(ctempbuf);
		tstate = treturn;	
	}
	fprint(2, "tscref not implemented\n");
	tstate = TSDATA;
}

void
tsncref(void)
{
	fprint(2, "tsncref not implemented\n");
	tstate = treturn;
}

void
tsamam(void)
{
	fprint(2, "tsamam not implemented\n");
	tstate = TSDATA;
}

void
tsnumref(void)
{
	fprint(2, "tsnumref not implemented\n");
	tstate = TSDATA;
}

void
tshexrefstart(void)
{
	fprint(2, "tshexrefstart not implemented\n");
	tstate = TSDATA;
}

void
tsdecrefstart(void)
{
	fprint(2, "tsdecrefstart not implemented\n");
	tstate = TSDATA;
}

void
tshexref(void)
{
	fprint(2, "tshexref not implemented\n");
	tstate = TSDATA;
}

void
tsdecref(void)
{
	fprint(2, "tsdecref not implemented\n");
	tstate = TSDATA;
}

void
tsnumrefend(void)
{
	fprint(2, "tsnumrefend not implemented\n");
	tstate = TSDATA;
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
		temitbuf(ctempbuf);
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
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
		temit(chartok(REPCHAR));
		break;
	case -1: /* EOF */
		fprint(2, "eof in scipt html comment like text parse error, tc='%c'\n", tc);
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
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
		tstate = TSSCRIPT_ESC;
		temit(chartok(REPCHAR));
		break;
	case -1:
		fprint(2, "eof in script html comment like text parse error, tc='%c'\n", tc);
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
	fprint(2, "tsscriptescddash not implemented\n");
	tstate = TSDATA;
}


void
tsscriptescless(void)
{
	fprint(2, "tsscriptescless not implemented\n");
	tstate = TSDATA;
}


void
tsscriptescendopen(void)
{
	fprint(2, "tsscriptescendopen not implemented\n");
	tstate = TSDATA;
}


void
tsscriptescendname(void)
{
	fprint(2, "tsscriptescendname not implemented\n");
	tstate = TSDATA;
}


void
tsscriptdescstart(void)
{
	fprint(2, "tsscriptdescstart not implemented\n");
	tstate = TSDATA;
}


void
tsscriptdesc(void)
{
	fprint(2, "tsscriptdesc not implemented\n");
	tstate = TSDATA;
}


void
tsscriptdescdash(void)
{
	fprint(2, "tsscriptdescdash not implemented\n");
	tstate = TSDATA;
}


void
tsscriptdescddash(void)
{
	fprint(2, "tsscriptdescddash not implemented\n");
	tstate = TSDATA;
}


void
tsscriptdescless(void)
{
	fprint(2, "tsscriptdescless not implemented\n");
	tstate = TSDATA;
}


void
tsscriptdescend(void)
{
	fprint(2, "tsscriptdescend not implemented\n");
	tstate = TSDATA;
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
		temitbuf(ctempbuf);
		treconsume = 1;
		tstate = TSRAWT;
	}
}

void
tsrawtendopen(void)
{
	if (ALPHA(tc) != 0) {
		ctoken = newtok(TEND);
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
		temitbuf(ctempbuf);
		treconsume = 1;
		tstate = TSRCDT;
	}
}

void
tsrcdtendopen(void)
{
	if (ALPHA(tc) != 0) {
		ctoken = newtok(TEND);
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
	switch (tc) {
	case '\t':
	case '\n':
	case '\r':
	case ' ':
		s_terminate(ctoken->name);
		tstate = TSANAME_BEFORE;
		break;
	case '/':
		s_terminate(ctoken->name);
		tstate = TSSCSTAG;
		break;
	case '>':
		s_terminate(ctoken->name);
		temit(ctoken);
		tstate = TSDATA;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
		s_putc(ctoken->name, REPCHAR);
		break;
	case -1:
		fprint(2, "eof in tag parse error, tc='%c'\n", tc);
		teof = 1;
		temit(eoftok());
		break;
	default:
		talpha(1);
	} 
}

void
tsetagopen(void)
{
	if (ALPHA(tc) != 0) {
		ctoken = newtok(TEND);
		treconsume = 1;
		tstate = TSTAG_NAME;
	} else switch (tc) {
	case '>':
		fprint(2, "missing end tag name parse error, tc='%c'\n", tc);
		tstate = TSDATA;
		break;
	case -1:
		fprint(2, "eof before tag name parse error, tc='%c'\n", tc);
		temit(chartok('<'));
		teof = 1;
		temit(eoftok());
		break;
	default:
		fprint(2, "invalid first character of tag name parse error, tc='%c'\n", tc);
		ctoken = newtok(TCOMM);
		treconsume = 1;
		tstate = TSBOGUS_COMMENT;
	}
}

void
tstagopen(void)
{
	if (ALPHA(tc) != 0) {
		ctoken = newtok(TSTART);
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
		fprint(2, "unexpected question mark instead of tag name parse error, tc='%c'\n", tc);
		ctoken = newtok(TCOMM);
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
		fprint(2, "invalid first character of tag name parse error, tc='%c'\n", tc);
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
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
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
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
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
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
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
		treturn = TSRCDT;
		tstate = TSCREF;
		break;
	case '<':
		tstate = TSRCDT_LESS;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
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
		treturn = TSDATA;
		tstate = TSCREF;
		break;
	case '<':
		tstate = TSTAG_OPEN;
		break;
	case '\0':
		fprint(2, "unexpected null character parse error, tc='%c'\n", tc);
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
	if (ALPHA(tc) == 0) return 0;
	s_putc(ctempbuf, tc);
	if ((tolower != 0) && (tc < 'a')) tc+=0x20;
	s_putc(ctoken->name, tc);
	return 1;
}

void
tconsume(void)
{
	char *buf;
	if (treconsume != 0) {
		treconsume = 0;
		return;
	}
	buf = s_to_c(clookaheadbuf);
	if (buf[0] != '\0') {
		tc = buf[0];
		print("tc = %uX\n", tc);
		/* TODO make this code utf-aware */
		String *shift;
		shift = s_copy(buf+1);
		s_free(clookaheadbuf);
		clookaheadbuf = shift;
	}
	else tc = gc();
}

void
temitbuf(String *str)
{
	Rune r;
	char *buf;
	int n, len;
	buf = s_to_c(str);
	len = strlen(buf);
	for (n = 0; n < len; n += chartorune(&r, buf+n)){
		temit(chartok(r));
	}
	
}

void
temit(Token *t)
{
	send(outchannel, &t);
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
threadtokenize(void *v)
{
	Tokctl *tc;
	tc = v;
	outchannel = tc->c;
	teof = 0;
	threadsetname("tokenizer");
	ctempbuf = s_new();
	clookaheadbuf = s_new();
	while (teof == 0) {
		if (tstate >= TMAX) {
			fprint(2, "[TOKENIZER] unknown tstate %d\n", tstate);
			break;
		}
		tconsume();
		tstab[tstate]();
	}
}

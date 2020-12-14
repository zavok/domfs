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
	TEOF = -1,
};

enum { /* Token flags */
	TF_FORCE_QUIRKS = 1,
	TF_SELF_CLOSING = 1 << 1,
};

typedef struct Token Token;

struct Token {
	int type;
	u64int flags;
	Rune c;
	String *name;
	Attr **attr;
};

Token* chartok(Rune);
Token* eoftok(void);
Token* newtok(int);
void t_free(Token*);
Attr* tnewattr(Token*);
void attr_free(Attr*);

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

typedef struct Tokctl Tokctl;
struct Tokctl {
	Channel *c;
};

typedef struct Treeconstrctl Treeconstrctl;
struct Treeconstrctl {
	char *treeroot;
	Channel *in;
};

void threadtokenize(void*);
void threadtreeconstr(void*);

/*
 * I will be exstensively using DOM Standard style for names and such
 * in this file, despite my better judgement. 
 */

/* node types */
enum {
	ELEMENT_NODE                = 1,
	ATTRIBUTE_NODE              = 2,
	TEXT_NODE                   = 3,
	CDATA_SECTION_NODE          = 4,
	ENTITY_REFERENCE_NODE       = 5, /* legacy */
	ENTITY_NODE                 = 6, /* legacy */
	PROCESSING_INSTRUCTION_NODE = 7,
	COMMENT_NODE                = 8,
	DOCUMENT_NODE               = 9,
	DOCUMENT_TYPE_NODE          = 10,
	DOCUMENT_FRAGMENT_NODE      = 11,
	NOTATION_NODE               = 12, /* legacy /*
};

typedef struct Node Node;

struct Node {
	u8int nodeType;
	char *nodeName;
	char *baseURI;
	int isConnected;
	Node *ownerDocument;
	Node *parentNode;
	Node *parentElement;
	Node **childNodes;
	Node *firstChild;
	Node *lastChild;
	Node *previousSibling;
	Node *nextSibling;
	char *nodeValue;
	char *textContent;
/* Document specific */
	char *URL;
	char *documentURI;
	char *compatMode;
	char *characterSet;
	char *charset; /* legacy */
	char *inputEncoding; /* legacy */
	char *contentType;
	Node *doctype;
	Node *documentElement;
/* DocumentType specific */
	char *name;
	char *publicId;
	char *systemId;
/* Element specific */
	char *namespaceURI;
	char *prefix;
	char *localName;
	char *tagName;
	char *id;
	char *className;
	TokenList classList; /* ??? */
	char *slot;
	NamedNodeMap attributes; /* ??? */
/* Text specific */
	char *wholeText;
/* ProcessingInstruction specific */
	char *target;
/* CharacterDataSpecific */
	char *data;
	ulong length;
};

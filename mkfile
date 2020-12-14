</$objtype/mkfile

TARG=\
	domfs\
	html5dom\

HFILES=\
	html5dom.h\
	ncref.h\

BIN=/$objtype/bin
</sys/src/cmd/mkmany

$O.html5dom: tok.$O tree.$O

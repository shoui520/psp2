/*
 * _Files: the standard streams. stdin reads one character at a time (its _Cbuf), stdout
 * is line buffered and stderr not buffered, each with 80 bytes; their handles are 0-2.
 */

#include "xstdio.h"

static unsigned char ebuf[80];
static unsigned char obuf[80];

FILE _Stdin = {
	_MOPENR | _MLBF, 0, 0,
	&_Stdin._Cbuf, &_Stdin._Cbuf + 1, &_Stdin._Cbuf,
	&_Stdin._Cbuf, &_Stdin._Cbuf, _Stdin._Back + sizeof(_Stdin._Back),
	_Stdin._WBack + sizeof(_Stdin._WBack) / sizeof(_Stdin._WBack[0]),
};

FILE _Stdout = {
	_MOPENW | _MLBF, 1, 1,
	obuf, obuf + sizeof(obuf), obuf,
	obuf, obuf, _Stdout._Back + sizeof(_Stdout._Back),
	_Stdout._WBack + sizeof(_Stdout._WBack) / sizeof(_Stdout._WBack[0]),
};

FILE _Stderr = {
	_MOPENW | _MNBF, 2, 2,
	ebuf, ebuf + sizeof(ebuf), ebuf,
	ebuf, ebuf, _Stderr._Back + sizeof(_Stderr._Back),
	_Stderr._WBack + sizeof(_Stderr._WBack) / sizeof(_Stderr._WBack[0]),
};

FILE *_Files[FOPEN_MAX] = { &_Stdin, &_Stdout, &_Stderr };

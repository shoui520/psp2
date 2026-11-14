// debug.cpp: _Debug_message, which reports a failed library check on stderr and aborts, and
// _Throw, which reports an exception raised with exceptions off
#include <cstdio>
#include <cstdlib>
#include <exception>

_STD_BEGIN
void _Debug_message(const char *, const char *, unsigned int);
void _Debug_message(const char *, const char *);

void _Debug_message(const char *message, const char *file, unsigned int line)
	{	// report error and die
	_CSTD fflush(0);
	_CSTD fputs("error: ", stderr);
	_CSTD fputs(message, stderr);
	if (file != 0)
		{	// put the file name and line number
		_CSTD fputs(": ", stderr);
		_CSTD fputs(file, stderr);
		if (line != 0)
			{	// put the line number, generated low digit first
			char buf[16];
			char *p = &buf[sizeof (buf) - 1];

			*p = '\0';
			for (; line != 0 && buf < --p; line /= 10)
				*p = (char)(line % 10 + '0');
			_CSTD fputs(": line ", stderr);
			_CSTD fputs(p, stderr);
			}
		}
	_CSTD fputs("\n", stderr);
	_CSTD abort();
	}

void _Debug_message(const char *message, const char *file)
	{	// report error and die, no line number
	_Debug_message(message, file, 0);
	}

void _Throw(const exception& ex)
	{	// report the exception and die
	_Debug_message("exception", ex.what(), 0);
	}
_STD_END

// fiopen.cpp: _Fiopen, open a file for a basic_filebuf with an ios_base::openmode
#include <fstream>
_STD_BEGIN

FILE *_Fiopen(const char *filename, ios_base::openmode mode, int)
	{	// open a file with a native name
	static const char *mods[] =
		{	// fopen mode strings corresponding to valid[i]
		"r", "w", "w", "a", "rb", "wb", "wb", "ab",
		"r+", "w+", "a+", "r+b", "w+b", "a+b", 0};

	static const int valid[] =
		{	// valid combinations of open flags
		ios_base::in,
		ios_base::out,
		ios_base::out | ios_base::trunc,
		ios_base::out | ios_base::app,
		ios_base::in | ios_base::binary,
		ios_base::out | ios_base::binary,
		ios_base::out | ios_base::trunc | ios_base::binary,
		ios_base::out | ios_base::app | ios_base::binary,
		ios_base::in | ios_base::out,
		ios_base::in | ios_base::out | ios_base::trunc,
		ios_base::in | ios_base::out | ios_base::app,
		ios_base::in | ios_base::out | ios_base::binary,
		ios_base::in | ios_base::out | ios_base::trunc
			| ios_base::binary,
		ios_base::in | ios_base::out | ios_base::app
			| ios_base::binary,
		0};

	FILE *fp;
	int n;
	ios_base::openmode atendflag = mode & ios_base::ate;
	ios_base::openmode norepflag = mode & ios_base::_Noreplace;

	if (mode & ios_base::_Nocreate)
		mode |= ios_base::in;	// file must exist
	if (mode & ios_base::app)
		mode |= ios_base::out;	// extension -- app implies out

	mode &= ~(ios_base::ate | ios_base::_Nocreate | ios_base::_Noreplace);
	for (n = 0; valid[n] != 0 && valid[n] != mode; ++n)
		;	// look for a valid mode

	if (valid[n] == 0)
		return (0);	// no valid mode
	else if (norepflag && (mode & ios_base::in)
		&& (fp = _CSTD fopen(filename, "r")) != 0)
		{	// file must not exist, close and fail
		_CSTD fclose(fp);
		return (0);
		}
	else if ((fp = _CSTD fopen(filename, mods[n])) == 0)
		return (0);	// open failed

	if (!atendflag || _CSTD fseek(fp, 0, SEEK_END) == 0)
		return (fp);	// no need to seek to end, or seek succeeded

	_CSTD fclose(fp);	// can't position at end
	return (0);
	}

FILE *_Fiopen(const wchar_t *filename, ios_base::openmode mode, int prot)
	{	// open a file with a wide name, converted to a multibyte name
	char buf[FILENAME_MAX];

	if (_CSTD wcstombs(buf, filename, sizeof (buf)) == (size_t)(-1))
		return (0);
	return (_Fiopen(buf, mode, prot));
	}
_STD_END

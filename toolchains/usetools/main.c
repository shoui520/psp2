/*
 * useasm: assembles a USE program into its little-endian binary.
 *
 *	useasm -o OUTPUT SOURCE...
 *
 * The sources are read in order as one program. A line holds a label ("name:"), an
 * instruction in the syntax of Imagination's disassembler (parsed by useparse.c and
 * encoded by Imagination's encoder), or ".word" with 32-bit data values (an even number
 * of them over the program, since an instruction is two words), or ".align2", which pads
 * with one zero instruction to an even instruction address. Comments are C's.
 * Branches name a label or give a number; labels count in instructions.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "useparse.h"

typedef struct Label {
	char *name;
	IMG_UINT32 address;
} Label;

typedef struct Line {
	char *text;
	const char *file;
	unsigned lineNumber;
} Line;

static Label *labels;
static size_t labelCount;
static Line *lines;
static size_t lineCount;
static const Line *current;
static int errors;

static void error(const Line *line, const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%u: error: ", line->file, line->lineNumber);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	errors++;
}

static void *xrealloc(void *p, size_t size)
{
	p = realloc(p, size);
	if (p == NULL) {
		fprintf(stderr, "useasm: out of memory\n");
		exit(1);
	}
	return p;
}

static char *xstrdup(const char *s)
{
	return strcpy(xrealloc(NULL, strlen(s) + 1), s);
}

/* reads a source into lines, without its comments and surrounding blanks */
static int readSource(const char *path)
{
	FILE *f = fopen(path, "r");
	char buf[1024];
	unsigned lineNumber = 0;
	int inComment = 0;

	if (f == NULL) {
		fprintf(stderr, "useasm: cannot open %s\n", path);
		return 0;
	}
	while (fgets(buf, sizeof(buf), f) != NULL) {
		char out[1024];
		size_t n = 0;
		char *s = buf;

		lineNumber++;
		while (*s != '\0' && *s != '\n') {
			if (inComment) {
				if (s[0] == '*' && s[1] == '/') {
					inComment = 0;
					s += 2;
				} else {
					s++;
				}
			} else if (s[0] == '/' && s[1] == '*') {
				inComment = 1;
				s += 2;
			} else if (s[0] == '/' && s[1] == '/') {
				break;
			} else {
				out[n++] = *s++;
			}
		}
		while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t' || out[n - 1] == '\r'))
			n--;
		out[n] = '\0';
		s = out;
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s == '\0')
			continue;
		lines = xrealloc(lines, (lineCount + 1) * sizeof(*lines));
		lines[lineCount].text = xstrdup(s);
		lines[lineCount].file = path;
		lines[lineCount].lineNumber = lineNumber;
		lineCount++;
	}
	fclose(f);
	return 1;
}

/* a label definition: "name:" alone on its line */
static int isLabel(const char *s, size_t *len)
{
	size_t n = strspn(s, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$.");

	if (n == 0 || s[n] != ':' || s[n + 1] != '\0')
		return 0;
	*len = n;
	return 1;
}

static int findLabel(void *context, const char *name, IMG_UINT32 *label)
{
	size_t i;

	(void)context;
	for (i = 0; i < labelCount; i++) {
		if (strcmp(labels[i].name, name) == 0) {
			*label = (IMG_UINT32)i;
			return 1;
		}
	}
	return 0;
}

/* the 32-bit values of a .word line */
static int parseWords(const Line *line, IMG_UINT32 *words, int max)
{
	const char *s = line->text + 5;
	int count = 0;

	for (;;) {
		char *end;
		unsigned long v;

		while (*s == ' ' || *s == '\t')
			s++;
		v = strtoul(s, &end, 0);
		if (end == s || count == max) {
			error(line, "bad .word");
			return -1;
		}
		words[count++] = (IMG_UINT32)v;
		s = end;
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s == '\0')
			return count;
		if (*s++ != ',') {
			error(line, "bad .word");
			return -1;
		}
	}
}

static IMG_PVOID ctxRealloc(IMG_PVOID context, IMG_PVOID old, IMG_UINT32 size, IMG_UINT32 oldSize)
{
	(void)context;
	(void)oldSize;
	return xrealloc(old, size);
}

static IMG_UINT32 ctxGetLabelAddress(IMG_PVOID context, IMG_UINT32 label)
{
	(void)context;
	return labels[label].address;
}

static IMG_VOID ctxSetLabelAddress(IMG_PVOID context, IMG_UINT32 label, IMG_UINT32 address)
{
	(void)context;
	labels[label].address = address;
}

static IMG_PCHAR ctxGetLabelName(IMG_PVOID context, IMG_UINT32 label)
{
	(void)context;
	return labels[label].name;
}

static IMG_VOID ctxError(IMG_PVOID context, PUSE_INST inst, IMG_CHAR *fmt, ...)
{
	char buf[512];
	va_list ap;

	(void)context;
	(void)inst;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	error(current, "%s", buf);
}

static IMG_VOID ctxLaddrNotify(IMG_UINT32 address)
{
	(void)address;
}

int main(int argc, char **argv)
{
	SGX_CORE_INFO core = { SGX_CORE_ID_543, 216 };
	PCSGX_CORE_DESC target = UseAsmGetCoreDesc(&core);
	USEASM_CONTEXT context = {
		NULL, NULL, ctxRealloc, ctxGetLabelAddress, ctxSetLabelAddress,
		ctxGetLabelName, ctxError, ctxLaddrNotify,
	};
	const char *output = NULL;
	IMG_UINT32 *code = NULL;
	IMG_UINT32 words = 0;
	FILE *f;
	size_t i;
	int arg;

	for (arg = 1; arg < argc; arg++) {
		if (strcmp(argv[arg], "-o") == 0 && arg + 1 < argc)
			output = argv[++arg];
		else if (!readSource(argv[arg]))
			return 1;
	}
	if (output == NULL || lineCount == 0 || target == NULL) {
		fprintf(stderr, "usage: useasm -o OUTPUT SOURCE...\n");
		return 1;
	}

	/* first pass: the labels' addresses */
	for (i = 0; i < lineCount; i++) {
		IMG_UINT32 data[64];
		size_t len;
		IMG_UINT32 existing;

		if (isLabel(lines[i].text, &len)) {
			lines[i].text[len] = '\0';
			if (findLabel(NULL, lines[i].text, &existing)) {
				error(&lines[i], "label '%s' defined twice", lines[i].text);
				continue;
			}
			labels = xrealloc(labels, (labelCount + 1) * sizeof(*labels));
			labels[labelCount].name = lines[i].text;
			labels[labelCount].address = words / 2;
			labelCount++;
			lines[i].text = NULL;
		} else if (strncmp(lines[i].text, ".word", 5) == 0) {
			int n = parseWords(&lines[i], data, 64);

			if (n > 0)
				words += (IMG_UINT32)n;
		} else if (strcmp(lines[i].text, ".align2") == 0) {
			if (words & 1)
				error(&lines[i], ".align2 after an odd number of data words");
			if (words & 2)
				words += 2;
		} else {
			if (words & 1)
				error(&lines[i], "instruction after an odd number of data words");
			words += 2;
		}
	}
	if (words & 1)
		error(&lines[lineCount - 1], "odd number of words");
	if (errors)
		return 1;

	/* second pass: the code */
	code = xrealloc(NULL, (words + 2) * sizeof(*code));
	words = 0;
	for (i = 0; i < lineCount; i++) {
		char parseError[USEPARSE_ERROR_SIZE];
		USE_INST inst;

		current = &lines[i];
		if (lines[i].text == NULL)
			continue;
		if (strncmp(lines[i].text, ".word", 5) == 0) {
			words += (IMG_UINT32)parseWords(&lines[i], code + words, 64);
			continue;
		}
		if (strcmp(lines[i].text, ".align2") == 0) {
			if (words & 2) {
				code[words] = code[words + 1] = 0;
				words += 2;
			}
			continue;
		}
		if (!UseParseInstruction(target, lines[i].text, words / 2, &inst, findLabel, NULL,
					 parseError)) {
			error(&lines[i], "%s", parseError);
			words += 2;
			continue;
		}
		inst.psNext = NULL;
		code[words] = code[words + 1] = 0;
		if (UseAssembleInstruction(target, &inst, code, code + words, 0, &context) != 2 &&
		    !errors)
			error(&lines[i], "not one instruction");
		words += 2;
	}
	if (errors)
		return 1;

	f = fopen(output, "wb");
	if (f == NULL || fwrite(code, sizeof(*code), words, f) != words || fclose(f) != 0) {
		fprintf(stderr, "useasm: cannot write %s\n", output);
		remove(output);
		return 1;
	}
	return 0;
}

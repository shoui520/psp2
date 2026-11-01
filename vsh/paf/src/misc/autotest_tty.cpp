/*
 * ScePafAutoTestTty: messages for an automatic tester on the extra TTY, plain
 * or as an XML document of their kind. Only the development kits' libpaf
 * exports it.
 */
#include <stdarg.h>
#include <stdint.h>
#include <std/string>
#include <std/stdio.h>
#include "../std/tty.h"

#if !defined(FW_TYPE_CEX)

#define AUTOTEST_XML_HEADER	"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"

/* what a message is: its XML element */
enum AutoTestTtyType {
	AUTOTEST_TTY_TEXT = 0,
	AUTOTEST_TTY_CLASS = 1,
	AUTOTEST_TTY_FOCUS = 2,
	AUTOTEST_TTY_EVENT = 3
};

/* The format, wrapped for its type, is printed to the extra TTY when it is open. */
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...)
{
	va_list args;
	char text[1024];

	va_start(args, format);
	if (extratty_is_open()) {
		paf::string fmt;

		switch (type) {
		case AUTOTEST_TTY_TEXT: {
			paf::string body(format);
			paf::string end("\n\n");

			fmt = body + end;
			break;
		}
		case AUTOTEST_TTY_CLASS: {
			paf::string header(AUTOTEST_XML_HEADER);
			paf::string open("<class>");
			paf::string head = header + open;
			paf::string body(format);
			paf::string message = head + body;
			paf::string close("</class>\n\n");

			fmt = message + close;
			break;
		}
		case AUTOTEST_TTY_FOCUS: {
			paf::string header(AUTOTEST_XML_HEADER);
			paf::string open("<focus>");
			paf::string head = header + open;
			paf::string body(format);
			paf::string message = head + body;
			paf::string close("</focus>\n\n");

			fmt = message + close;
			break;
		}
		case AUTOTEST_TTY_EVENT: {
			paf::string header(AUTOTEST_XML_HEADER);
			paf::string open("<event>");
			paf::string head = header + open;
			paf::string body(format);
			paf::string message = head + body;
			paf::string close("</event>\n\n");

			fmt = message + close;
			break;
		}
		}

		int len = sce_paf_vsnprintf(text, sizeof(text), fmt.c_str(), args);

		if (len > 0)
			write_to_extratty(text, len);
	}
}

#endif

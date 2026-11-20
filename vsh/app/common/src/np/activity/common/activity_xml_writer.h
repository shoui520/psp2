/*
 * The XML writer the generator writes its requests with: elements, attributes and text into a
 * caller's buffer, a start tag left open until its content or end comes. No binary names the
 * class, its file or its methods; the names are ours.
 */
#ifndef NP_ACTIVITY_COMMON_ACTIVITY_XML_WRITER_H
#define NP_ACTIVITY_COMMON_ACTIVITY_XML_WRITER_H

#include <scetypes.h>
#include "activity_object.h"

/* the longest element or attribute name, text and attribute value the writer measures */
#define ACTIVITY_XML_NAME_MAX		0x80
#define ACTIVITY_XML_TEXT_MAX		0x800
#define ACTIVITY_XML_VALUE_MAX		0x400
/* room for an int's text, its sign and the terminator */
#define ACTIVITY_XML_INT_SIZE		12

class ActivityXmlWriter : public ActivityObject {
public:
	ActivityXmlWriter();
	virtual ~ActivityXmlWriter();

	int SetBuffer(char *buffer, SceSize size);
	/* ends the document: *size gets its length, with the terminator */
	int Finish(SceSize *size);
	int WriteDeclaration(const char *declaration);
	int StartElement(const char *name);
	int WriteText(const char *text);
	int WriteInt(int value);
	int WriteTextString(const char *text);
	int EndElement(const char *name);
	int WriteAttribute(const char *name, const char *value);
	int WriteAttributeInt(const char *name, int value);

private:
	int closeStartTag();

	int m_opened;			/* 1 from SetBuffer to Finish */
	char *m_buffer;
	SceSize m_size;
	char *m_cursor;
	SceSize m_remaining;
	int m_startTagOpen;		/* 1: the last start tag still lacks its '>' */
	int m_elementEnded;		/* 1: the last thing written ended an element */
};

#endif

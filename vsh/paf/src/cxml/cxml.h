#ifndef PAF_CXML_CXML_H
#define PAF_CXML_CXML_H

#include <stdint.h>

typedef struct CxmlDocument CxmlDocument;

typedef struct CxmlElement {
	CxmlDocument *doc;
	int32_t offset;
} CxmlElement;

typedef char CxmlElementSizeCheck[sizeof(CxmlElement) == 8 ? 1 : -1];

#define CxmlElementGetFirstChild ScePafResource_B673D5D7

void CxmlElementGetFirstChild(CxmlElement *result, const CxmlElement *self);

#endif

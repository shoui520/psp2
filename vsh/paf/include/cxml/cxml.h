/*
 * paf::cxml, as far as converted C++ uses it: an element is a document and the
 * offset of the element in its tree. Names after ScePaf-RE.
 */
#ifndef PAF_CXML_CXML_H
#define PAF_CXML_CXML_H

#include <stdint.h>

namespace paf {
namespace cxml {

class Document;

class Element {
public:
	Document *doc;
	int32_t offset;
};

}
}

#endif

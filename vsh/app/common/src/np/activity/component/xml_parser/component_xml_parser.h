/*
 * The XML parser component: reads the replies, 72 bytes (Sony's file: component_xml_parser.cpp; the class name is ours). Its fields and
 * methods are written with component_xml_parser.cpp.
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_PARSER_COMPONENT_XML_PARSER_H
#define NP_ACTIVITY_COMPONENT_XML_PARSER_COMPONENT_XML_PARSER_H

#include "../abstract_component.h"

class ComponentXmlParser : public Component {
public:
	ComponentXmlParser();
	virtual ~ComponentXmlParser();
};

#endif

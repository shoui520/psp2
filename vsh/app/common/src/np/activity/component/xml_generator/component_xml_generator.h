/*
 * The XML generator component: writes the requests, 116 bytes (Sony's file: component_xml_generator.cpp; the class name is ours). Its fields and
 * methods are written with component_xml_generator.cpp.
 */
#ifndef NP_ACTIVITY_COMPONENT_XML_GENERATOR_COMPONENT_XML_GENERATOR_H
#define NP_ACTIVITY_COMPONENT_XML_GENERATOR_COMPONENT_XML_GENERATOR_H

#include "../abstract_component.h"

class ComponentXmlGenerator : public Component {
public:
	ComponentXmlGenerator();
	virtual ~ComponentXmlGenerator();
};

#endif

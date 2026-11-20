/*
 * The HTTP component: the requests' connection, 56 bytes (Sony's file: component_http.cpp; the class name is ours). Its fields and
 * methods are written with component_http.cpp.
 */
#ifndef NP_ACTIVITY_COMPONENT_HTTP_COMPONENT_HTTP_H
#define NP_ACTIVITY_COMPONENT_HTTP_COMPONENT_HTTP_H

#include "../abstract_component.h"

class ComponentHttp : public Component {
public:
	ComponentHttp();
	virtual ~ComponentHttp();
};

#endif

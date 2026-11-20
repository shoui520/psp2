/*
 * The authentication component: the NP ticket the requests are signed with, 20 bytes (Sony's file: component_auth.cpp; the class name is ours). Its fields and
 * methods are written with component_auth.cpp.
 */
#ifndef NP_ACTIVITY_COMPONENT_AUTH_COMPONENT_AUTH_H
#define NP_ACTIVITY_COMPONENT_AUTH_COMPONENT_AUTH_H

#include "../abstract_component.h"

class ComponentAuth : public Component {
public:
	ComponentAuth();
	virtual ~ComponentAuth();
};

#endif

/*
 * The model tag of a LiveArea's frame or liveitem: the system's model (vita, dolce), shown only
 * where one of its model tags names it.
 */
#include <std/string.h>
#include <system/system.h>
#include "livearea.h"

/* the model tag's names, in the order of paf's model types */
static paf::string s_liModels[2] = { "vita", "dolce" };

/* whether the system's model is one a model tag names (any, when there is no model tag) */
int LiCheckModel(LiParser *parser, paf::list<XmlElement> *children)
{
	bool hasModel = false;

	for (paf::list<XmlElement>::iterator child = children->begin(); child != children->end();
	     ++child) {
		if (g_liTagModel == child->name) {
			hasModel = true;

			unsigned int type = paf::system::Model::GetType();

			if (type < 2 && child->text == s_liModels[type])
				return hasModel;
		}
	}
	if (hasModel)
		return 0;
	return 1;
}

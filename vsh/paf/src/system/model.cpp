/* paf::system::Model: whether the console is a Vita or a Dolce (PS Vita TV). */
#include <system/system.h>

using namespace paf::system;

static Model s_model;

int32_t Model::GetType()
{
	return s_model.type;
}

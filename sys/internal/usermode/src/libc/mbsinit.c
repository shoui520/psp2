/* mbsinit function */
#include <wchar.h>

int mbsinit(const mbstate_t *pst)
{
	return pst == NULL || pst->_State == 0;
}

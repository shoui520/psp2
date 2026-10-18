/*
 * stdbool.h: the SDK's boolean type (SDK Release 03.570.011, target/include/stdbool.h).
 */
#ifndef _STDBOOL
#define _STDBOOL

#define __bool_true_false_are_defined	1

#ifndef __cplusplus
#define bool		_Bool
#define false		0
#define true		1
#endif

#endif /* _STDBOOL */

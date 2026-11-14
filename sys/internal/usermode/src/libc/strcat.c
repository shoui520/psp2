/* strcat function */
#include <string.h>

char *strcat(char *__restrict s1, const char *__restrict s2)
{
	char *s;

	for (s = s1; *s != '\0'; ++s)
		;
	for (; (*s = *s2) != '\0'; ++s, ++s2)
		;
	return s1;
}

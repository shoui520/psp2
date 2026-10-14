/* Descriptive source name: the original library has no debug records. */
unsigned int MarlinStringLength(const char *string)
{
	unsigned int length;

	for (length = 0; string[length] != 0; length++)
		;
	return length;
}

signed char *MarlinStringCopyBounded(signed char *destination,
				   const signed char *source, int size)
{
	signed char *start = destination;

	while (size--) {
		*destination = *source;
		if (*destination++ == 0)
			break;
		source++;
	}
	return start;
}

int MarlinStringCompare(const signed char *first, const signed char *second)
{
	int index;

	for (index = 0; first[index] != 0; index++) {
		if (first[index] != second[index])
			return first[index] > second[index] ? 1 : -1;
	}
	return second[index] != 0 ? -1 : 0;
}

signed char *MarlinStringFindCharacter(signed char *string, int character)
{
	while (*string != 0 && *string != (signed char)character)
		string++;
	return *string == character ? string : 0;
}

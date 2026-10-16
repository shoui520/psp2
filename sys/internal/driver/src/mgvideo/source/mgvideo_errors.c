/* Descriptive names: the original contains no source-file debug records. */

int translateParserError(int result)
{
	switch (result) {
	case -2:
		result = 0x8050C041;
		break;
	case -1:
		return 0x8050C003;
	case -15:
		return 0x8050C04E;
	case -8:
		return 0x8050C047;
	case -7:
		return 0x8050C046;
	case -6:
		return 0x8050C045;
	case -5:
		return 0x8050C044;
	case -4:
		return 0x8050C043;
	case -3:
		return 0x8050C042;
	default:
		break;
	}
	return result;
}

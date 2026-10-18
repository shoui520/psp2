/* Mapping-file parsing and overlay configuration. */

#include <sysmem/debug.h>
#include <iofilemgr/iofilemgr.h>
#include <fios2/fios2.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include "mapfile_internal.h"

extern int appmgrResolvePath(const char *path, SceUID pid, char *out,
			    SceSize size);
extern void *appmgrAllocMountHeapMemory(SceSize size);
extern int appmgrFreeMountHeapMemory(void *memory);
extern int appmgrIsDevelopmentPath(const char *path);

enum AppmgrMapFileAttributeKind {
	MAPFILE_BLANK,
	MAPFILE_VERSION,
	MAPFILE_APP_PATH,
	MAPFILE_SAVEDATA_PATH,
	MAPFILE_ADDCONT_PATH,
	MAPFILE_OVERLAY_TYPE,
	MAPFILE_OVERLAY_ORDER,
	MAPFILE_OVERLAY_SRC,
	MAPFILE_OVERLAY_DST,
};

typedef struct AppmgrMapFileAttribute {
	const char *name;
	enum AppmgrMapFileAttributeKind kind;
} AppmgrMapFileAttribute;

typedef struct AppmgrMapFileOverlayType {
	const char *name;
	SceFiosOverlayType type;
} AppmgrMapFileOverlayType;

static const AppmgrMapFileOverlayType mapFileOverlayTypes[] = {
	{ "SCE_FIOS_OVERLAY_TYPE_OPAQUE", SCE_FIOS_OVERLAY_TYPE_OPAQUE },
	{ "SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT", SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT },
	{ "SCE_FIOS_OVERLAY_TYPE_NEWER", SCE_FIOS_OVERLAY_TYPE_NEWER },
	{ "SCE_FIOS_OVERLAY_TYPE_WRITABLE", SCE_FIOS_OVERLAY_TYPE_WRITABLE },
	{ NULL, 0 },
};

static const AppmgrMapFileAttribute mapFileAttributes[] = {
	{ "version=", MAPFILE_VERSION },
	{ "app0=", MAPFILE_APP_PATH },
	{ "savedata0=", MAPFILE_SAVEDATA_PATH },
	{ "addcont0=", MAPFILE_ADDCONT_PATH },
	{ "otherApps=", MAPFILE_BLANK },
	{ "otherAppsPatches=", MAPFILE_BLANK },
	{ "otherAppsSavedata=", MAPFILE_BLANK },
	{ "otherAppsContents=", MAPFILE_BLANK },
	{ "overlay1type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay1order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay1src=", MAPFILE_OVERLAY_SRC },
	{ "overlay1dst=", MAPFILE_OVERLAY_DST },
	{ "overlay2type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay2order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay2src=", MAPFILE_OVERLAY_SRC },
	{ "overlay2dst=", MAPFILE_OVERLAY_DST },
	{ "overlay3type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay3order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay3src=", MAPFILE_OVERLAY_SRC },
	{ "overlay3dst=", MAPFILE_OVERLAY_DST },
	{ "overlay4type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay4order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay4src=", MAPFILE_OVERLAY_SRC },
	{ "overlay4dst=", MAPFILE_OVERLAY_DST },
	{ "overlay5type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay5order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay5src=", MAPFILE_OVERLAY_SRC },
	{ "overlay5dst=", MAPFILE_OVERLAY_DST },
	{ "overlay6type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay6order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay6src=", MAPFILE_OVERLAY_SRC },
	{ "overlay6dst=", MAPFILE_OVERLAY_DST },
	{ "overlay7type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay7order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay7src=", MAPFILE_OVERLAY_SRC },
	{ "overlay7dst=", MAPFILE_OVERLAY_DST },
	{ "overlay8type=", MAPFILE_OVERLAY_TYPE },
	{ "overlay8order=", MAPFILE_OVERLAY_ORDER },
	{ "overlay8src=", MAPFILE_OVERLAY_SRC },
	{ "overlay8dst=", MAPFILE_OVERLAY_DST },
};

_Static_assert(sizeof(mapFileAttributes) == 320, "map-file attribute table");
_Static_assert(sizeof(mapFileOverlayTypes) == 40, "map-file overlay types");

static char mapFileParseError[64] = { 0 };

int appmgrMapFileSetParseError(int line, const char *message)
{
	return snprintf(mapFileParseError, sizeof(mapFileParseError),
			"%d: error: %s", line + 1, message);
}

int appmgrMapFileReadSourcePath(const char *attribute, char *text, char *output,
			      const char *launchPath, int line, SceSize attributeLength)
{
	const char *message = "invalid attribute.";
	int result = strncmp(text, attribute, attributeLength);
	char *value;
	char *scan;
	char *next;
	char *normalized;
	const char *suffix;
	SceSize length;
	unsigned char character;
	int previousDot;

	if (result != 0)
		goto error;
	value = text + attributeLength;
	character = *value;
	if (character == 0) {
		*output = '\0';
		return result;
	}
	scan = value;
	next = value + 1;
	previousDot = 0;
	do {
		if ((signed char)character == '.') {
			if (previousDot == 1)
				goto invalid_path;
			previousDot = 1;
		} else {
			previousDot = 0;
			if ((signed char)character == '\\')
				*scan = '/';
		}
		scan = next;
		character = *next++;
		if (character == 0)
			break;
	} while (next != value + 293);

	if (strncmp(value, "launch:", 7) != 0) {
		if (strnlen(value, 292) >= 292)
			goto too_long;
		normalized = appmgrAllocMountHeapMemory(292);
		if (normalized == NULL)
			return (int)0x80801006;
		memset(normalized, 0, 292);
		if (appmgrResolvePath(value, 0, normalized, 292) < 0) {
			appmgrFreeMountHeapMemory(normalized);
			appmgrMapFileSetParseError(line, "too long overlay src path.");
			return (int)0x80800021;
		}
		if (!appmgrIsDevelopmentPath(normalized)) {
			result = (int)0x80800021;
			snprintf(mapFileParseError, sizeof(mapFileParseError),
				 "%d: error: %s", line + 1,
				 "invalid overlay src path.");
			appmgrFreeMountHeapMemory(normalized);
			return result;
		}
		snprintf(output, 292, "%s", normalized);
		appmgrFreeMountHeapMemory(normalized);
	} else {
		suffix = value + 7;
		length = strnlen(launchPath, 292);
		length += strnlen(suffix, 292);
		if (value[7] == '/') {
			if (length >= 292)
				goto too_long;
			suffix = value + 8;
		} else if (length + 1 >= 292) {
			goto too_long;
		}
		snprintf(output, 292, "%s/%s", launchPath, suffix);
		normalized = appmgrAllocMountHeapMemory(292);
		if (normalized == NULL)
			return (int)0x80801006;
		memset(normalized, 0, 292);
		if (appmgrResolvePath(output, 0, normalized, 292) < 0) {
			result = (int)0x80800021;
			appmgrFreeMountHeapMemory(normalized);
			snprintf(mapFileParseError, sizeof(mapFileParseError),
				 "%d: error: %s", line + 1,
				 "too long overlay src path.");
			return result;
		}
		memset(output, 0, 292);
		strncpy(output, normalized, 291);
		appmgrFreeMountHeapMemory(normalized);
	}
	if (strncmp(attribute, "savedata0=", 10) == 0 &&
	    (strncmp(output, "ux0:", 4) == 0 ||
	     strncmp(output, "sd0:/userdata", 14) == 0 ||
	     strncmp(output, "sd0:/userdata/", 14) == 0))
		goto invalid_path;
	if (strncmp(attribute, "app0=", 5) != 0)
		return result;
	if (strncmp(output, "ux0:/data", 10) != 0 &&
	    strncmp(output, "ux0:/data/savedata", 19) != 0 &&
	    strncmp(output, "ux0:/data/savedata/", 19) != 0 &&
	    strncmp(output, "ux0:/data/userdata", 19) != 0 &&
	    strncmp(output, "ux0:/data/userdata/", 19) != 0 &&
	    strncmp(output, "sd0:/userdata", 14) != 0 &&
	    strncmp(output, "sd0:/userdata/", 14) != 0)
		return 0;
invalid_path:
	message = "invalid overlay src path.";
	goto error;
too_long:
	message = "too long overlay src path.";
error:
	result = (int)0x80800021;
	snprintf(mapFileParseError, sizeof(mapFileParseError),
		 "%d: error: %s", line + 1, message);
	return result;
}

int sceAppMgrMapFilePrintParseError(const signed char *prefix)
{
	SCE_PRINTF_WITH_INFO(0, 313, "\n");
	if (prefix == NULL || *prefix == '\0')
		SCE_PRINTF_WITH_INFO(0, 315, "%s\n", mapFileParseError);
	else
		SCE_PRINTF_WITH_INFO(0, 318, "[%s] %s\n", prefix,
				     mapFileParseError);
	return SCE_PRINTF_WITH_INFO(0, 320, "\n");
}

int sceAppMgrMapFilePrintMappingError(int error)
{
	SCE_PRINTF_WITH_INFO(0, 325, "\n");
	SCE_PRINTF_WITH_INFO(0, 326, "sceFiosOverlayAdd() failed : 0x%08x",
			     error);
	return SCE_PRINTF_WITH_INFO(0, 327, "\n");
}

int appmgrMapFileOpen(const char *path, AppmgrMapFile *mapFile)
{
	int file;

	memset(mapFile, 0, sizeof(*mapFile));
	mapFileParseError[0] = '\0';
	file = sceIoOpen(path, 1, 6);
	mapFile->file = file;
	if (file < 0) {
		snprintf(mapFileParseError, sizeof(mapFileParseError),
			 "%d: error: %s : 0x%08x", mapFile->line + 1,
			 "file open failed.", file);
		return (int)0x80800020;
	}
	strncpy(mapFile->mapFilePath, path, sizeof(mapFile->mapFilePath));
	mapFile->mapFilePath[1023] = '\0';
	mapFile->line = 0;
	return 0;
}

int appmgrMapFileClose(AppmgrMapFile *mapFile)
{
	SceUID file = mapFile->file;

	if (file <= 0)
		return 0;
	return sceIoClose(file);
}

int appmgrMapFileRead(const char *launchPath, AppmgrMapFile *mapFile)
{
	signed char lineText[313];
	char *text = (char *)lineText;
	unsigned char character;
	unsigned int overlayIndex = 0;
	const char *message;
	int result;
	int line;
	int count;

	for (;;) {
		SceUID file = mapFile->file;
		char *end = text;
		const char *attribute;
		enum AppmgrMapFileAttributeKind kind;
		SceSize attributeLength;
		char *value;
		unsigned int order;
		unsigned int typeIndex;
		unsigned int digit;
		char *scan;
		char *next;
		int previousDot;

		count = 0;
		character = 0;
		do {
			result = sceIoRead(file, &character, 1);
			if (result != 1)
				break;
			if (character == '\n')
				break;
			++count;
			*end++ = character;
		} while (count != 312);
		*end = '\0';
		if (end == text || end[-1] != '\r') {
			if (count == 312) {
				line = mapFile->line;
				goto too_long_line;
			}
		} else {
			end[-1] = '\0';
			--count;
		}
		if (result == 0)
			break;
		line = mapFile->line;
		if (line > 39) {
			message = "unexpected line.";
			goto error;
		}
		attribute = mapFileAttributes[line].name;
		kind = mapFileAttributes[line].kind;
		attributeLength = strnlen(attribute, 21);
		if (attributeLength > strnlen(text, 20))
			goto invalid_attribute;
		switch (kind) {
		case MAPFILE_BLANK:
			if (strncmp(text, attribute, attributeLength) != 0)
				goto invalid_attribute;
			if (text[attributeLength] != '\0') {
				message = "value must be blank.";
				goto error;
			}
			break;
		case MAPFILE_VERSION:
			value = text;
			if (lineText[0] == -17 && lineText[1] == -69 &&
			    lineText[2] == -65)
				value = text + 3;
			if (strncmp(value, attribute, attributeLength) != 0)
				goto invalid_attribute;
			value += attributeLength;
			if (value[0] != '1' || value[1] != '\0') {
				message = "invalid version.";
				goto error;
			}
			mapFile->version = 1;
			break;
		case MAPFILE_APP_PATH:
			result = appmgrMapFileReadSourcePath(attribute, text,
				mapFile->app0, launchPath, line, attributeLength);
			goto source_path_result;
		case MAPFILE_SAVEDATA_PATH:
			result = appmgrMapFileReadSourcePath(attribute, text,
				mapFile->savedata0, launchPath, line, attributeLength);
			goto source_path_result;
		case MAPFILE_ADDCONT_PATH:
			result = appmgrMapFileReadSourcePath(attribute, text,
				mapFile->addcont0, launchPath, line, attributeLength);
			goto source_path_result;
		case MAPFILE_OVERLAY_TYPE:
			if (strncmp(text, attribute, attributeLength) != 0)
				goto invalid_attribute;
			value = text + attributeLength;
			if (*value != '\0') {
				typeIndex = 0;
				attribute = mapFileOverlayTypes[0].name;
				while (strncmp(value, attribute, 32) != 0) {
					attribute = mapFileOverlayTypes[++typeIndex].name;
					if (attribute == NULL) {
						message = "invalid overlay type.";
						goto error;
					}
				}
				mapFile->overlays[overlayIndex].type =
					mapFileOverlayTypes[typeIndex].type;
			} else {
				mapFile->overlays[overlayIndex].type =
					SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT;
			}
			line = mapFile->line;
			break;
		case MAPFILE_OVERLAY_ORDER:
			if (strncmp(text, attribute, attributeLength) != 0)
				goto invalid_attribute;
			value = text + attributeLength;
			order = (unsigned char)value[0];
			if (order != 0) {
				if (order - '0' > 9)
					goto invalid_order;
				order = (signed char)order - '0';
				digit = (unsigned char)value[1];
				if (digit != 0) {
					if (digit - '0' > 9)
						goto invalid_order;
					order = (signed char)digit - '0' + 10 * order;
					digit = (unsigned char)value[2];
					if (digit != 0) {
						if (digit - '0' > 9)
							goto invalid_order;
						order = (signed char)digit - '0' + 10 * order;
						if (value[3] != '\0')
							goto invalid_order;
					}
				}
				if (order > 127)
					goto invalid_order;
			}
			mapFile->overlays[overlayIndex].order = order;
			line = mapFile->line;
			break;
		case MAPFILE_OVERLAY_SRC:
			result = appmgrMapFileReadSourcePath(attribute, text,
				mapFile->overlays[overlayIndex].src, launchPath,
				line, attributeLength);
			goto source_path_result;
		case MAPFILE_OVERLAY_DST:
			result = strncmp(text, attribute, attributeLength);
			if (result != 0)
				goto invalid_attribute;
			value = text + attributeLength;
			character = *value;
			if (character == 0) {
				mapFile->overlays[overlayIndex].dst[0] = '\0';
				++overlayIndex;
				goto source_path_result;
			}
			scan = value;
			next = value + 1;
			previousDot = result;
			do {
				if ((signed char)character != '.') {
					previousDot = 0;
					if ((signed char)character == '\\')
						*scan = '/';
				} else {
					if (previousDot == 1)
						goto invalid_destination;
					previousDot = 1;
				}
				scan = next;
				character = *next++;
				if (character == 0)
					break;
			} while (next != value + 293);
			if (strncmp(value, "app0:", 5) != 0)
				goto invalid_destination;
			if (value[5] == '\0' ||
			    ((value[5] == '/' || value[5] == '\\') &&
			     value[6] == '\0')) {
				message = "invalid overlay dst path format.";
				goto destination_error;
			}
			if (strnlen(value, 292) >= 292) {
				message = "too long overlay dst path.";
				goto destination_error;
			}
			snprintf(mapFile->overlays[overlayIndex].dst, 292, "%s", value);
			++overlayIndex;
			result = 0;
			goto source_path_result;
		default:
			goto invalid_attribute;
		}
advance_line:
		mapFile->line = line + 1;
		continue;
source_path_result:
		if (result != 0)
			return result;
		line = mapFile->line;
		goto advance_line;
invalid_destination:
		appmgrMapFileSetParseError(line, "invalid overlay dst path format.");
		result = (int)0x80800021;
		++overlayIndex;
		goto source_path_result;
destination_error:
		snprintf(mapFileParseError, sizeof(mapFileParseError),
			 "%d: error: %s", line + 1, message);
		result = (int)0x80800021;
		++overlayIndex;
		goto source_path_result;
invalid_order:
		appmgrMapFileSetParseError(line, "invalid overlay order.");
		return (int)0x80800021;
invalid_attribute:
		message = "invalid attribute.";
		goto error;
	}
	line = mapFile->line;
	if (line <= 39) {
		if (count != 312) {
			message = "unexpected EOF.";
			goto error;
		}
	} else if (count != 312) {
		return result;
	}
too_long_line:
	message = "too long line.";
error:
	snprintf(mapFileParseError, sizeof(mapFileParseError),
		 "%d: error: %s", line + 1, message);
	return (int)0x80800021;
}

int appmgrMapFileValidatePath(const char *path, char *out)
{
	const char *message;

	strncpy(out, path, 1024);
	out[1023] = '\0';
	if (strncmp(path, "host0:", 6) != 0 &&
	    strncmp(path, "sd0:", 4) != 0) {
		message = "invalid file path.";
	} else {
		if (strnlen(path, 1024) < 1024)
			return 0;
		message = "too long mapping file path.";
	}
	snprintf(mapFileParseError, sizeof(mapFileParseError),
		 "%d: error: %s", 1, message);
	return (int)0x80800020;
}

int sceAppMgrMapFileIsValidOverlay(unsigned int index, AppmgrMapFile *mapFile)
{
	AppmgrMapFileOverlay *overlay;
	const char *typeName;

	if (index > 7)
		goto skipped;
	overlay = &mapFile->overlays[index];
	if ((signed char)overlay->src[0] == 0 ||
	    (signed char)overlay->dst[0] == 0)
		goto skipped;
	if (overlay->type != SCE_FIOS_OVERLAY_TYPE_OPAQUE) {
		if (overlay->type == SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT) {
			typeName = "SCE_FIOS_OVERLAY_TYPE_TRANSLUCENT";
		} else if (overlay->type == SCE_FIOS_OVERLAY_TYPE_NEWER) {
			typeName = "SCE_FIOS_OVERLAY_TYPE_NEWER";
		} else if (overlay->type == SCE_FIOS_OVERLAY_TYPE_WRITABLE) {
			typeName = "SCE_FIOS_OVERLAY_TYPE_WRITABLE";
		} else {
			typeName = "";
		}
	} else {
		typeName = "SCE_FIOS_OVERLAY_TYPE_OPAQUE";
	}
	SCE_PRINTF_WITH_INFO(0, 487,
		"mapping overlay%d type:%s order%d  src:\"%s\" -> dst:\"%s\"\n",
		index + 1, typeName, overlay->order, overlay->src, overlay->dst);
	return 1;
skipped:
	SCE_PRINTF_WITH_INFO(0, 494, "skipped overlay%d\n", index + 1);
	return 0;
}

int sceAppMgrDebugDumpMapFileInfo(AppmgrMapFile *mapFile)
{
	unsigned int index;
	AppmgrMapFileOverlay *overlay;

	SCE_PRINTF_WITH_INFO(0, 907,
		"========= Mapping File configuration =========\n");
	SCE_PRINTF_WITH_INFO(0, 930,
		"version:%d\napp0:\"%s\"\nsavedata0:\"%s\"\naddcont0:\"%s\"\n"
		"otherApps:\"%s\"\notherAppsPatches:\"%s\"\n"
		"otherAppsSavedata:\"%s\"\notherAppsContents:\"%s\"\n",
		mapFile->version, mapFile->app0, mapFile->savedata0,
		mapFile->addcont0, mapFile->otherApps, mapFile->otherAppsPatches,
		mapFile->otherAppsSavedata, mapFile->otherAppsContents);
	index = 0;
	overlay = mapFile->overlays;
	do {
		++index;
		SCE_PRINTF_WITH_INFO(0, 941,
			"overlay%dtype:%d\noverlay%dorder:%d\n"
			"overlay%dsrc:\"%s\"\noverlay%ddst:\"%s\"\n",
			index, overlay->type, index, overlay->order,
			index, overlay->src, index, overlay->dst);
		++overlay;
	} while (index != 8);
	return SCE_PRINTF_WITH_INFO(0, 945,
		"==============================================\n");
}

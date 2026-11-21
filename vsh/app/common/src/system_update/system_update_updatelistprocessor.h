/*
 * system_update_updatelistprocessor.h: bXCe's handler that reads the update list
 * (system_update_updatelistprocessor.cpp, Sony's file by its messages) into a
 * SystemUpdateList. m_list_data, m_list_updata and m_pRcvInfo_array are Sony's names (its
 * messages); every other name is ours (PLACEHOLDER).
 */
#ifndef _SYSTEM_UPDATE_UPDATELISTPROCESSOR_H
#define _SYSTEM_UPDATE_UPDATELISTPROCESSOR_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <bXCe/bxce_internal.h>

/* an image of an update list entry (PLACEHOLDER names) */
struct SystemUpdateImage {
	SystemUpdateImage() : size(-1)
	{
	}

	int type;			/* the system version it updates from, -1 any */
	int kind;			/* 0 full, 1 patch, 2 multiple patch */
	paf::string url;
	SceInt64 size;
};

PAF_VECTOR_INLINE_DTOR(SystemUpdateImage)
PAF_VECTOR_NOINLINE_PUSH_BACK(SystemUpdateImage, paf::allocator<SystemUpdateImage>)

/* an update list entry: a system version and its images (PLACEHOLDER names) */
struct SystemUpdateData {
	int type;			/* the system version */
	paf::string version;		/* its label */
	paf::vector<SystemUpdateImage> images;
};

PAF_VECTOR_NOINLINE_CLEAR(SystemUpdateData, paf::allocator<SystemUpdateData>)
PAF_VECTOR_NOINLINE_PUSH_BACK(SystemUpdateData, paf::allocator<SystemUpdateData>)

/* the update list: the region it was read for and its entries (PLACEHOLDER names) */
struct SystemUpdateList {
	paf::string url;		/* the region the list's entries are for */
	paf::vector<SystemUpdateData> list;
};

/* a recovery package the list is searched for (PLACEHOLDER names) */
struct SystemUpdateRecoveryInfo {
	paf::string type;		/* its spkg_type */
	int version;			/* its spkg_version */
	paf::string url;		/* where the list says it is */
	SceInt64 size;
};

#define UPDATE_LIST_RECOVERY_MAX	5	/* PLACEHOLDER name */

/* the list's elements, in the order it nests them (PLACEHOLDER names) */
enum UpdateListState {
	UPDATE_LIST_STATE_NONE,
	UPDATE_LIST_STATE_ROOT,		/* in <update_data_list> */
	UPDATE_LIST_STATE_REGION,	/* in the console's <region> */
	UPDATE_LIST_STATE_VERSION,	/* in its <version> */
	UPDATE_LIST_STATE_DATA,		/* in its <update_data> */
	UPDATE_LIST_STATE_RECOVERY	/* in a <recovery> searched for */
};

/* bXCe's handler that reads the update list into a SystemUpdateList (vtable 0x81530580) */
class UpdateListProcessor : public bXCe::Sax::DocumentHandler {
public:
	UpdateListProcessor(SystemUpdateList *list);

	virtual ~UpdateListProcessor()
	{
	}

	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int entityData(void *userData, int entityType, const char *name, int length);

	int Bgdl_81133210(void);					/* Initialize */
	int Bgdl_8113329E(void);					/* the list's result */

	int reserved[2];		/* PLACEHOLDER: not read */
	paf::string m_region;		/* the console's CDN region */
	SystemUpdateList *m_list;
	SystemUpdateData *m_list_data;
	SystemUpdateImage *m_list_updata;
	bool m_inRoot;
	bool m_inRegion;
	bool m_inVersion;
	bool m_inData;
	bool m_inImage;
	bool m_inText;			/* PLACEHOLDER: nothing sets it */
	bool m_regionDone;
	int m_result;
	paf::string m_url;		/* the image's text */
	int reserved2[3];		/* PLACEHOLDER: not read */
	paf::string m_text;		/* PLACEHOLDER: what m_inText collects */
	int m_state;			/* UpdateListState */
	paf::string m_text2;		/* PLACEHOLDER: not read */
	int reserved3;			/* PLACEHOLDER: not read */
	paf::string m_recoveryUrl;
	SceInt64 m_recoverySize;
	SystemUpdateRecoveryInfo *m_pRcvInfo_array[UPDATE_LIST_RECOVERY_MAX];
	int m_rcvIndex;			/* the recovery package being read */
	bool m_inRecoveryImage;
	paf::string m_rcvType;
	int m_rcvValue;			/* PLACEHOLDER: only cleared */
};

/* the processor for the console's CDN region (vtable 0x8153048C) */
class UpdateListReader : public UpdateListProcessor {
public:
	UpdateListReader(SystemUpdateList *list, SceSize max);

	int Bgdl_81134D70(void);					/* Initialize and the region */

private:
	void setRegion(paf::string region);
};

#endif

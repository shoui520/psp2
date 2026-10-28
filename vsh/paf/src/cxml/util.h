#ifndef PAF_CXML_UTIL_H
#define PAF_CXML_UTIL_H

#include "cxml.h"

#define CxmlUtilCheckElementName ScePafResource_01B5996A
#define CxmlUtilFindChildElement ScePafResource_B8F7FD66
#define CxmlUtilFindChildElementByID ScePafResource_4381989C
#define CxmlUtilFindChildElementByIDHash ScePafResource_2E541C63
#define CxmlUtilFindEmptyAttribute ScePafResource_55AB01CC
#define CxmlUtilGetInt scePafResourceGetAttributeInteger
#define CxmlUtilGetFloat scePafResourceGetAttributeFloat
#define CxmlUtilGetString scePafResourceGetAttributeString
#define CxmlUtilGetWString scePafResourceGetAttributeChar
#define CxmlUtilGetHash scePafResourceGetAttributeStyleId
#define CxmlUtilGetIntArray scePafResourceGetAttributeIntegerArray
#define CxmlUtilGetFloatArray scePafResourceGetAttributeFloatArray
#define CxmlUtilGetFile scePafResourceGetAttributeData
#define CxmlUtilGetID scePafResourceGetAttributeIdStrLpb
#define CxmlUtilGetIDRef scePafResourceGetAttributeIdStr
#define CxmlUtilGetIDHash scePafResourceGetAttributeIdIntLpb
#define CxmlUtilGetIDHashRef scePafResourceGetAttributeIdInt

unsigned char CxmlUtilCheckElementName(const CxmlElement *elm, const char *valid_name);
void CxmlUtilFindChildElement(CxmlElement *result, const CxmlElement *parent,
			      const char *name);
void CxmlUtilFindChildElementByID(CxmlElement *result, const CxmlElement *parent,
				  const char *name, const char *idname, const char *idvalue);
void CxmlUtilFindChildElementByIDHash(CxmlElement *result, const CxmlElement *parent,
				      const char *name, const char *idhashname, uint32_t idhashvalue);
int32_t CxmlUtilFindEmptyAttribute(const CxmlElement *elm, CxmlAttribute *attr);
int32_t CxmlUtilGetInt(const CxmlElement *elm, const char *attrname, int32_t *value);
int32_t CxmlUtilGetFloat(const CxmlElement *elm, const char *attrname, float *value);
int32_t CxmlUtilGetString(const CxmlElement *elm, const char *attrname,
			 const char **value, uint32_t *len);
int32_t CxmlUtilGetWString(const CxmlElement *elm, const char *attrname,
			  const uint16_t **value, uint32_t *len);
int32_t CxmlUtilGetHash(const CxmlElement *elm, const char *attrname, uint32_t *value);
int32_t CxmlUtilGetIntArray(const CxmlElement *elm, const char *attrname,
			   const int32_t **addr, uint32_t *num);
int32_t CxmlUtilGetFloatArray(const CxmlElement *elm, const char *attrname,
			     const float **addr, uint32_t *num);
int32_t CxmlUtilGetFile(const CxmlElement *elm, const char *attrname, CxmlFile *file);
int32_t CxmlUtilGetID(const CxmlElement *elm, const char *attrname, const char **value);
int32_t CxmlUtilGetIDRef(const CxmlElement *elm, const char *attrname,
			const char **value, CxmlElement *entity);
int32_t CxmlUtilGetIDHash(const CxmlElement *elm, const char *attrname, uint32_t *value);
int32_t CxmlUtilGetIDHashRef(const CxmlElement *elm, const char *attrname,
			    uint32_t *value, CxmlElement *entity);

#endif

/*
 * Freeing what the XML signature readers return (0x8105D018-). The file's name is ours.
 */
#include "mln_internal.h"

/* free what a Reference holds */
int MlnImpl_8105D018(MlnXmlDsigReference *reference)
{
	int i;

	if (reference == NULL)
		return 0;
	if (reference->uri != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, reference->uri);
	if (reference->transforms != NULL) {
		for (i = 0; i < reference->transformCount; i++)
			MlnBufferDestroy(MLN_HEAP_GENERAL, reference->transforms[i]);
		MlnFree(MLN_HEAP_GENERAL, reference->transforms);
	}
	if (reference->digestMethod != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, reference->digestMethod);
	if (reference->digestValue != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, reference->digestValue);
	return 0;
}

/* free a signature MlnImpl_81059934 or MlnImpl_8105A3D8 read */
int MlnImpl_8105D0AC(MlnXmlDsigSignature *signature)
{
	int i;

	if (signature == NULL)
		return 0;
	if (signature->id != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, signature->id);
	if (signature->canonicalizationMethod != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, signature->canonicalizationMethod);
	if (signature->signatureMethod != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, signature->signatureMethod);
	if (signature->references != NULL) {
		for (i = 0; i < signature->referenceCount; i++)
			MlnImpl_8105D018(&signature->references[i]);
		MlnFree(MLN_HEAP_GENERAL, signature->references);
	}
	if (signature->signedInfo != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, signature->signedInfo);
	if (signature->signatureValue != NULL)
		MlnBufferDestroy(MLN_HEAP_GENERAL, signature->signatureValue);
	switch (signature->keyInfoType) {
	case MLN_XMLDSIG_KEYINFO_KEYNAME:
	case MLN_XMLDSIG_KEYINFO_X509SKI:
		if (signature->keyInfo.keyName != NULL)
			MlnBufferDestroy(MLN_HEAP_GENERAL, signature->keyInfo.keyName);
		break;
	case MLN_XMLDSIG_KEYINFO_X509:
		if (signature->keyInfo.x509.certificates != NULL) {
			for (i = 0; i < signature->keyInfo.x509.count; i++)
				MlnBufferDestroy(MLN_HEAP_GENERAL,
						 signature->keyInfo.x509.certificates[i]);
			MlnFree(MLN_HEAP_GENERAL, signature->keyInfo.x509.certificates);
		}
		break;
	}
	MlnFree(MLN_HEAP_GENERAL, signature);
	return 0;
}

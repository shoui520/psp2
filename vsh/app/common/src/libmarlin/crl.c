/*
 * Certificate revocation lists: a CRL's scope and validity, its TBSCertList and its
 * extensions parsed, and the list kept for checking certificates (0x81057728-). The file's
 * name is ours.
 */
#include "mln_internal.h"

#define MLN_CRL_ERROR_ENCODING	0x22010001
#define MLN_CRL_ERROR_FORMAT	0x22010003

/*
 * Whether an indirect CRL's issuing distribution point covers every reason for all
 * certificates: no "only" restriction, and onlySomeReasons absent or with its unused bit.
 */
int MlnImpl_81057728(const MlnX509CrlCertificate *crl)
{
	const MlnX509CrlIssuingDistributionPoint *point =
		&crl->tbs->extensions.issuingDistributionPoint;
	unsigned int mask;
	int byte;
	int i;

	if (!point->indirectCRL)
		return 0;
	if (point->onlyContainsUserCerts)
		return 0;
	if (point->onlyContainsCACerts)
		return 0;
	if (point->onlyContainsAttributeCerts)
		return 0;
	mask = 0x80;
	byte = 0;
	for (i = 0; i < point->onlySomeReasonsBits; i++) {
		if ((i & 7) == 0) {
			if (i != 0)
				byte++;
			mask = 0x80;
		}
		if (point->onlySomeReasons[byte] & mask) {
			if (i == 0)
				return 1;
			return 0;
		}
		mask >>= 1;
	}
	return 1;
}

/* whether a CRL's thisUpdate is not after its nextUpdate */
int MlnImpl_810577D4(const MlnX509CrlCertificate *crl)
{
	return MlnImpl_8103B0C4(&crl->tbs->thisUpdate, &crl->tbs->nextUpdate) == 0;
}

/* an issuing distribution point's boolean, its context tag read as BOOLEAN */
static inline int MlnCrlParseFlag(MlnAsn1Node *node, int *flag)
{
	int value;
	int ret;

	node->tag = MLN_ASN1_TAG_BOOLEAN;
	ret = MlnImpl_81037EC8(MLN_HEAP_GENERAL, node, &value);
	if (ret != 0)
		return ret;
	*flag = value != 0;
	return 0;
}

/*
 * A TBSCertList's crlExtensions ([0] EXPLICIT Extensions) into its fields: the CRL number,
 * the authority key identifier and the issuing distribution point, all three required;
 * another extension may not be critical. present is cleared when there are none.
 */
int MlnImpl_810577F4(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	MlnX509CrlExtensions *crlExtensions = &tbs->extensions;
	MlnX509CrlIssuingDistributionPoint *idp;
	MlnX509Extensions *extensions = NULL;
	MlnX509Extension *extension;
	MlnAsn1Node *number;
	MlnAsn1Node *point;
	MlnAsn1Node *child;
	int haveNumber;
	int haveKeyIdentifier;
	int havePoint;
	unsigned int j;
	int i;
	int ret;

	*present = 1;
	if (node == NULL || node->tagClass != 2 || !node->constructed || node->tag != 0 ||
	    MlnX509Child(node, 0) == NULL) {
		*present = 0;
		ret = MLN_CRL_ERROR_FORMAT;
		goto out;
	}
	ret = MlnImpl_8103AC98(node->children[0], &extensions);
	if (ret != 0)
		goto out;
	number = NULL;
	point = NULL;
	haveNumber = 0;
	haveKeyIdentifier = 0;
	havePoint = 0;
	for (i = 0; i < extensions->count; i++) {
		extension = extensions->items[i];
		switch (extension->id) {
		case MLN_X509_EXTENSION_CRL_NUMBER:
			ret = MlnImpl_810378E4(MLN_HEAP_GENERAL, extension->value.data,
					       extension->value.size, NULL, &number);
			if (ret != 0)
				goto free;
			ret = MlnImpl_81037FE0(MLN_HEAP_GENERAL, number,
					       sizeof(crlExtensions->crlNumber.number), crlExtensions->crlNumber.number);
			if (ret != 0)
				goto free;
			haveNumber = 1;
			crlExtensions->crlNumber.present = 1;
			break;
		case MLN_X509_EXTENSION_ISSUING_DISTRIBUTION_POINT:
			ret = MlnImpl_810378E4(MLN_HEAP_GENERAL, extension->value.data,
					       extension->value.size, NULL, &point);
			if (ret != 0)
				goto free;
			idp = &crlExtensions->issuingDistributionPoint;
			idp->onlyContainsUserCerts = 0;
			idp->onlyContainsCACerts = 0;
			idp->indirectCRL = 0;
			idp->onlyContainsAttributeCerts = 0;
			idp->onlySomeReasonsBits = 0;
			/* Sony clears eight bytes from onlySomeReasons, the bit count with them */
			MlnMemset(idp->onlySomeReasons, 0, 8);
			if (point == NULL || point->tagClass != 0 || !point->constructed ||
			    point->tag != MLN_ASN1_TAG_SEQUENCE)
				goto invalid;
			for (j = 0; (child = MlnX509Child(point, j)) != NULL; j++) {
				/* distributionPoint is not read */
				if (child->tagClass == 2 && child->constructed && child->tag == 0)
					continue;
				switch (MLN_ASN1_IDENTIFIER(child)) {
				case MLN_ASN1_ID(2, 0, 1):
					ret = MlnCrlParseFlag(child, &idp->onlyContainsUserCerts);
					break;
				case MLN_ASN1_ID(2, 0, 2):
					ret = MlnCrlParseFlag(child, &idp->onlyContainsCACerts);
					break;
				case MLN_ASN1_ID(2, 0, 3):
					child->tag = MLN_ASN1_TAG_BIT_STRING;
					ret = MlnImpl_8103814C(MLN_HEAP_GENERAL, child,
							       sizeof(idp->onlySomeReasons),
							       idp->onlySomeReasons,
							       &idp->onlySomeReasonsBits);
					break;
				case MLN_ASN1_ID(2, 0, 4):
					ret = MlnCrlParseFlag(child, &idp->indirectCRL);
					break;
				case MLN_ASN1_ID(2, 0, 5):
					ret = MlnCrlParseFlag(child, &idp->onlyContainsAttributeCerts);
					break;
				default:
					ret = MLN_CRL_ERROR_FORMAT;
					break;
				}
				if (ret != 0)
					goto free;
			}
			havePoint = 1;
			break;
		case MLN_X509_EXTENSION_AUTHORITY_KEY_IDENTIFIER:
			ret = MlnImpl_8103AFFC(&extension->value, &crlExtensions->authorityKeyIdentifier,
					       MLN_HEAP_GENERAL);
			if (ret != 0)
				goto free;
			haveKeyIdentifier = 1;
			break;
		default:
			if (extension->critical)
				goto invalid;
			break;
		}
	}
	if (!haveNumber || !haveKeyIdentifier || !havePoint)
		goto invalid;
	ret = 0;
	goto free;
invalid:
	ret = MLN_CRL_ERROR_FORMAT;
free:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, number);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, point);
out:
	MlnImpl_8103AC34(extensions);
	return ret;
}

/* whether a CRL is signed with the algorithm its TBSCertList names, SHA-1 or SHA-256 RSA */
int MlnImpl_81057C3C(const MlnX509CrlCertificate *crl)
{
	const MlnX509CrlTbs *tbs = crl->tbs;
	int algorithm = crl->signatureAlgorithm.algorithm;

	int tbsAlgorithm = tbs->signature.algorithm;

	if (algorithm != MLN_X509_ALGORITHM_SHA1_RSA && algorithm != MLN_X509_ALGORITHM_SHA256_RSA)
		return 0;
	if (algorithm != tbsAlgorithm)
		return 0;
	return MlnBufferCompare(MLN_HEAP_GENERAL, &crl->signatureAlgorithm.oid,
				&tbs->signature.oid) == 0;
}

/* a TBSCertList's signature algorithm */
int MlnImpl_81057C88(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	*present = 1;
	if (node == NULL) {
		*present = 0;
		return MLN_CRL_ERROR_FORMAT;
	}
	return MlnImpl_8103AA04(node, &tbs->signature);
}

static inline int MlnCrlIsTime(const MlnAsn1Node *node)
{
	return MLN_ASN1_IDENTIFIER(node) == MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_GENERALIZED_TIME) ||
	       MLN_ASN1_IDENTIFIER(node) == MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_UTC_TIME);
}

/* a TBSCertList's nextUpdate */
int MlnImpl_81057CB8(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	*present = 1;
	if (node != NULL && MlnCrlIsTime(node))
		return MlnImpl_8103AB18(node, &tbs->nextUpdate);
	*present = 0;
	return MLN_CRL_ERROR_FORMAT;
}

/* a TBSCertList's thisUpdate */
int MlnImpl_81057D00(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	*present = 1;
	if (node != NULL && MlnCrlIsTime(node))
		return MlnImpl_8103AB18(node, &tbs->thisUpdate);
	*present = 0;
	return MLN_CRL_ERROR_FORMAT;
}

/*
 * A TBSCertList's issuer, which must be one Marlin name: a single RDN of one attribute,
 * 1.3.6.1.4.1.23727.1.1.1, with a UTF8String value.
 */
int MlnImpl_81057D48(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	const unsigned char type[] = {
		0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xB9, 0x2F, 0x01, 0x01, 0x01,
	};
	MlnBuffer expected;
	MlnBuffer value;
	MlnAsn1Node *rdn;
	MlnAsn1Node *attribute;
	MlnAsn1Node *oid;
	MlnAsn1Node *name;
	int ret;

	*present = 1;
	if (node == NULL) {
		*present = 0;
		return MLN_CRL_ERROR_FORMAT;
	}
	ret = MlnImpl_8103AC08(node, &tbs->issuer);
	if (ret != 0)
		return ret;
	if (node->tagClass != 0 || !node->constructed || node->tag != MLN_ASN1_TAG_SEQUENCE)
		goto invalid;
	rdn = MlnX509Child(node, 0);
	if (rdn == NULL || rdn->tagClass != 0 || !rdn->constructed || rdn->tag != MLN_ASN1_TAG_SET)
		goto invalid;
	attribute = MlnX509Child(rdn, 0);
	if (attribute == NULL || attribute->tagClass != 0 || !attribute->constructed ||
	    attribute->tag != MLN_ASN1_TAG_SEQUENCE)
		goto invalid;
	oid = MlnX509Child(attribute, 0);
	if (oid == NULL || MLN_ASN1_IDENTIFIER(oid) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_OID))
		goto invalid;
	expected.size = sizeof(type);
	expected.data = (unsigned char *)type;
	value.size = oid->length;
	value.data = oid->data;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, &expected, &value) != 0)
		goto invalid;
	name = MlnX509Child(attribute, 1);
	if (name == NULL ||
	    MLN_ASN1_IDENTIFIER(name) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_UTF8_STRING))
		goto invalid;
	return 0;
invalid:
	return MLN_CRL_ERROR_FORMAT;
}

/* a TBSCertList's version, which must be v2 */
int MlnImpl_81057F04(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	int ret;

	*present = 1;
	if (node == NULL || MLN_ASN1_IDENTIFIER(node) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_INTEGER)) {
		*present = 0;
		return MLN_CRL_ERROR_FORMAT;
	}
	ret = MlnImpl_81037F54(MLN_HEAP_GENERAL, node, &tbs->version);
	if (ret != 0)
		return ret;
	return tbs->version == 1 ? 0 : MLN_CRL_ERROR_FORMAT;
}

/*
 * A revokedCertificates entry: its serial number, positive and not zero, its revocation
 * date, and its certificateIssuer extension's directoryName, the issuer of the entry before
 * it otherwise; another critical entry extension is refused.
 */
static inline int MlnCrlParseEntry(const MlnAsn1Node *node, const MlnBuffer *issuer,
				   MlnX509CrlEntry **out)
{
	MlnX509Extensions *extensions = NULL;
	MlnX509Extension *extension;
	MlnX509CrlEntry *entry;
	MlnAsn1Node *serial;
	MlnAsn1Node *date;
	MlnAsn1Node *extensionsNode;
	MlnAsn1Node *names;
	MlnAsn1Node *name;
	MlnAsn1Node *directoryName;
	unsigned int j;
	int i;
	int ret;

	entry = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*entry));
	if (entry == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(entry, 0, sizeof(*entry));
	entry->certificateIssuer = *issuer;
	serial = MlnX509Child(node, 0);
	date = MlnX509Child(node, 1);
	extensionsNode = MlnX509Child(node, 2);
	if (serial == NULL || date == NULL)
		goto invalid;
	if (serial->tag != MLN_ASN1_TAG_INTEGER || serial->length == 0 || (serial->data[0] & 0x80))
		goto encoding;
	for (j = 0; j < serial->length && serial->data[j] == 0; j++)
		;
	if (j == serial->length)
		goto encoding;
	MlnBufferSet(MLN_HEAP_GENERAL, &entry->serialNumber, serial->length, serial->data);
	ret = MlnImpl_8103AB18(date, &entry->revocationDate);
	if (ret != 0)
		goto out;
	if (extensionsNode != NULL) {
		ret = MlnImpl_8103AC98(extensionsNode, &extensions);
		if (ret != 0)
			goto out;
		for (i = 0; i < extensions->count; i++) {
			extension = extensions->items[i];
			if (extension->id != MLN_X509_EXTENSION_CERTIFICATE_ISSUER) {
				if (extension->critical)
					goto invalid;
				continue;
			}
			names = NULL;
			ret = MlnImpl_810378E4(MLN_HEAP_GENERAL, extension->value.data,
					       extension->value.size, NULL, &names);
			if (ret != 0)
				goto names;
			if (names == NULL || names->tagClass != 0 || !names->constructed ||
			    names->tag != MLN_ASN1_TAG_SEQUENCE)
				goto badNames;
			/* the first directoryName ([4]) of the GeneralNames */
			for (j = 0; ; j++) {
				if (j == names->length)
					goto badNames;
				name = names->children[j];
				if (name == NULL)
					goto badNames;
				if (name->tagClass == 2 && name->constructed && name->tag == 4) {
					directoryName = MlnX509Child(name, 0);
					if (directoryName != NULL)
						break;
				}
			}
			ret = MlnImpl_8103AC08(directoryName, &entry->certificateIssuer);
			if (ret != 0)
				goto names;
			MlnImpl_81036FC8(MLN_HEAP_GENERAL, names);
		}
	}
	*out = entry;
	entry = NULL;
	ret = 0;
	goto out;
badNames:
	ret = MLN_CRL_ERROR_ENCODING;
names:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, names);
	goto out;
encoding:
	ret = MLN_CRL_ERROR_ENCODING;
	goto out;
invalid:
	ret = MLN_CRL_ERROR_FORMAT;
out:
	MlnImpl_8103AC34(extensions);
	MlnFree(MLN_HEAP_GENERAL, entry);
	return ret;
}

/* a CRL's entries, each with the buffers it points to in the DER */
static inline void MlnCrlFreeEntries(MlnX509CrlEntries *entries)
{
	int i;

	for (i = 0; i < entries->count; i++)
		MlnFree(MLN_HEAP_GENERAL, entries->items[i]);
	MlnFree(MLN_HEAP_GENERAL, entries->items);
	MlnFree(MLN_HEAP_GENERAL, entries);
}

/* a TBSCertList's revokedCertificates, an empty list when it has none */
int MlnImpl_81057F70(const MlnAsn1Node *node, MlnX509CrlTbs *tbs, int *present)
{
	MlnX509CrlEntries *entries;
	const MlnBuffer *issuer;
	unsigned int i;
	int ret;

	if (node == NULL || node->tagClass != 0 || !node->constructed ||
	    node->tag != MLN_ASN1_TAG_SEQUENCE) {
		*present = 0;
		node = NULL;
	} else {
		if (node->length == 0)
			return MLN_CRL_ERROR_FORMAT;
		*present = 1;
	}
	entries = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*entries));
	if (entries == NULL)
		return 2;
	MlnMemset(entries, 0, sizeof(*entries));
	if (node != NULL && node->length != 0) {
		entries->items = MlnAllocate(MLN_HEAP_GENERAL,
					     node->length * sizeof(*entries->items));
		if (entries->items == NULL) {
			ret = 2;
			goto fail;
		}
		entries->count = 0;
		for (i = 0; i < node->length; i++) {
			if (i == 0)
				issuer = &tbs->issuer;
			else
				issuer = &entries->items[i - 1]->certificateIssuer;
			ret = MlnCrlParseEntry(node->children[i], issuer, &entries->items[i]);
			if (ret != 0)
				goto fail;
			entries->count++;
		}
	}
	tbs->revokedCertificates = entries;
	return 0;
fail:
	MlnCrlFreeEntries(entries);
	return ret;
}

/*
 * X.509: certificates and their public keys, read from the DER trees asn1.c builds.
 * The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_X509_ERROR_FORMAT	0x22010001

/* the child at index of a constructed node, NULL when there is none */
static inline MlnAsn1Node *MlnX509Child(const MlnAsn1Node *node, unsigned int index)
{
	if (node == NULL || !node->constructed || index >= node->length)
		return NULL;
	return node->children[index];
}

/* an RSA public key's modulus and exponent, from a SubjectPublicKeyInfo's key bits */
__attribute__((noinline)) int MlnImpl_81038718(const MlnX509PublicKey *publicKey, MlnBuffer **modulus,
		     MlnBuffer **exponent)
{
	MlnAsn1Node *tree = NULL;
	MlnAsn1Node *first, *second;
	MlnBuffer *n = NULL, *e = NULL;
	int result;

	if (publicKey == NULL || modulus == NULL || exponent == NULL) {
		result = 3;
		goto out;
	}
	if (publicKey->algorithm.algorithm != MLN_X509_ALGORITHM_RSA) {
		result = MLN_X509_ERROR_FORMAT;
		goto out;
	}
	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, publicKey->key.data, publicKey->key.size,
				  NULL, &tree);
	if (result != 0)
		goto out;
	first = MlnX509Child(tree, 0);
	second = MlnX509Child(tree, 1);
	if (first == NULL || MLN_ASN1_IDENTIFIER(first) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_INTEGER) ||
	    second == NULL || MLN_ASN1_IDENTIFIER(second) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_INTEGER)) {
		result = MLN_X509_ERROR_FORMAT;
		goto out;
	}
	n = MlnBufferCreate(MLN_HEAP_GENERAL, first->length, first->data);
	if (n == NULL) {
		result = 2;
		goto out;
	}
	e = MlnBufferCreate(MLN_HEAP_GENERAL, second->length, second->data);
	if (e == NULL) {
		result = 2;
		goto out;
	}
	*modulus = n;
	n = NULL;
	*exponent = e;
	e = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, n);
	MlnBufferDestroy(MLN_HEAP_GENERAL, e);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	return result;
}

/* a SubjectPublicKeyInfo: the algorithm, then the key bits (the BIT STRING's unused-bits octet skipped) */
__attribute__((noinline)) int MlnImpl_81038874(const MlnAsn1Node *node, MlnX509PublicKey *publicKey)
{
	MlnAsn1Node *algorithm, *key;
	int result;

	algorithm = MlnX509Child(node, 0);
	key = MlnX509Child(node, 1);
	if (algorithm == NULL || key == NULL)
		return MLN_X509_ERROR_FORMAT;
	result = MlnImpl_8103AA04(algorithm, &publicKey->algorithm);
	if (result != 0)
		return result;
	if (MLN_ASN1_IDENTIFIER(key) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_BIT_STRING))
		return MLN_X509_ERROR_FORMAT;
	publicKey->key.size = key->length - 1;
	publicKey->key.data = key->data + 1;
	return 0;
}

/* the ASN.1 layer's errors are reported as the certificate's format error */
static inline int MlnX509Error(int result)
{
	if ((result & 0xFFFF0000) == 0x22000000 || (result & 0xFFFF0000) == 0x22060000)
		return MLN_X509_ERROR_FORMAT;
	return result;
}

static inline void MlnX509FreeCertificate(MlnX509Certificate *certificate)
{
	MlnX509Tbs *tbs = certificate->tbs;

	if (tbs != NULL) {
		MlnImpl_8103AC34(tbs->extensions);
		MlnFree(MLN_HEAP_GENERAL, tbs);
	}
	MlnFree(MLN_HEAP_GENERAL, certificate);
}

/* a TBSCertificate: v3, a positive serial number, and the parts after it */
static inline int MlnX509ParseTbs(const MlnAsn1Node *node, MlnX509Tbs **out)
{
	MlnAsn1Node *version, *serial, *signature, *issuer, *validity, *subject, *publicKey;
	MlnAsn1Node *notBefore, *notAfter, *extensions;
	MlnX509Tbs *tbs;
	unsigned int i;
	int result;

	tbs = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*tbs));
	if (tbs == NULL)
		return 2;
	MlnMemset(tbs, 0, sizeof(*tbs));
	tbs->encoding.size = node->encodingLength;
	tbs->encoding.data = (unsigned char *)node->encoding;
	version = MlnX509Child(node, 0);
	serial = MlnX509Child(node, 1);
	signature = MlnX509Child(node, 2);
	issuer = MlnX509Child(node, 3);
	validity = MlnX509Child(node, 4);
	subject = MlnX509Child(node, 5);
	publicKey = MlnX509Child(node, 6);
	if (serial == NULL || version == NULL || issuer == NULL || signature == NULL ||
	    subject == NULL || validity == NULL || publicKey == NULL)
		goto format;
	if (version->tagClass != 2 || !version->constructed || version->tag != 0 ||
	    version->length != 1)
		goto format;
	result = MlnImpl_81037F54(MLN_HEAP_GENERAL, version->children[0], &tbs->version);
	if (result != 0)
		goto fail;
	if (tbs->version != 2)
		goto format;
	if (serial->tag != MLN_ASN1_TAG_INTEGER || serial->length == 0 ||
	    (signed char)serial->data[0] < 0)
		goto format;
	for (i = 0; serial->data[i] == 0; )
		if (++i == serial->length)
			goto format;
	MlnBufferSet(MLN_HEAP_GENERAL, &tbs->serialNumber, serial->length, serial->data);
	result = MlnImpl_8103AA04(signature, &tbs->signature);
	if (result != 0)
		goto fail;
	result = MlnImpl_8103AC08(issuer, &tbs->issuer);
	if (result != 0)
		goto fail;
	notBefore = MlnX509Child(validity, 0);
	notAfter = MlnX509Child(validity, 1);
	if (notBefore == NULL || notAfter == NULL)
		goto format;
	result = MlnImpl_8103AB18(notBefore, &tbs->validity.notBefore);
	if (result != 0)
		goto fail;
	result = MlnImpl_8103AB18(notAfter, &tbs->validity.notAfter);
	if (result != 0)
		goto fail;
	result = MlnImpl_8103AC08(subject, &tbs->subject);
	if (result != 0)
		goto fail;
	result = MlnImpl_81038874(publicKey, &tbs->publicKey);
	if (result != 0)
		goto fail;
	/* the extensions, [3], after the optional unique identifiers */
	for (i = 7; i < 9; i++) {
		extensions = MlnX509Child(node, i);
		if (extensions != NULL && extensions->tagClass == 2 && extensions->constructed &&
		    extensions->tag == 3)
			break;
	}
	if (i == 9) {
		tbs->extensions = NULL;
	} else {
		if (extensions->length != 1)
			goto format;
		if (extensions->children[0] == NULL) {
			tbs->extensions = NULL;
		} else {
			result = MlnImpl_8103AC98(extensions->children[0], &tbs->extensions);
			if (result != 0)
				goto fail;
		}
	}
	*out = tbs;
	return 0;
format:
	result = MLN_X509_ERROR_FORMAT;
fail:
	MlnImpl_8103AC34(tbs->extensions);
	MlnFree(MLN_HEAP_GENERAL, tbs);
	return result;
}

/* a Certificate, parsed from its DER; the tree's data is not kept */
int MlnImpl_81038900(const MlnBuffer *der, MlnX509Certificate **out)
{
	MlnAsn1Node *tree = NULL;
	MlnAsn1Node *tbs, *algorithm, *signature;
	MlnX509Certificate *certificate;
	int result;

	if (out == NULL || der == NULL) {
		result = 3;
		goto out;
	}
	certificate = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*certificate));
	if (certificate == NULL) {
		result = 2;
		goto out;
	}
	MlnMemset(certificate, 0, sizeof(*certificate));
	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, der->data, der->size, NULL, &tree);
	if (result != 0)
		goto fail;
	tbs = MlnX509Child(tree, 0);
	algorithm = MlnX509Child(tree, 1);
	signature = MlnX509Child(tree, 2);
	if (algorithm == NULL || tbs == NULL || signature == NULL) {
		result = MLN_X509_ERROR_FORMAT;
		goto fail;
	}
	result = MlnX509ParseTbs(tbs, &certificate->tbs);
	if (result != 0)
		goto fail;
	result = MlnImpl_8103AA04(algorithm, &certificate->signatureAlgorithm);
	if (result != 0)
		goto fail;
	if (MLN_ASN1_IDENTIFIER(signature) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_BIT_STRING)) {
		result = MLN_X509_ERROR_FORMAT;
		goto fail;
	}
	certificate->signature.size = signature->length - 1;
	certificate->signature.data = signature->data + 1;
	*out = certificate;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	return result;
fail:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	MlnX509FreeCertificate(certificate);
	return result;
}

static inline void MlnBufferListFree(MlnBufferList *list)
{
	int i;

	if (list == NULL)
		return;
	for (i = 0; i < list->count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, list->items[i]);
	MlnFree(MLN_HEAP_GENERAL, list->items);
	MlnFree(MLN_HEAP_GENERAL, list);
}

/* a copy of item added */
static inline __attribute__((always_inline)) int MlnBufferListAppend(MlnBufferList *list, const MlnBuffer *item)
{
	MlnBuffer *copy = NULL;
	int result;

	if (list == NULL || item == NULL || list->count >= list->capacity) {
		result = 3;
	} else {
		copy = MlnBufferCreate(MLN_HEAP_GENERAL, item->size, item->data);
		if (copy == NULL) {
			result = 2;
		} else {
			list->items[list->count] = copy;
			list->count++;
			copy = NULL;
			result = 0;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, copy);
	return result;
}

/* the item equal to key, NULL when there is none */
static inline __attribute__((always_inline)) MlnBuffer *MlnBufferListFind(const MlnBufferList *list, const MlnBuffer *key)
{
	int i;

	if (list == NULL)
		return NULL;
	for (i = 0; i < list->count; i++)
		if (MlnBufferCompare(MLN_HEAP_GENERAL, list->items[i], key) == 0)
			return list->items[i];
	return NULL;
}

/* an empty list with room for capacity buffers */
static inline __attribute__((always_inline)) int MlnBufferListCreate(int capacity,
								   MlnBufferList **out)
{
	MlnBufferList *list;

	if (out == NULL)
		return 3;
	list = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*list));
	if (list == NULL)
		return 2;
	MlnMemset(list, 0, sizeof(*list));
	list->capacity = capacity;
	if (capacity > 0) {
		list->items = MlnAllocate(MLN_HEAP_GENERAL, capacity * sizeof(MlnBuffer *));
		if (list->items == NULL) {
			MlnBufferListFree(list);
			return 2;
		}
		MlnMemset(list->items, 0, capacity * sizeof(MlnBuffer *));
	}
	*out = list;
	return 0;
}

__attribute__((noinline)) int MlnImpl_81038DB4(int capacity, MlnBufferList **out)
{
	return MlnBufferListCreate(capacity, out);
}

static inline void MlnX509Free(MlnX509 *x509)
{
	if (x509->certificate != NULL)
		MlnX509FreeCertificate(x509->certificate);
	MlnBufferDestroy(MLN_HEAP_GENERAL, x509->der);
	MlnFree(MLN_HEAP_GENERAL, x509);
}

/* a certificate and its DER freed */
int MlnImpl_81038E90(MlnX509 *x509)
{
	if (x509 != NULL)
		MlnX509Free(x509);
	return 0;
}

/* a certificate parsed from a copy of der */
int MlnImpl_81038EF4(const MlnBuffer *der, MlnX509 **out)
{
	MlnX509 *x509;
	int result;

	if (out == NULL || der == NULL) {
		result = 3;
	} else {
		x509 = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*x509));
		if (x509 == NULL) {
			result = 2;
		} else {
			MlnMemset(x509, 0, sizeof(*x509));
			x509->der = MlnBufferCreate(MLN_HEAP_GENERAL, der->size, der->data);
			if (x509->der == NULL) {
				result = 2;
			} else {
				result = MlnImpl_81038900(x509->der, &x509->certificate);
				if (result == 0) {
					*out = x509;
					return MlnX509Error(result);
				}
			}
			MlnX509Free(x509);
		}
	}
	return MlnX509Error(result);
}

/* an RSA public key's modulus and exponent, from a SubjectPublicKeyInfo's DER */
int MlnImpl_81039000(const MlnBuffer *der, MlnBuffer **modulus, MlnBuffer **exponent)
{
	MlnAsn1Node *tree = NULL;
	MlnX509PublicKey publicKey;
	int result;

	if (der == NULL || der->size == 0 || der->data == NULL) {
		result = 3;
		goto out;
	}
	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, der->data, der->size, NULL, &tree);
	if (result != 0)
		goto out;
	if (tree == NULL) {
		result = 3;
		goto out;
	}
	result = MlnImpl_81038874(tree, &publicKey);
	if (result == 0)
		result = MlnImpl_81038718(&publicKey, modulus, exponent);
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	return result;
}

/* the subject key identifier extension's key identifier, copied */
int MlnImpl_810390C0(const MlnX509 *x509, MlnBuffer **out)
{
	MlnX509Extensions *extensions;
	MlnX509Extension *extension;
	MlnAsn1Node *tree;
	MlnBuffer *identifier = NULL;
	const unsigned char *data;
	unsigned int size;
	int i, result;

	if (x509 == NULL) {
		result = 4;
		goto out;
	}
	if (out == NULL) {
		result = 3;
		goto out;
	}
	extensions = x509->certificate->tbs->extensions;
	if (extensions == NULL || extensions->count <= 0) {
		result = MLN_X509_ERROR_FORMAT;
		goto out;
	}
	for (i = 0; i < extensions->count; i++) {
		extension = extensions->items[i];
		if (extension->id == MLN_X509_EXTENSION_SUBJECT_KEY_IDENTIFIER)
			break;
	}
	if (i == extensions->count) {
		result = MLN_X509_ERROR_FORMAT;
		goto out;
	}
	tree = NULL;
	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, extension->value.data, extension->value.size,
				  NULL, &tree);
	if (result == 0)
		result = MlnImpl_81038218(MLN_HEAP_GENERAL, tree, &data, &size);
	if (result != 0) {
		MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
		MlnBufferDestroy(MLN_HEAP_GENERAL, identifier);
		return MlnX509Error(result);
	}
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	identifier = MlnBufferCreate(MLN_HEAP_GENERAL, size, data);
	if (identifier == NULL) {
		result = 2;
		goto out;
	}
	*out = identifier;
	identifier = NULL;
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, identifier);
	return result;
}

/* the certificate's RSA public key */
int MlnImpl_81039228(const MlnX509 *x509, MlnBuffer **modulus, MlnBuffer **exponent)
{
	int result;

	if (x509 == NULL)
		return 4;
	if (modulus == NULL || exponent == NULL)
		return 3;
	result = MlnImpl_81038718(&x509->certificate->tbs->publicKey, modulus, exponent);
	if (result != 0)
		result = MlnX509Error(result);
	return result;
}

/* the subject's first attribute value, a UTF8String, copied */
int MlnImpl_81039298(const MlnX509 *x509, MlnBuffer **out)
{
	const MlnX509Tbs *tbs = x509->certificate->tbs;
	MlnAsn1Node *tree = NULL;
	MlnAsn1Node *value;
	MlnBuffer *name = NULL;
	int result;

	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, tbs->subject.data, tbs->subject.size, NULL,
				  &tree);
	if (result != 0) {
		result = MlnX509Error(result);
		goto out;
	}
	value = MlnX509Child(MlnX509Child(MlnX509Child(tree, 0), 0), 1);
	if (value == NULL ||
	    MLN_ASN1_IDENTIFIER(value) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_UTF8_STRING)) {
		result = MLN_X509_ERROR_FORMAT;
		goto out;
	}
	name = MlnBufferCreate(MLN_HEAP_GENERAL, value->length, value->data);
	if (name == NULL) {
		result = 2;
	} else {
		*out = name;
		name = NULL;
	}
out:
	MlnBufferDestroy(MLN_HEAP_GENERAL, name);
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	return result;
}

#define MLN_X509_CRL_URL	"http://crl.marlin-tmo.com/crl/mtmocrls.crl"

/* where the certificate revocation list is fetched from */
unsigned int MlnImpl_810393D0(int kind, MlnBuffer **out)
{
	MlnBuffer *url = NULL;
	int result;

	if (out == NULL) {
		result = 3;
	} else {
		url = MlnBufferCreate(MLN_HEAP_GENERAL, sizeof(MLN_X509_CRL_URL) - 1, MLN_X509_CRL_URL);
		if (url == NULL) {
			result = 2;
		} else {
			*out = url;
			url = NULL;
			result = 0;
		}
	}
	MlnBufferDestroy(MLN_HEAP_GENERAL, url);
	return result;
}

/* anyPolicy, 2.5.29.32.0 */
static const MlnBuffer MlnX509AnyPolicy = { 4, (unsigned char *)"\x55\x1d\x20\x00" };

/* the KeyUsage BIT STRINGs recognised, whole encodings, and the usage each grants */
static const struct MlnX509KeyUsage {
	unsigned int flags;
	const char *encoding;
	unsigned int length;
} MlnX509KeyUsages[] = {
	{ 0x001, "\x03\x02\x07\x80", 4 },	/* digitalSignature */
	{ 0x002, "\x03\x02\x06\x40", 4 },	/* nonRepudiation */
	{ 0x004, "\x03\x02\x05\x20", 4 },	/* keyEncipherment */
	{ 0x008, "\x03\x02\x04\x10", 4 },	/* dataEncipherment */
	{ 0x010, "\x03\x02\x03\x08", 4 },	/* keyAgreement */
	{ 0x020, "\x03\x02\x02\x04", 4 },	/* keyCertSign */
	{ 0x040, "\x03\x02\x01\x02", 4 },	/* cRLSign */
	{ 0x040, "\x03\x02\x01\x80", 4 },
	{ 0x040, "\x03\x02\x01\x82", 4 },
	{ 0x080, "\x03\x02\x00\x01", 4 },	/* encipherOnly */
	{ 0x100, "\x03\x03\x07\x00\x80", 5 },	/* decipherOnly */
	{ 0x021, "\x03\x02\x02\x84", 4 },	/* digitalSignature, keyCertSign */
	{ 0x00C, "\x03\x02\x04\x30", 4 },	/* keyEncipherment, dataEncipherment */
	{ 0xFFFFFFFF, NULL, 0 },
};

/* the usage a KeyUsage extension grants, NULL for an encoding not in the table */
static inline const struct MlnX509KeyUsage *MlnX509FindKeyUsage(const MlnBuffer *value)
{
	const struct MlnX509KeyUsage *usage;

	for (usage = MlnX509KeyUsages; usage->encoding != NULL; usage++)
		if (MlnBytesCompare(MLN_HEAP_GENERAL, value->size, value->data, usage->length,
				    usage->encoding) == 0)
			return usage;
	return NULL;
}

/* the certificate's signature, checked with the working public key */
static inline int MlnX509Verify(const MlnX509Validation *state,
				const MlnX509Certificate *certificate)
{
	const MlnBuffer *modulus = state->modulus;
	const MlnBuffer *exponent = state->exponent;
	void *rsa = NULL;
	int result;

	if (modulus == NULL || exponent == NULL) {
		result = 3;
		goto out;
	}
	result = MlnImpl_81055670(&rsa, modulus, exponent, NULL, NULL, NULL, NULL, NULL, NULL);
	if (result != 0)
		goto out;
	switch (certificate->signatureAlgorithm.algorithm) {
	case MLN_X509_ALGORITHM_SHA1_RSA:
		result = MlnImpl_81056F0C(rsa, 0, &certificate->tbs->encoding, &certificate->signature);
		break;
	case MLN_X509_ALGORITHM_SHA256_RSA:
		result = MlnImpl_81056F0C(rsa, 1, &certificate->tbs->encoding, &certificate->signature);
		break;
	default:
		result = MLN_X509_ERROR_FORMAT;
		break;
	}
out:
	MlnImpl_81055ABC(rsa);
	return result;
}

/* notBefore <= time <= notAfter */
static inline int MlnX509CheckValidity(const MlnX509Validity *validity, const MlnDateTime *time)
{
	int result;

	if (validity == NULL)
		return 3;
	result = MlnImpl_8103B0C4(&validity->notBefore, time);
	if (result != 0)
		return result;
	return MlnImpl_8103B0C4(time, &validity->notAfter);
}

/* BasicConstraints: cA, and pathLenConstraint when there is one */
static inline int MlnX509ParseBasicConstraints(const MlnBuffer *value,
					       MlnX509BasicConstraints *constraints)
{
	MlnAsn1Node *tree = NULL;
	MlnAsn1Node *ca, *pathLength;
	int isCa, result;

	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, value->data, value->size, NULL, &tree);
	if (result != 0)
		goto out;
	ca = MlnX509Child(tree, 0);
	if (ca == NULL)
		goto none;
	result = MlnImpl_81037EC8(MLN_HEAP_GENERAL, ca, &isCa);
	if (result != 0)
		goto out;
	if (isCa == 0)
		goto none;
	constraints->ca = 1;
	pathLength = MlnX509Child(tree, 1);
	if (pathLength == NULL) {
		constraints->pathLength = -1;
	} else {
		result = MlnImpl_81037F54(MLN_HEAP_GENERAL, pathLength, &constraints->pathLength);
		if (result != 0)
			goto out;
	}
	goto out;
none:
	constraints->ca = 0;
	constraints->pathLength = 0;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	return result;
}

/* CertificatePolicies: each PolicyInformation's OID */
static inline int MlnX509ParsePolicies(const MlnBuffer *value, MlnBufferList **out)
{
	MlnAsn1Node *tree = NULL;
	MlnAsn1Node *information, *oid;
	MlnBufferList *list = NULL;
	unsigned int i;
	int result;

	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, value->data, value->size, NULL, &tree);
	if (result != 0)
		goto out;
	if (tree == NULL || tree->tagClass != 0 || !tree->constructed || tree->tag != 16) {
		result = MLN_X509_ERROR_FORMAT;
		goto out;
	}
	result = MlnImpl_81038DB4(tree->length, &list);
	if (result != 0)
		goto out;
	for (i = 0; i < tree->length; i++) {
		information = MlnX509Child(tree, i);
		oid = MlnX509Child(information, 0);
		if (oid == NULL || MLN_ASN1_IDENTIFIER(oid) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_OID)) {
			result = MLN_X509_ERROR_FORMAT;
			goto out;
		}
		result = MlnBufferListAppend(list, &oid->value);
		if (result != 0)
			goto out;
	}
	*out = list;
	list = NULL;
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	MlnBufferListFree(list);
	return result;
}

/* the valid policies after a certificate's CertificatePolicies */
static inline int MlnX509ProcessPolicies(const MlnBufferList *valid, const MlnBuffer *value,
					 MlnBufferList **out)
{
	MlnBufferList *policies = NULL;
	MlnBufferList *next = NULL;
	MlnBuffer any = MlnX509AnyPolicy;
	int i, j, result;

	if (valid == NULL) {
		*out = NULL;
		return 0;
	}
	result = MlnX509ParsePolicies(value, &policies);
	if (result != 0)
		goto out;
	if (MlnBufferListFind(policies, &any) != NULL) {
		/* anyPolicy asserted: the valid policies stay */
		result = MlnImpl_81038DB4(valid->count, &next);
		if (result != 0)
			goto out;
		for (i = 0; i < valid->count; i++) {
			result = MlnBufferListAppend(next, valid->items[i]);
			if (result != 0)
				goto out;
		}
	} else if (MlnBufferListFind(valid, &any) != NULL) {
		/* any policy was valid: the certificate's become */
		result = MlnImpl_81038DB4(policies->count, &next);
		if (result != 0)
			goto out;
		for (i = 0; i < policies->count; i++) {
			result = MlnBufferListAppend(next, policies->items[i]);
			if (result != 0)
				goto out;
		}
	} else {
		/* those in both */
		result = MlnImpl_81038DB4(policies->count < valid->count ? valid->count :
					  policies->count, &next);
		if (result != 0)
			goto out;
		for (i = 0; i < policies->count; i++) {
			for (j = 0; j < valid->count; j++)
				if (MlnBufferCompare(MLN_HEAP_GENERAL, policies->items[i],
						     valid->items[j]) == 0)
					break;
			if (j == valid->count)
				continue;
			result = MlnBufferListAppend(next, policies->items[i]);
			if (result != 0)
				goto out;
		}
	}
	*out = next;
	next = NULL;
out:
	MlnBufferListFree(policies);
	MlnBufferListFree(next);
	return result;
}

/* one certificate of the path checked against the state, and the state moved on to it */
static inline int MlnX509ValidationStep(MlnX509Validation *state,
					const MlnX509Certificate *certificate)
{
	const MlnX509Tbs *tbs;
	const MlnX509Extensions *extensions;
	const MlnX509Extension *extension;
	const struct MlnX509KeyUsage *usage;
	MlnX509BasicConstraints constraints;
	MlnBufferList *policies;
	MlnBuffer any;
	MlnBuffer keyIdentifier;
	MlnAsn1Node *tree;
	int pathLength, i, j, result;

	if (certificate == NULL || state == NULL)
		return 3;
	tbs = certificate->tbs;
	if (state->checkRevocation != 0 &&
	    MlnImpl_8103B1E0(&tbs->serialNumber, &tbs->issuer) != 0)
		return MLN_X509_ERROR_FORMAT;
	result = MlnX509Verify(state, certificate);
	if (result != 0)
		return result;
	if (MlnBufferCompare(MLN_HEAP_GENERAL, &certificate->tbs->issuer, state->workingIssuer) != 0)
		return MLN_X509_ERROR_FORMAT;
	if (state->checkValidity == 1) {
		result = MlnX509CheckValidity(&certificate->tbs->validity, &state->time);
		if (result != 0)
			return result;
	}
	tbs = certificate->tbs;
	if (tbs->publicKey.algorithm.algorithm != MLN_X509_ALGORITHM_RSA)
		return MLN_X509_ERROR_FORMAT;
	extensions = tbs->extensions;
	if (extensions == NULL)
		goto next;
	any = MlnX509AnyPolicy;
	policies = NULL;
	pathLength = -1;
	for (i = 0; i < extensions->count; i++) {
		extension = extensions->items[i];
		switch (extension->id) {
		case MLN_X509_EXTENSION_AUTHORITY_KEY_IDENTIFIER:
			if (state->keyIdentifier == NULL)
				break;
			result = MlnImpl_8103AFFC(&extension->value, &keyIdentifier,
						  MLN_HEAP_GENERAL);
			if (result != 0)
				goto fail;
			if (MlnBufferCompare(MLN_HEAP_GENERAL, state->keyIdentifier,
					     &keyIdentifier) != 0)
				goto format;
			break;
		case MLN_X509_EXTENSION_KEY_USAGE:
			if (state->index == state->depth) {
				usage = MlnX509FindKeyUsage(&extension->value);
				if (usage == NULL || (state->keyUsage & usage->flags) == 0)
					goto format;
			} else {
				usage = MlnX509FindKeyUsage(&extension->value);
				if (usage == NULL ||
				    (usage->flags & MLN_X509_KEY_USAGE_KEY_CERT_SIGN) == 0)
					goto format;
			}
			break;
		case MLN_X509_EXTENSION_BASIC_CONSTRAINTS:
			if (state->index == state->depth)
				break;
			result = MlnX509ParseBasicConstraints(&extension->value, &constraints);
			if (result != 0)
				goto fail;
			if (constraints.ca != 1)
				goto format;
			pathLength = constraints.pathLength;
			break;
		case MLN_X509_EXTENSION_CRL_DISTRIBUTION_POINTS:
			break;
		case MLN_X509_EXTENSION_CERTIFICATE_POLICIES:
			result = MlnX509ProcessPolicies(state->validPolicies, &extension->value,
							&policies);
			if (result != 0)
				goto fail;
			break;
		default:
			if (extension->critical == 1)
				goto format;
			break;
		}
	}
	/* the subject key identifier the next certificate's authority key identifier names */
	for (i = 0; i < extensions->count; i++) {
		extension = extensions->items[i];
		if (extension->id != MLN_X509_EXTENSION_SUBJECT_KEY_IDENTIFIER)
			continue;
		tree = NULL;
		result = MlnImpl_810378E4(MLN_HEAP_GENERAL, extension->value.data,
					  extension->value.size, NULL, &tree);
		if (result == 0)
			result = MlnImpl_81038218(MLN_HEAP_GENERAL, tree,
						  (const unsigned char **)&keyIdentifier.data,
						  &keyIdentifier.size);
		if (result != 0) {
			MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
			goto fail;
		}
		MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
		MlnBufferDestroy(MLN_HEAP_GENERAL, state->keyIdentifier);
		state->keyIdentifier = NULL;
		state->keyIdentifier = MlnBufferCreate(MLN_HEAP_GENERAL, keyIdentifier.size,
						       keyIdentifier.data);
		if (state->keyIdentifier == NULL) {
			result = 2;
			goto fail;
		}
		break;
	}
	if (state->index != state->depth) {
		if (state->maxPathLength <= 0)
			goto format;
		state->maxPathLength--;
		if (pathLength > 0 && state->maxPathLength > pathLength)
			state->maxPathLength = pathLength;
	}
	/* the policies the caller accepts: one of them must stay valid */
	if (state->userPolicies != NULL && state->userPolicies->count != 0) {
		if (policies != NULL) {
			if (state->index == state->depth &&
			    MlnBufferListFind(policies, &any) == NULL) {
				for (j = 0; j < state->userPolicies->count; j++)
					if (MlnBufferListFind(policies,
							      state->userPolicies->items[j]) != NULL)
						break;
				if (j == state->userPolicies->count)
					goto format;
			}
		} else if (state->userPolicies->count > 1 ||
			   MlnBytesCompare(MLN_HEAP_GENERAL, state->userPolicies->items[0]->size,
					   state->userPolicies->items[0]->data, any.size,
					   any.data) != 0) {
			return MLN_X509_ERROR_FORMAT;
		}
	}
	MlnBufferListFree(state->validPolicies);
	state->validPolicies = policies;
next:
	state->index++;
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->workingIssuer);
	state->workingIssuer = NULL;
	state->workingIssuer = MlnBufferCreate(MLN_HEAP_GENERAL, tbs->subject.size,
					       tbs->subject.data);
	if (state->workingIssuer == NULL)
		return 2;
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->modulus);
	state->modulus = NULL;
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->exponent);
	state->exponent = NULL;
	return MlnImpl_81038718(&certificate->tbs->publicKey, &state->modulus, &state->exponent);
format:
	result = MLN_X509_ERROR_FORMAT;
fail:
	MlnBufferListFree(policies);
	return result;
}

static inline void MlnX509ValidationFree(MlnX509Validation *state)
{
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->keyIdentifier);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->exponent);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->modulus);
	MlnBufferDestroy(MLN_HEAP_GENERAL, state->workingIssuer);
	MlnBufferListFree(state->validPolicies);
	MlnBufferListFree(state->userPolicies);
	MlnFree(MLN_HEAP_GENERAL, state);
}

/* the state at the trust anchor */
static inline int MlnX509ValidationCreate(const MlnX509ValidationParameters *parameters,
					  MlnX509Validation **out)
{
	MlnX509Validation *state;
	int i, result;

	state = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*state));
	if (state == NULL)
		return 2;
	MlnMemset(state, 0, sizeof(*state));
	state->checkValidity = parameters->checkValidity;
	state->checkRevocation = parameters->checkRevocation;
	state->time.year = parameters->time.year;
	state->time.month = parameters->time.month;
	state->time.day = parameters->time.day;
	state->time.hour = parameters->time.hour;
	state->time.minute = parameters->time.minute;
	state->time.second = parameters->time.second;
	state->time.microsecond = parameters->time.microsecond;
	state->time.utcOffset = parameters->time.utcOffset;
	state->time.daylightSaving = parameters->time.daylightSaving;
	state->depth = parameters->depth;
	result = MlnImpl_81038DB4(parameters->policyCount, &state->userPolicies);
	if (result != 0)
		goto fail;
	for (i = 0; i < parameters->policyCount; i++) {
		result = MlnBufferListAppend(state->userPolicies, &parameters->policies[i]);
		if (result != 0)
			goto fail;
	}
	state->keyUsage = parameters->keyUsage;
	state->maxPathLength = parameters->depth;
	result = MlnBufferListCreate(1, &state->validPolicies);
	if (result != 0)
		goto fail;
	result = MlnBufferListAppend(state->validPolicies, &MlnX509AnyPolicy);
	if (result != 0)
		goto fail;
	state->index = 1;
	result = 2;
	state->workingIssuer = MlnBufferCreate(MLN_HEAP_GENERAL, parameters->anchorName.size,
					       parameters->anchorName.data);
	if (state->workingIssuer == NULL)
		goto fail;
	if (parameters->anchorKey.algorithm != MLN_X509_ALGORITHM_RSA) {
		result = 3;
		goto fail;
	}
	state->modulus = MlnBufferCreate(MLN_HEAP_GENERAL, parameters->anchorKey.modulus.size,
					 parameters->anchorKey.modulus.data);
	if (state->modulus == NULL)
		goto fail;
	state->exponent = MlnBufferCreate(MLN_HEAP_GENERAL, parameters->anchorKey.exponent.size,
					  parameters->anchorKey.exponent.data);
	if (state->exponent == NULL)
		goto fail;
	state->keyIdentifier = NULL;
	*out = state;
	return 0;
fail:
	MlnX509ValidationFree(state);
	return result;
}

/* a path of certificates validated from the trust anchor the parameters name */
int MlnImpl_81039418(const MlnX509ValidationParameters *parameters,
		     const MlnX509Certificates *certificates)
{
	MlnX509Validation *state = NULL;
	MlnX509Certificate *certificate = NULL;
	int i, result;

	if (certificates == NULL || parameters == NULL) {
		result = 3;
		goto out;
	}
	result = MlnX509ValidationCreate(parameters, &state);
	if (result != 0)
		goto done;
	for (i = 0; i < certificates->count; i++) {
		result = MlnImpl_81038900(certificates->items[i], &certificate);
		if (result != 0)
			goto done;
		result = MlnX509ValidationStep(state, certificate);
		if (result != 0)
			goto done;
		MlnX509FreeCertificate(certificate);
		certificate = NULL;
	}
done:
	if (certificate != NULL)
		MlnX509FreeCertificate(certificate);
	if (state != NULL)
		MlnX509ValidationFree(state);
	if (result == 0)
		return 0;
out:
	return MlnX509Error(result);
}

/* a SEQUENCE OF the given DER encodings, encoded */
int MlnImpl_8103A888(const MlnX509Certificates *certificates, MlnBuffer **out)
{
	MlnAsn1Node *sequence = NULL;
	MlnAsn1Node **nodes;
	unsigned char *data = NULL;
	unsigned int size = 0;
	int i, result;

	if (certificates == NULL) {
		result = 3;
		goto out;
	}
	nodes = MlnAllocate(MLN_HEAP_GENERAL, certificates->count * sizeof(MlnAsn1Node *));
	if (nodes == NULL) {
		result = 2;
		goto out;
	}
	MlnMemset(nodes, 0, certificates->count * sizeof(MlnAsn1Node *));
	for (i = 0; i < certificates->count; i++) {
		result = MlnImpl_81037918(MLN_HEAP_GENERAL, certificates->items[i]->data,
					  certificates->items[i]->size, NULL, &nodes[i]);
		if (result != 0)
			goto fail;
	}
	result = MlnImpl_81036E34(MLN_HEAP_GENERAL, 0, 16, &sequence);
	if (result != 0)
		goto fail;
	sequence->children = nodes;
	sequence->length = certificates->count;
	result = MlnImpl_810368D0(MLN_HEAP_GENERAL, sequence, &data, &size);
	if (result != 0)
		goto out;
	*out = MlnBufferCreate(MLN_HEAP_GENERAL, size, data);
	if (*out == NULL)
		result = 2;
	goto out;
fail:
	if (sequence == NULL)
		for (i = 0; i < certificates->count; i++)
			MlnImpl_81036FC8(MLN_HEAP_GENERAL, nodes[i]);
	MlnFree(MLN_HEAP_GENERAL, nodes);
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, sequence);
	MlnFree(MLN_HEAP_GENERAL, data);
	return result;
}

/* an OID the tables know, and its id */
struct MlnX509Oid {
	int id;
	const char *oid;
	unsigned int length;
};

static const struct MlnX509Oid MlnX509Algorithms[] = {
	{ MLN_X509_ALGORITHM_SHA1_RSA, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x05", 9 },
	{ MLN_X509_ALGORITHM_SHA256_RSA, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b", 9 },
	{ MLN_X509_ALGORITHM_RSA, "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01", 9 },
	{ -1, NULL, 0 },
};

static const struct MlnX509Oid MlnX509ExtensionOids[] = {
	{ MLN_X509_EXTENSION_AUTHORITY_KEY_IDENTIFIER, "\x55\x1d\x23", 3 },
	{ MLN_X509_EXTENSION_SUBJECT_KEY_IDENTIFIER, "\x55\x1d\x0e", 3 },
	{ MLN_X509_EXTENSION_KEY_USAGE, "\x55\x1d\x0f", 3 },
	{ MLN_X509_EXTENSION_BASIC_CONSTRAINTS, "\x55\x1d\x13", 3 },
	{ MLN_X509_EXTENSION_CRL_DISTRIBUTION_POINTS, "\x55\x1d\x1f", 3 },
	{ MLN_X509_EXTENSION_CERTIFICATE_POLICIES, "\x55\x1d\x20", 3 },
	{ MLN_X509_EXTENSION_CRL_NUMBER, "\x55\x1d\x14", 3 },
	{ MLN_X509_EXTENSION_CERTIFICATE_ISSUER, "\x55\x1d\x1d", 3 },
	{ MLN_X509_EXTENSION_ISSUING_DISTRIBUTION_POINT, "\x55\x1d\x1c", 3 },
	{ -1, NULL, 0 },
};

/* the id a table gives an OID, 0 when it has none */
static inline void MlnX509FindOid(const struct MlnX509Oid *table, const MlnAsn1Node *oid, int *id)
{
	int i;

	*id = 0;
	for (i = 0; table[i].oid != NULL; i++) {
		if (MlnBytesCompare(MLN_HEAP_GENERAL, oid->length, oid->data, table[i].length,
				    table[i].oid) == 0) {
			*id = table[i].id;
			break;
		}
	}
}

/* an AlgorithmIdentifier */
__attribute__((noinline)) int MlnImpl_8103AA04(const MlnAsn1Node *node, MlnX509Algorithm *algorithm)
{
	MlnAsn1Node *oid, *parameters;

	if (algorithm == NULL || node == NULL)
		return 3;
	oid = MlnX509Child(node, 0);
	parameters = MlnX509Child(node, 1);
	if (oid == NULL || MLN_ASN1_IDENTIFIER(oid) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_OID))
		return MLN_X509_ERROR_FORMAT;
	MlnX509FindOid(MlnX509Algorithms, oid, &algorithm->algorithm);
	algorithm->oid.size = oid->length;
	algorithm->oid.data = oid->data;
	if (parameters == NULL) {
		algorithm->parameters.size = 0;
		algorithm->parameters.data = NULL;
	} else {
		algorithm->parameters.size = parameters->encodingLength;
		algorithm->parameters.data = (unsigned char *)parameters->encoding;
	}
	return 0;
}

/* a Time: a UTCTime for 1950 to 2049, a GeneralizedTime after; whole seconds, UTC */
__attribute__((noinline)) int MlnImpl_8103AB18(const MlnAsn1Node *node, MlnDateTime *time)
{
	MlnDateTime decoded;

	if (node == NULL || time == NULL)
		return 3;
	if (MlnImpl_810382F8(MLN_HEAP_GENERAL, node, 50, &decoded) == 0) {
		if ((unsigned int)(decoded.year - 1950) > 99 || node->length != 13)
			return MLN_X509_ERROR_FORMAT;
	} else {
		if (MlnImpl_8103833C(MLN_HEAP_GENERAL, node, &decoded) != 0)
			return MLN_X509_ERROR_FORMAT;
		if (decoded.year <= 2048 || node->length != 15)
			return MLN_X509_ERROR_FORMAT;
	}
	if (decoded.microsecond != 0 || decoded.utcOffset != 0)
		return MLN_X509_ERROR_FORMAT;
	time->year = decoded.year;
	time->month = decoded.month;
	time->day = decoded.day;
	time->hour = decoded.hour;
	time->minute = decoded.minute;
	time->second = decoded.second;
	time->utcOffset = 0;
	time->daylightSaving = 0;
	return 0;
}

/* a Name, kept as its DER */
__attribute__((noinline)) int MlnImpl_8103AC08(const MlnAsn1Node *node, MlnBuffer *name)
{
	if (node == NULL || name == NULL)
		return 3;
	name->size = node->encodingLength;
	name->data = (unsigned char *)node->encoding;
	return 0;
}

static inline __attribute__((always_inline)) void MlnX509FreeExtensions(MlnX509Extensions *extensions)
{
	int i;

	if (extensions != NULL) {
		if (extensions->items != NULL) {
			for (i = 0; i < extensions->count; i++)
				MlnFree(MLN_HEAP_GENERAL, extensions->items[i]);
			MlnFree(MLN_HEAP_GENERAL, extensions->items);
		}
		MlnFree(MLN_HEAP_GENERAL, extensions);
	}
}

__attribute__((noinline)) int MlnImpl_8103AC34(MlnX509Extensions *extensions)
{
	MlnX509FreeExtensions(extensions);
	return 0;
}

/* an Extension: extnID, critical (default FALSE), extnValue's contents */
static inline int MlnX509ParseExtension(const MlnAsn1Node *node, MlnX509Extension **out)
{
	MlnX509Extension *extension = NULL;
	MlnAsn1Node *oid, *critical, *value;
	int isCritical, result;

	if (out == NULL || node == NULL) {
		result = 3;
		goto out;
	}
	extension = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*extension));
	if (extension == NULL) {
		result = 2;
		goto out;
	}
	MlnMemset(extension, 0, sizeof(*extension));
	oid = MlnX509Child(node, 0);
	if (oid == NULL || MLN_ASN1_IDENTIFIER(oid) != MLN_ASN1_ID(0, 0, MLN_ASN1_TAG_OID))
		goto format;
	MlnX509FindOid(MlnX509ExtensionOids, oid, &extension->id);
	extension->oid.size = oid->length;
	extension->oid.data = oid->data;
	if (node->length == 2) {
		value = MlnX509Child(node, 1);
		extension->critical = 0;
	} else if (node->length == 3) {
		critical = MlnX509Child(node, 1);
		value = MlnX509Child(node, 2);
		if (critical == NULL) {
			extension->critical = 0;
		} else {
			result = MlnImpl_81037EC8(MLN_HEAP_GENERAL, critical, &isCritical);
			if (result != 0)
				goto out;
			extension->critical = isCritical != 0 ? 1 : 0;
		}
	} else {
		goto format;
	}
	if (value == NULL)
		goto format;
	result = MlnImpl_81038218(MLN_HEAP_GENERAL, value,
				  (const unsigned char **)&extension->value.data,
				  &extension->value.size);
	if (result != 0)
		goto out;
	*out = extension;
	extension = NULL;
	goto out;
format:
	result = MLN_X509_ERROR_FORMAT;
out:
	MlnFree(MLN_HEAP_GENERAL, extension);
	return result;
}

/* Extensions */
__attribute__((noinline)) int MlnImpl_8103AC98(const MlnAsn1Node *node, MlnX509Extensions **out)
{
	MlnX509Extensions *extensions;
	int i, result;

	if (out == NULL || node == NULL)
		return 3;
	extensions = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*extensions));
	if (extensions == NULL)
		return 2;
	MlnMemset(extensions, 0, sizeof(*extensions));
	if (node->tagClass != 0 || !node->constructed || node->tag != 16) {
		result = MLN_X509_ERROR_FORMAT;
		goto fail;
	}
	extensions->count = node->length;
	if (extensions->count > 0) {
		extensions->items = MlnAllocate(MLN_HEAP_GENERAL,
						extensions->count * sizeof(MlnX509Extension *));
		if (extensions->items == NULL) {
			result = 2;
			goto fail;
		}
		MlnMemset(extensions->items, 0, extensions->count * sizeof(MlnX509Extension *));
		for (i = 0; i < extensions->count; i++) {
			result = MlnX509ParseExtension(node->children[i], &extensions->items[i]);
			if (result != 0)
				goto fail;
		}
	}
	*out = extensions;
	return 0;
fail:
	MlnX509FreeExtensions(extensions);
	return result;
}

/* an AuthorityKeyIdentifier's keyIdentifier, empty when it has none */
__attribute__((noinline)) int MlnImpl_8103AFFC(const MlnBuffer *value, MlnBuffer *keyIdentifier, int heap)
{
	MlnAsn1Node *tree = NULL;
	MlnAsn1Node *identifier;
	int result;

	if (value == NULL || keyIdentifier == NULL) {
		result = 3;
		goto out;
	}
	result = MlnImpl_810378E4(MLN_HEAP_GENERAL, value->data, value->size, NULL, &tree);
	if (result != 0)
		goto out;
	identifier = MlnX509Child(tree, 0);
	if (identifier != NULL && MLN_ASN1_IDENTIFIER(identifier) == MLN_ASN1_ID(2, 0, 0)) {
		keyIdentifier->size = identifier->length;
		keyIdentifier->data = identifier->data;
	} else {
		keyIdentifier->size = 0;
		keyIdentifier->data = NULL;
	}
out:
	MlnImpl_81036FC8(MLN_HEAP_GENERAL, tree);
	return result;
}

/* earlier is not after later, to the second */
__attribute__((noinline)) int MlnImpl_8103B0C4(const MlnDateTime *earlier, const MlnDateTime *later)
{
	int year, month, day, hour, minute, second;
	int laterYear, laterMonth, laterDay, laterHour, laterMinute, laterSecond;

	if (earlier == NULL || later == NULL)
		return 3;
	year = earlier->year;
	month = earlier->month;
	day = earlier->day;
	hour = earlier->hour;
	minute = earlier->minute;
	second = earlier->second;
	laterYear = later->year;
	laterMonth = later->month;
	laterDay = later->day;
	laterHour = later->hour;
	laterMinute = later->minute;
	laterSecond = later->second;
	if (year > laterYear)
		return MLN_X509_ERROR_FORMAT;
	if (year < laterYear)
		return 0;
	if (month > laterMonth)
		return MLN_X509_ERROR_FORMAT;
	if (month < laterMonth)
		return 0;
	if (day > laterDay)
		return MLN_X509_ERROR_FORMAT;
	if (day < laterDay)
		return 0;
	if (hour > laterHour)
		return MLN_X509_ERROR_FORMAT;
	if (hour < laterHour)
		return 0;
	if (minute > laterMinute)
		return MLN_X509_ERROR_FORMAT;
	if (minute < laterMinute)
		return 0;
	if (second > laterSecond)
		return MLN_X509_ERROR_FORMAT;
	return 0;
}

/* whether the stored CRL is current: *current is 0 without one */
unsigned int MlnImpl_8103B190(int *current)
{
	MlnShared *shared = MlnImpl_81006EAC();
	MlnX509CrlStore *store = shared->buffers[3];

	if (store == NULL) {
		store = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*store));
		if (store == NULL)
			return 2;
		store->crl = NULL;
		shared->buffers[3] = store;
	}
	return MlnImpl_81058B80(store->crl, current);
}

/* 1 if the stored CRL revokes the certificate serialNumber of issuer */
int MlnImpl_8103B1E0(const MlnBuffer *serialNumber, const MlnBuffer *issuer)
{
	MlnShared *shared = MlnImpl_81006EAC();
	MlnX509CrlStore *store = shared->buffers[3];
	MlnX509CrlEntries *revoked;
	MlnX509CrlEntry *entry;
	int i;

	if (store == NULL) {
		store = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*store));
		if (store == NULL)
			return 0;
		store->crl = NULL;
		shared->buffers[3] = store;
	}
	if (store->crl == NULL)
		return 0;
	revoked = store->crl->certificate->tbs->revokedCertificates;
	for (i = 0; i < revoked->count; i++) {
		entry = revoked->items[i];
		if (MlnBufferCompareKey(MLN_HEAP_GENERAL, serialNumber, &entry->serialNumber) == 0 &&
		    MlnBufferCompareKey(MLN_HEAP_GENERAL, issuer, &entry->certificateIssuer) == 0)
			return 1;
	}
	return 0;
}

/*
 * stores the CRL der, which must be newer than the stored one (a higher CRL
 * number); NULL drops the stored CRL
 */
unsigned int MlnImpl_8103B29C(const MlnBuffer *der)
{
	MlnShared *shared = MlnImpl_81006EAC();
	MlnX509CrlStore *store = shared->buffers[3];
	MlnX509Crl *crl = NULL;
	MlnX509CrlNumber stored;
	MlnX509CrlNumber number;
	unsigned int ret;
	int order;

	if (store == NULL) {
		store = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*store));
		if (store == NULL) {
			ret = 2;
			goto out;
		}
		store->crl = NULL;
		shared->buffers[3] = store;
	}
	if (der == NULL) {
		ret = 0;
		MlnImpl_81058AD0(store->crl);
		store->crl = NULL;
		goto out;
	}
	ret = MlnImpl_810583CC(der, &crl);
	if (ret != 0)
		goto out;
	if (store->crl != NULL) {
		stored = store->crl->certificate->tbs->crlNumber;
		number = crl->certificate->tbs->crlNumber;
		ret = MlnImpl_81058C0C(&number, &stored, &order);
		if (ret != 0)
			goto out;
		if (order <= 0) {
			ret = 0x22010004;
			goto out;
		}
	}
	MlnImpl_81058AD0(store->crl);
	store->crl = crl;
	crl = NULL;
out:
	MlnImpl_81058AD0(crl);
	return ret;
}

/* the stored CRL's number, all zero without a CRL */
unsigned int MlnImpl_8103B3EC(MlnX509CrlNumber *number)
{
	MlnShared *shared = MlnImpl_81006EAC();
	MlnX509CrlStore *store = shared->buffers[3];

	if (store == NULL) {
		store = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*store));
		if (store == NULL)
			return 2;
		store->crl = NULL;
		shared->buffers[3] = store;
	}
	if (number == NULL)
		return 3;
	if (store->crl == NULL) {
		MlnMemset(number, 0, sizeof(*number));
		return 0;
	}
	*number = store->crl->certificate->tbs->crlNumber;
	return 0;
}

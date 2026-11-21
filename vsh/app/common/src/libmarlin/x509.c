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
	result = MlnImpl_8103AB18(notBefore, &tbs->notBefore);
	if (result != 0)
		goto fail;
	result = MlnImpl_8103AB18(notAfter, &tbs->notAfter);
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

static inline void MlnX509FreeChain(MlnX509Chain *chain)
{
	int i;

	for (i = 0; i < chain->count; i++)
		MlnBufferDestroy(MLN_HEAP_GENERAL, chain->certificates[i]);
	MlnFree(MLN_HEAP_GENERAL, chain->certificates);
	MlnFree(MLN_HEAP_GENERAL, chain);
}

/* an empty chain with room for capacity certificates */
int MlnImpl_81038DB4(int capacity, MlnX509Chain **out)
{
	MlnX509Chain *chain;

	if (out == NULL)
		return 3;
	chain = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*chain));
	if (chain == NULL)
		return 2;
	MlnMemset(chain, 0, sizeof(*chain));
	chain->capacity = capacity;
	if (capacity > 0) {
		chain->certificates = MlnAllocate(MLN_HEAP_GENERAL, capacity * sizeof(MlnBuffer *));
		if (chain->certificates == NULL) {
			MlnX509FreeChain(chain);
			return 2;
		}
		MlnMemset(chain->certificates, 0, capacity * sizeof(MlnBuffer *));
	}
	*out = chain;
	return 0;
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

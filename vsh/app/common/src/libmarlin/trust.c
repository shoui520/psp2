/*
 * A certificate path validated for a purpose: the purpose's trust anchor, certificate
 * policy and key usage (0x8105725C-). The file's name is ours.
 */
#include "mln_internal.h"

/* the Marlin policies, 1.3.6.1.4.1.23727.<branch>.2.10.<n> */
#define MLN_POLICY_OID(branch, n) \
	"\x2B\x06\x01\x04\x01\x81\xB9\x2F" branch "\x02\x0A" n
#define MLN_POLICY_OID_SIZE	12

#define MLN_KEY_USAGE_DIGITAL_SIGNATURE	1
#define MLN_KEY_USAGE_KEY_ENCIPHERMENT	4

/* a trust anchor's name and RSA key */
static inline void MlnTrustSetAnchor(MlnX509ValidationParameters *parameters, int anchor)
{
	parameters->anchorName.size = MlnImpl_81058D90(anchor);
	parameters->anchorName.data = (unsigned char *)MlnImpl_81058CA4(anchor);
	parameters->anchorKey.modulus.size = MlnImpl_81058DEC(anchor);
	parameters->anchorKey.modulus.data = (unsigned char *)MlnImpl_81058CFC(anchor);
	parameters->anchorKey.exponent.size = MlnImpl_81058E20(anchor);
	parameters->anchorKey.exponent.data = (unsigned char *)MlnImpl_81058D58(anchor);
}

/* the trust anchor: the platform's anchors 0 and 1, any other the last one */
void MlnImpl_8105725C(MlnX509ValidationParameters *parameters, int anchor)
{
	parameters->anchorKey.algorithm = MLN_X509_ALGORITHM_RSA;
	if (anchor == 0 || anchor == 1)
		MlnTrustSetAnchor(parameters, anchor);
	else
		MlnTrustSetAnchor(parameters, 2);
}

static inline void MlnTrustSetPolicy(MlnX509ValidationParameters *parameters,
				     const char *oid, unsigned int keyUsage)
{
	parameters->policies->size = MLN_POLICY_OID_SIZE;
	parameters->policies->data = (unsigned char *)oid;
	parameters->keyUsage = keyUsage;
}

/* validate a certificate path, its end certificate first, for a purpose */
int MlnImpl_81057314(int purpose, const MlnX509Certificates *certificates)
{
	unsigned int seconds;
	MlnX509ValidationParameters parameters;
	int ret;

	MlnMemset(&parameters, 0, sizeof(parameters));
	if (certificates == NULL)
		goto invalid;
	parameters.checkRevocation = 1;
	parameters.checkValidity = 1;
	parameters.depth = certificates->count;
	ret = MlnImpl_81052EBC(&seconds);
	if (ret != 0)
		goto out;
	ret = MlnTimeFromSeconds(&seconds, 0, 0, &parameters.time);
	if (ret != 0)
		goto out;
	parameters.policyCount = 1;
	parameters.policies = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*parameters.policies));
	if (parameters.policies == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(parameters.policies, 0, sizeof(*parameters.policies));

	switch (purpose) {
	case 1:
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x06"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		parameters.checkValidity = 0;
		break;
	case 2:
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x05"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 3:
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x02", "\x04"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		parameters.checkValidity = 0;
		break;
	case 4:
		parameters.checkRevocation = 0;
		parameters.checkValidity = 0;
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x02", "\x05"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 5:
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x02", "\x07"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 6:
		parameters.checkRevocation = 0;
		parameters.checkValidity = 0;
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x02", "\x06"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 7:
		parameters.anchorKey.algorithm = MLN_X509_ALGORITHM_RSA;
		MlnTrustSetAnchor(&parameters, 2);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x05"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 8:
		parameters.anchorKey.algorithm = MLN_X509_ALGORITHM_RSA;
		MlnTrustSetAnchor(&parameters, 2);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x04"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 9:
		MlnImpl_8105725C(&parameters, 1);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x03"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		break;
	case 10:
		MlnImpl_8105725C(&parameters, 1);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x03"),
				  MLN_KEY_USAGE_KEY_ENCIPHERMENT);
		break;
	case 11:
		MlnImpl_8105725C(&parameters, 1);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x01"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		parameters.checkValidity = 0;
		break;
	case 12:
		MlnImpl_8105725C(&parameters, 1);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x01", "\x01"),
				  MLN_KEY_USAGE_KEY_ENCIPHERMENT);
		parameters.checkValidity = 0;
		break;
	case 15:
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x02", "\x07"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		parameters.checkRevocation = 0;
		parameters.checkValidity = 0;
		break;
	case 0:
		parameters.checkRevocation = 0;
		parameters.checkValidity = 0;
		/* fall through */
	case 19:
		MlnImpl_8105725C(&parameters, 0);
		MlnTrustSetPolicy(&parameters, MLN_POLICY_OID("\x02", "\x03"),
				  MLN_KEY_USAGE_DIGITAL_SIGNATURE);
		parameters.checkValidity = 0;
		break;
	default:
		goto invalid;
	}
	ret = MlnImpl_81039418(&parameters, certificates);
	goto out;
invalid:
	ret = 3;
out:
	MlnFree(MLN_HEAP_GENERAL, parameters.policies);
	return ret;
}

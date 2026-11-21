/*
 * The WS-Security elements of the protocol's SOAP messages. The file's name is ours.
 */
#include "mln_internal.h"

/* a new string of three: the first and the last are C strings, the middle one a buffer */
static inline __attribute__((always_inline)) int
MlnStringJoin(const char *first, const MlnBuffer *middle, const char *last, char **out)
{
	unsigned int size = MlnStrlen(first) + middle->size + 1 + MlnStrlen(last);
	char *string;
	int ret = 0;

	string = MlnAllocate(MLN_HEAP_SECURE, size);
	if (string == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(string, 0, size);
	MlnStrncat(string, first, MlnStrlen(first));
	MlnStrncat(string, (const char *)middle->data, middle->size);
	MlnStrncat(string, last, MlnStrlen(last));
	*out = string;
	string = NULL;
out:
	MlnFree(MLN_HEAP_SECURE, string);
	return ret;
}

/*
 * A wsse:SecurityTokenReference to an embedded X509PKIPathv1 token: the certificates, last
 * first, as a DER sequence in base64, for the target usage given.
 */
int MlnImpl_81034D38(const char *usage, int count, MlnBuffer **certificates, char **out)
{
	MlnBuffer target = { MlnStrlen(usage), (unsigned char *)usage };
	MlnAsn1Node *path = NULL;
	MlnAsn1Node *certificate = NULL;
	unsigned char *der = NULL;
	unsigned int derSize = 0;
	MlnBuffer encoded = { 0, NULL };
	MlnBuffer *base64 = NULL;
	char *head = NULL;
	char *reference;
	int i, err, ret;

	err = MlnImpl_81037E6C(MLN_HEAP_SECURE, &path);
	if (err != 0)
		goto fail;
	for (i = count - 1; i >= 0; i--) {
		err = MlnImpl_81037918(MLN_HEAP_SECURE, certificates[i]->data,
				       certificates[i]->size, 0, &certificate);
		if (err != 0)
			goto fail;
		err = MlnImpl_81037948(MLN_HEAP_SECURE, path, certificate, 0);
		if (err != 0)
			goto fail;
		certificate = NULL;
	}
	err = MlnImpl_810368D0(MLN_HEAP_SECURE, path, &der, &derSize);
	if (err != 0)
		goto fail;
	encoded.size = derSize;
	encoded.data = der;
	err = MlnImpl_81026814(MLN_HEAP_SECURE, &encoded, 0, 0, &base64);
	if (err != 0)
		goto fail;
	ret = MlnStringJoin("<wsse:SecurityTokenReference nemosec:TargetUsage=\""
			    "http://nemo.intertrust.com/2005/10/security/secure-protocol/basic/1.0#",
			    &target,
			    "\"><wsse:Embedded><wsse:BinarySecurityToken ValueType=\""
			    "http://docs.oasis-open.org/wss/2004/01/"
			    "oasis-200401-wss-x509-token-profile-1.0#X509PKIPathv1\" EncodingType=\""
			    "http://docs.oasis-open.org/wss/2004/01/"
			    "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">",
			    &head);
	if (ret != 0)
		goto out;
	ret = MlnStringJoin(head, base64,
			    "</wsse:BinarySecurityToken></wsse:Embedded></wsse:SecurityTokenReference>",
			    &reference);
	if (ret != 0)
		goto out;
	*out = reference;
	goto out;
fail:
	ret = err == 2 ? 2 : 7;
out:
	MlnImpl_81036FD4(MLN_HEAP_SECURE, der);
	MlnBufferDestroy(MLN_HEAP_SECURE, base64);
	MlnFree(MLN_HEAP_SECURE, head);
	MlnImpl_81036FC8(MLN_HEAP_SECURE, path);
	MlnImpl_81036FC8(MLN_HEAP_SECURE, certificate);
	return ret;
}

/* the license transfer service's configuration, around its node's info */
#define MLN_LTP_CONFIG_HEAD \
	"<BroadbandServiceConfig\n" \
	"    xmlns=\"urn:marlin:broadband:1-2:nemo:services:configuration\"\n" \
	"    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n" \
	"    xsi:schemaLocation=\"urn:marlin:broadband:1-2:nemo:services:configuration   Broadband-services-config.xsd\n" \
	"       http://nemo.intertrust.com/2005/10/security  NEMOWSSecureMessaging.xsd\n" \
	"       urn:marlin:organization:sony:nemo:services:schemas:configuration:1-0  ServiceConfig.xsd\"\n" \
	"    xmlns:nemoc=\"http://nemo.intertrust.com/2005/10/core\"\n" \
	"    xmlns:nemosec=\"http://nemo.intertrust.com/2005/10/security\"\n" \
	"    xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\"\n" \
	"    xmlns:dcsi=\"urn:marlin:broadband:1-1:nemo:services:schemas:data-certification-service:data-item\"\n" \
	"    xmlns:sbsc=\"urn:marlin:organization:sony:nemo:services:schemas:configuration:1-0\"\n" \
	"    xmlns:saml=\"urn:oasis:names:tc:SAML:1.0:assertion\"\n" \
	"    xmlns:ds=\"http://www.w3.org/2000/09/xmldsig#\"\n" \
	"    broadbandServiceId=\"urn:marlin:organization:sony:ltp-service\"\n" \
	"    broadbandServiceFriendlyName=\"Sony Connect Service\"\n" \
	"    configVersion=\"0\">\n" \
	"  <LtpServiceConfig>\n" \
	"    <nemoc:NodeInfo>"

#define MLN_LTP_CONFIG_TAIL \
	"    </nemoc:NodeInfo>\n" \
	"    <wsdl:definitions xmlns=\"urn:marlin:core:1-1:nemo:services\"\n" \
	"      xmlns:nemosec=\"http://nemo.intertrust.com/2004/security\"\n" \
	"      xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\"\n" \
	"      xmlns:lt=\"urn:marlin:core:1-1:nemo:services:schemas\"\n" \
	"      xmlns:wsdl=\"http://schemas.xmlsoap.org/wsdl/\"\n" \
	"      xmlns:wsdlsoap=\"http://schemas.xmlsoap.org/wsdl/soap/\"\n" \
	"      targetNamespace=\"urn:marlin:core:1-1:nemo:services\" name=\"license-transfer\">\n" \
	"      <wsdl:types>\n" \
	"        <xsd:schema>\n" \
	"          <xsd:import namespace=\"urn:marlin:core:1-1:nemo:services:schemas\"\n" \
	"            schemaLocation=\"marlin-nemo-core-services.xsd\"/>\n" \
	"        </xsd:schema>\n" \
	"      </wsdl:types>\n" \
	"      <wsdl:message name=\"requestLicenseTransferRequest\">\n" \
	"        <wsdl:part name=\"requestLicenseTransferReturn\" element=\"lt:LicenseTransferRequestPayload\"/>\n" \
	"      </wsdl:message>\n" \
	"      <wsdl:message name=\"responseLicenseTransferResponse\">\n" \
	"        <wsdl:part name=\"responseLicenseTransferReturn\" element=\"lt:LicenseTransferResponsePayload\"/>\n" \
	"      </wsdl:message>\n" \
	"      <wsdl:portType name=\"license-transfer\">\n" \
	"        <wsdl:operation name=\"invokeLicenseTransfer\">\n" \
	"          <wsdl:input name=\"requestLicenseTransferRequest\" message=\"requestLicenseTransferRequest\"/>\n" \
	"          <wsdl:output name=\"responseLicenseTransferResponse\" message=\"responseLicenseTransferResponse\"/>\n" \
	"        </wsdl:operation>\n" \
	"      </wsdl:portType>\n" \
	"      <wsdl:binding name=\"LicenseTransferSoapBinding\" type=\"license-transfer\">\n" \
	"        <wsdlsoap:binding style=\"document\" transport=\"http://schemas.xmlsoap.org/soap/http\"/>\n" \
	"        <wsdl:operation name=\"invokeLicenseTransfer\">\n" \
	"          <wsdlsoap:operation\n" \
	"            soapAction=\"urn:marlin:core:1-1:nemo:services:license-transfer#invokeLicenseTransfer\"/>\n" \
	"          <wsdl:input>\n" \
	"            <wsdlsoap:body use=\"literal\"/>\n" \
	"          </wsdl:input>\n" \
	"          <wsdl:output>\n" \
	"            <wsdlsoap:body use=\"literal\"/>\n" \
	"          </wsdl:output>\n" \
	"        </wsdl:operation>\n" \
	"      </wsdl:binding>\n" \
	"      <wsdl:service name=\"LicenseTransferService\">\n" \
	"        <wsdl:port name=\"LicenseTransfer\" binding=\"LicenseTransferSoapBinding\">\n" \
	"          <wsdlsoap:address location=\"http://source/services/LicenseTransferService\"/>\n" \
	"        </wsdl:port>\n" \
	"      </wsdl:service>\n" \
	"    </wsdl:definitions>\n" \
	"    <PolicyURI operationName=\"invokeLicenseTransfer\">urn:marlin:core:1.3:nemo:services:license-transfer:policy:0</PolicyURI>\n" \
	"  </LtpServiceConfig>\n" \
	"</BroadbandServiceConfig>"

/*
 * The BroadbandServiceConfig of a license transfer (LTP) service: its node's id, the
 * certificate paths of its request encryption and response signing keys, and its role.
 */
int MlnImpl_81035018(int encryptionCount, MlnBuffer **encryptionCertificates, int signingCount,
		     MlnBuffer **signingCertificates, const MlnBuffer *role,
		     const MlnBuffer *nodeId, char **out)
{
	char *node = NULL;
	char *encryption = NULL;
	char *signing = NULL;
	char *roleReference = NULL;
	char *config = NULL;
	unsigned int size;
	int ret;

	if (signingCertificates == NULL || encryptionCertificates == NULL) {
		ret = 4;
		goto out;
	}
	if (nodeId == NULL || role == NULL || out == NULL) {
		ret = 4;
		goto out;
	}
	ret = MlnStringJoin("<nemoc:NodeId>", nodeId, "</nemoc:NodeId>", &node);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81034D38("request-encryptionKey", encryptionCount, encryptionCertificates,
			       &encryption);
	if (ret != 0)
		goto out;
	ret = MlnImpl_81034D38("response-signingKey", signingCount, signingCertificates,
			       &signing);
	if (ret != 0)
		goto out;
	ret = MlnStringJoin("<wsse:SecurityTokenReference nemosec:TargetUsage=\""
			    "http://nemo.intertrust.com/2004/attribute/role\"><wsse:Embedded>",
			    role,
			    "</wsse:Embedded></wsse:SecurityTokenReference>", &roleReference);
	if (ret != 0)
		goto out;
	size = MlnStrlen(node) + MlnStrlen(encryption) + sizeof(MLN_LTP_CONFIG_HEAD) - 1 +
	       sizeof(MLN_LTP_CONFIG_TAIL) - 1 + 1 + MlnStrlen(signing) + MlnStrlen(roleReference);
	config = MlnAllocate(MLN_HEAP_SECURE, size);
	if (config == NULL) {
		ret = 2;
		goto out;
	}
	MlnMemset(config, 0, size);
	MlnStrncat(config, MLN_LTP_CONFIG_HEAD, sizeof(MLN_LTP_CONFIG_HEAD) - 1);
	MlnStrncat(config, node, MlnStrlen(node));
	MlnStrncat(config, encryption, MlnStrlen(encryption));
	MlnStrncat(config, signing, MlnStrlen(signing));
	MlnStrncat(config, roleReference, MlnStrlen(roleReference));
	MlnStrncat(config, MLN_LTP_CONFIG_TAIL, sizeof(MLN_LTP_CONFIG_TAIL) - 1);
	*out = config;
	config = NULL;
out:
	MlnFree(MLN_HEAP_SECURE, node);
	MlnFree(MLN_HEAP_SECURE, encryption);
	MlnFree(MLN_HEAP_SECURE, signing);
	MlnFree(MLN_HEAP_SECURE, roleReference);
	MlnFree(MLN_HEAP_SECURE, config);
	return ret;
}

/* the action token of a license transfer: its configuration and one LtpType action */
#define MLN_LTP_ACTION_TOKEN \
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
	"      <ActionToken xmlns=\"urn:marlin:broadband:1-2:nemo:services:action-token\"" \
	" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"            " \
	" xmlns:ltpa=\"urn:marlin:core:1-3:services:schemas:ltp-service:action-token\"      " \
	"       xsi:schemaLocation=\"urn:marlin:broadband:1-2:nemo:services:action-token " \
	" bb-services-action.xsd                                 " \
	" urn:marlin:core:1-3:services:schemas:ltp-service:action-token" \
	" ltp-services-action.xsd\">" \
	"       <ConfigurationInfo" \
	" broadbandServiceId=\"urn:marlin:organization:sony:ltp-service\"" \
	" configVersion=\"0\">" \
	"           <ResourceLocation>" \
	"http://source/ConfigurationToken.xml</ResourceLocation>" \
	"       </ConfigurationInfo>" \
	"                                                    <Action" \
	" xsi:type=\"ltpa:LtpType\"/>" \
	"                                 </ActionToken>"

/* a copy of the license transfer's action token */
int MlnImpl_810353A8(char **text)
{
	char *token = NULL;
	int ret;

	if (text == NULL) {
		ret = 4;
		goto out;
	}
	token = MlnStrdup(MLN_HEAP_SECURE, MLN_LTP_ACTION_TOKEN);
	if (token == NULL) {
		ret = 2;
		goto out;
	}
	*text = token;
	token = NULL;
	ret = 0;
out:
	MlnFree(MLN_HEAP_SECURE, token);
	return ret;
}

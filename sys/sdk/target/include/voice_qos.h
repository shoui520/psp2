/* SCE CONFIDENTIAL
 * PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 * Copyright (C) 2010 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef _SCE_VOICE_QOS_H
#define _SCE_VOICE_QOS_H

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

///
/// \mainpage libvoiceQoS
///
///	libvoiceQoS provides a C function API for implementing voice chat with Quality of Service functionality.
///	Given a simple graph of local and remote endpoints, and the connections between them, the library handles
///	all input and output i/o, all encoding and decoding, all packetizing of voice data, all adjustments of bandwidth and latency,
///	and handling of out-of-order and lost packets.
///
///	The client of the library is responsible for handling all network connection setup and for transmitting
///	and receiving all voice packets.
///

//
// Type definitions
//
typedef int	SceVoiceQoSError;			///< Error code
typedef int	SceVoiceQoSLocalId;			///< Local endpoint identifier
typedef int	SceVoiceQoSRemoteId;		///< Remote endpoint identifier
typedef int	SceVoiceQoSConnectionId;	///< Connection identifier

//
// Constants
//
#define SCE_VOICE_QOS_INVALID_LOCAL_ID			(-1)	///< Invalid local ID.
#define SCE_VOICE_QOS_INVALID_REMOTE_ID			(-1)	///< Invalid remote ID.
#define SCE_VOICE_QOS_INVALID_CONNECTION_ID		(-1)	///< Invalid connection ID.

/// @brief
/// Attribute identifiers
typedef enum SceVoiceQoSAttributeId
{
	SCE_VOICE_QOS_ATTR_MIC_VOLUME,				///< Local endpoint attribute for microphone volume. Type <c>float</c> with value from 0 and up.
	SCE_VOICE_QOS_ATTR_MIC_MUTE,				///< Local endpoint attribute for microphone mute. Type <c>uint16_t</c> with true to enable mute, false (0) to disable mute.
	SCE_VOICE_QOS_ATTR_SPEAKER_VOLUME,			///< Local endpoint attribute for speaker volume. Type <c>float</c> with value from 0 and up.
	SCE_VOICE_QOS_ATTR_SPEAKER_MUTE,			///< Local endpoint attribute for speaker mute. Type <c>uint16_t</c> with true to enable mute, false (0) to disable mute.
	SCE_VOICE_QOS_ATTR_DESIRED_OUT_BIT_RATE,	///< Connection attribute out-going bit rate desired by a game. Type <c>int</c> with value from 0 and up. For better voice quality, set this value to a number larger than 3850.
	SCE_VOICE_QOS_ATTR_MIC_ACTIVE				///< Local endpoint attribute for determining whether the microphone is active. This is a get-only attribute. Type <c>uint32_t</c> with 1 indicating that the microphone is active and 0 indicating that it is not.
} SceVoiceQoSAttributeId;

/// @brief
/// Status attribute identifier
typedef enum SceVoiceQoSStatusId
{
	SCE_VOICE_QOS_IN_BITRATE,					///< Current QoS (Quality of Service) voice-in codec rate per connection
	SCE_VOICE_QOS_OUT_BITRATE,					///< Current QoS voice-out codec rate per connection
	SCE_VOICE_QOS_OUT_READ_BITRATE,				///< Current QoS voice-out read bit rate by a game per connection
	SCE_VOICE_QOS_IN_FRAME_RECEIVED_RATIO,		///< Ratio of frames received to total frames per connection, as a percentage (float).
	SCE_VOICE_QOS_HEARTBEAT_FLAG				///< Whether or not there is a heartbeat between the connecting points.
} SceVoiceQoSStatusId;

// Limits
#define SCE_VOICE_QOS_MAX_LOCAL_ENDPOINTS		1	///< Maximum number of local endpoints when running on the PlayStation(R)Vita platform.
#define SCE_VOICE_QOS_MAX_REMOTE_ENDPOINTS		7	///< Maximum number of remote endpoints when running on the PlayStation(R)Vita platform. See "Resource Limitations" in the <i>libvoiceQoS Overview</i>.
#define SCE_VOICE_QOS_MAX_CONNECTIONS			7	///< Maximum number of connections when running on the PlayStation(R)Vita platform. See "Resource Limitations" in the <i>libvoiceQoS Overview</i>.

//
// Function declarations
//

/// @brief
/// Initializes libvoiceQoS.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED libvoiceQoS has already been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_SYSTEM libvoiceQoS has system error.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSInit(void);

/// @brief
/// Terminates libvoiceQoS.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSEnd(void);

/// @brief
/// Creates a local endpoint.
///
/// @param pLocalId			[out]	The returned local endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_RESOURCE_USED libvoiceQoS reached its resource limit.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSCreateLocalEndpoint(SceVoiceQoSLocalId* pLocalId );

/// @brief
/// Deletes a local endpoint.
///
/// @param localId			[in]	The local endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSDeleteLocalEndpoint(SceVoiceQoSLocalId localId );

/// @brief
/// Creates a remote endpoint.
///
/// @param pRemoteId		[out]	The returned remote endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_RESOURCE_USED libvoiceQoS reached its resource limit.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSCreateRemoteEndpoint(SceVoiceQoSRemoteId* pRemoteId );

/// @brief
/// Deletes a remote endpoint.
///
/// @param remoteId			[in]	The remote endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSDeleteRemoteEndpoint(SceVoiceQoSRemoteId remoteId );

/// @brief
/// Creates a connection between a local and remote endpoint.
///
/// Only one connection may be made to each remote endpoint.
/// Multiple connections may be made to each local endpoint.
///
/// @param pConnectionId	[out]	The returned connection identifier.
/// @param localId			[in]	The local endpoint identifier.
/// @param remoteId			[in]	The remote endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_INTERNAL_UNRECOVERABLE libvoiceQoS encountered an unrecoverable error.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_RESOURCE_USED libvoiceQoS reached its resource limit.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSConnect(SceVoiceQoSConnectionId* pConnectionId, SceVoiceQoSLocalId localId, SceVoiceQoSRemoteId remoteId );

/// @brief
/// Deletes a connection between a local and remote endpoint.
///
/// @param connectionId		[in]	The connection identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSDisconnect(SceVoiceQoSConnectionId connectionId);

/// @brief
/// Retrieves the local endpoint of the connection.
///
/// @param connectionId		[in]	The connection identifier.
/// @param pLocalId			[out]	The returned local endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSGetLocalEndpoint(SceVoiceQoSConnectionId connectionId, SceVoiceQoSLocalId* pLocalId);

/// @brief
/// Retrieves the remote endpoint of the connection.
///
/// @param connectionId		[in]	The connection identifier.
/// @param pRemoteId		[out]	The returned remote endpoint identifier.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSGetRemoteEndpoint(SceVoiceQoSConnectionId connectionId, SceVoiceQoSRemoteId* pRemoteId);

/// @brief
/// Sets an attribute of the local endpoint.
///
/// @param localId			[in]	The local endpoint identifier.
/// @param attributeId		[in]	The attribute identifier.
/// @param pAttributeValue	[in]	The attribute value data.
/// @param attributeSize	[in]	The size of the attribute value data.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSSetLocalEndpointAttribute(SceVoiceQoSLocalId localId, SceVoiceQoSAttributeId attributeId, const void* pAttributeValue, int attributeSize);

/// @brief
/// Retrieves an attribute of the local endpoint.
///
/// @param localId			[in]	The local endpoint identifier.
/// @param attributeId		[in]	The attribute identifier.
/// @param pAttributeValue	[out]	The returned attribute value data.
/// @param attributeSize	[in]	The size of the attribute value data.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSGetLocalEndpointAttribute(SceVoiceQoSLocalId localId, SceVoiceQoSAttributeId attributeId, void* pAttributeValue, int attributeSize);

/// @brief
/// Sets an attribute of the connection.
///
/// @param connectionId		[in]	The connection identifier.
/// @param attributeId		[in]	The attribute identifier.
/// @param pAttributeValue	[in]	The attribute value data.
/// @param attributeSize	[in]	The size of the attribute value data.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSSetConnectionAttribute(SceVoiceQoSConnectionId connectionId, SceVoiceQoSAttributeId attributeId, const void* pAttributeValue, int attributeSize);

/// @brief
/// Retrieves an attribute of the connection.
///
/// @param connectionId		[in]	The connection identifier.
/// @param attributeId		[in]	The attribute identifier.
/// @param pAttributeValue	[out]	The returned attribute value data.
/// @param attributeSize	[in]	The expected size of the returned attribute value data.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSGetConnectionAttribute(SceVoiceQoSConnectionId connectionId, SceVoiceQoSAttributeId attributeId, void* pAttributeValue, int attributeSize);

/// @brief
/// Retrieves a status value for the connection.
///
/// @param connectionId		[in]	The connection identifier.
/// @param statusId			[in]	The status identifier.
/// @param pStatusValue		[out]	The returned status value data.
/// @param statusSize		[in]	The expected size of the returned status value data.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
SceVoiceQoSError	sceVoiceQoSGetStatus(SceVoiceQoSConnectionId connectionId, SceVoiceQoSStatusId statusId, void* pStatusValue, int statusSize);

/// @brief
/// Delivers a data packet received from a remote endpoint.
///
/// @param connectionId		[in]		The connection identifier.
/// @param pData			[in]		Data received from the remote endpoint.
/// @param pSize			[in/out]	The size of the data received from the remote endpoint. Returns the size written.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_PACKET_INVALID libvoiceQoS received an invalid packet to write.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSWritePacket(SceVoiceQoSConnectionId connectionId, const void* pData, unsigned int* pSize);

/// @brief
/// Retrieves a data packet to send to a remote endpoint.
///
/// @param connectionId		[in]		The connection identifier.
/// @param pData			[in]		Data to be sent from the local endpoint.
/// @param pSize			[in/out]	The size of the data to be sent from the local endpoint. Returns the size read.
///
/// @retval SCE_OK	Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT libvoiceQoS has not been initialized.
/// @retval SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID libvoiceQoS received an invalid argument.
/// @retval SCE_VOICE_ERROR_LIBVOICE_XXX  Related errors from libvoice.
SceVoiceQoSError	sceVoiceQoSReadPacket(SceVoiceQoSConnectionId connectionId, void* pData, unsigned int* pSize);

//-----------------------------------------------------------------------------
/* error code */
/*
    SCE_ERROR_FACILITY_VOICE            0x04e
    libvoice                            0x804e_0801 - 0x804e_08ff
    libvoiceqos                         0x804e_0901 - 0x804e_09ff
 */
/// libvoiceQoS has not been initialized.
#define SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT				(0x804e0901)
/// libvoiceQoS received an invalid argument.
#define SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID		(0x804e0902)
/// libvoiceQoS received an invalid packet.
#define SCE_VOICE_ERROR_LIBVOICEQOS_PACKET_INVALID			(0x804e0903)
/// libvoiceQoS encountered an unrecoverable internal error.
#define SCE_VOICE_ERROR_LIBVOICEQOS_INTERNAL_UNRECOVERABLE	(0x804e0904)
/// libvoiceQoS has already been initialized.
#define SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED				(0x804e0905)
/// libvoiceQoS has system error.
#define SCE_VOICE_ERROR_LIBVOICEQOS_SYSTEM					(0x804e0906)
/// libvoiceQoS resource limit error.
#define SCE_VOICE_ERROR_LIBVOICEQOS_RESOURCE_USED			(0x804e0907)



//-----------------------------------------------------------------------------

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif // _SCE_VOICE_QOS_H

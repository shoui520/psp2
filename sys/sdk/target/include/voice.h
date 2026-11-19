/* SCE CONFIDENTIAL
 * PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 * Copyright (C) 2011 Sony Computer Entertainment Inc.
 * All Rights Reserved.
 */

#ifndef _SCE_VOICE_H
#define _SCE_VOICE_H

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

#include "voice_types.h"

/// Initializes Voice Service.
/// <c>sceVoiceInit()</c> does the following:
/// - Initializes the Voice Service.
/// - Allocates resources for the Voice Service.
///
/// There are two methods to get Voice System status: One is via a Voice Event, where
/// the application registers a callback <c>onEvent</c> for receiving voice events.
/// The other method is via polling, where the application calls <c>sceVoiceGetPortInfo()</c> to get the port status
/// (see <c>SceVoicePortState</c>).
///
/// @param pArg         [in] Initialization parameters
/// @param version      [in] Version for future extension
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_INITIALIZED The libvoice library has already been initialized.
/// @retval SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT Insufficient resources.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoiceInit(SceVoiceInitParam *pArg, SceVoiceVersion version);

/// Terminates Voice Service.
/// <c>sceVoiceEnd()</c> does the following:
/// - Frees resources.
/// - Exits Voice Service.
///
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoiceEnd(void);

/// Starts the Voice Service with user provided memory.
/// The application provides a memory container and calls <c>sceVoiceStart()</c> to request the start
/// of the Voice Service. Changes the port state from IDLE to READY if the topology validation is
/// successful.
/// @param pArg         [in] System Voice Service start parameters.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_CONTAINER_INVALID Memory container error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_GENERAL Error from non-voice functions.
int sceVoiceStart(SceVoiceStartParam *pArg);

/// Stops the Voice Service.
/// The Voice Service releases memory and detaches.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_GENERAL Error from non-voice functions.
int sceVoiceStop(void);

///
/// Flushes the port's internal buffer.
/// @param portId  [in] The port ID.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
int sceVoiceResetPort(uint32_t portId);

///
/// Creates the inbound input port (IPort) and the outbound output port (OPort).
///
/// @param portId  [out] The port ID.
/// @param pArg    [in] Port Parameters.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT Insufficient resources.
/// @retval SCE_VOICE_ERROR_GENERAL Error from non-voice functions.
int sceVoiceCreatePort(uint32_t *portId, const SceVoicePortParam *pArg);

///
/// Updates the specified port with parameters specified to the <c>SceVoicePortParam</c> structure.
/// <c><i>portType</i></c> cannot be updated. There is no effect in updating
/// device <c><i>playerId</i></c> and pcmaudio <c><i>bufSize</i></c>.
/// @param portId   [in] The port ID.
/// @param pArg     [in] Port parameters.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_GENERAL Error from non-voice functions.
int sceVoiceUpdatePort(uint32_t portId, const SceVoicePortParam *pArg);

///
/// Connects the specified input port (IPort) to the specified output port (OPort).
/// To avoid recursive connections, the reverse connection
/// is not allowed. That is, connecting OPort to IPort is not allowed.
/// @param ips  [in] The input port ID.
/// @param ops  [in] The output port ID.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT Insufficient resources.
/// @see sceVoiceDisconnectIPortFromOPort
int sceVoiceConnectIPortToOPort(uint32_t ips, uint32_t ops);

///
/// Disconnects the specified input port (IPort) from the specified output port (OPort).
/// @param ips  [in] The input port ID.
/// @param ops  [in] The output port ID.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @see sceVoiceConnectIPortToOPort
int sceVoiceDisconnectIPortFromOPort(uint32_t ips, uint32_t ops);

///
/// Deletes the specified port.
/// It releases the port memory and binding. If the port is a microphone or a speaker device, it
/// also deletes the device.
/// @param portId   [in] The port ID.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_GENERAL Error from non-voice functions.
int sceVoiceDeletePort(uint32_t portId);

///
/// Writes data to the specified input port's (IPort) input buffer.
/// @param ips          [in] The input port ID.
/// @param data         [in] A constant buffer pointer to hold the write data.
/// @param size     [in/out] For input, the size of the data to write. For
///                          output, the actual number of bytes written.
/// @param frameGaps [in] The number of frames dropped between voice data writings. Values include:
/// - 0: For a continuous stream with no dropped frames.
/// - Positive value (+X): For X dropped forward frames.
///   For example, if the current top frame index were 5, the next expected continuous frame index
///   would be 6. If instead frame index 8 arrives, <c><i>frameGaps</i></c> is the difference between
///   the received frame index (8) and the expected continuous frame index (6): 8 - 6 = 2. The next
///   expected frame index is now 9.
/// - Negative value (-X): For X dropped preceding frames. For example, if the current top frame index were 5,
///   the next expected continuous frame index would be 6. If instead frame index 2 arrives,
///   <c><i>frameGaps</i></c> is the difference between the received frame index (2) and the
///   expected frame index (6): 2 - 6 = -4. The next expected frame index is still 6.
///
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_SERVICE_DETACHED System Voice Service is detached.
int sceVoiceWriteToIPort(uint32_t ips, const void *data, uint32_t *size, int16_t frameGaps);


///
/// Reads data from the specified output port's (OPort) output buffer.
/// @param ops              [in] The output port ID.
/// @param data            [out] The buffer pointer to hold the read data.
/// @param size         [in/out] For input, the number of requested bytes.
///                              For output, the actual number of  bytes read.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_SERVICE_DETACHED System Voice Service is detached.
int sceVoiceReadFromOPort(uint32_t ops, void *data, uint32_t *size);

///
/// Enables or disables mute at all input ports (IPorts) and output ports (OPorts). This does not
/// change the <c>SceVoicePortParam.<i>bMute</i></c>  flag setting for each port.  When a port is muted, its data is still being processed
/// but with an effective volume of 0.
/// @param bMuted   [in] If true, enables mute for all the IPorts/OPorts, no audio is generated.
///                 If false, the audio generation depends on the <c>SceVoicePortParam.<i>bMute</i></c> flag setting for each port.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @see SceVoicePortParam
int sceVoiceSetMuteFlagAll(uint16_t bMuted);

///
/// Enables or disables mute for the specified port. When a port is muted, its data is still being processed but with an effective
/// volume of 0.
/// @param portId   [in] The port ID.
/// @param bMuted   [in] If true,  mute is enabled for the port, no audio is generated.
///                     If false, mute is disabled for the port, audio is generated.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @see sceVoiceGetMuteFlag, sceVoiceSetMuteFlagAll
int sceVoiceSetMuteFlag(uint32_t portId, uint16_t bMuted);

///
/// Gets the mute flag for the specified port.
/// @param portId   [in] The port ID.
/// @param bMuted  [out] The address containing the <c><i>bMute</i></c> flag value
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @see sceVoiceSetMuteFlag, sceVoiceSetMuteFlagAll
int sceVoiceGetMuteFlag(uint32_t portId, uint16_t *bMuted);

///
/// Sets the volume for the specified port
/// @param portId   [in] The port ID.
/// @param volume   [in] The value of volume. A value of 0 silences the port.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @see sceVoiceGetVolume
int sceVoiceSetVolume(uint32_t portId, float volume);

///
/// Gets the volume for the specified port
/// @param[in] portId   [in] The port ID.
/// @param[out] volume  [out] The address containing the value of the volume.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @see sceVoiceSetVolume
int sceVoiceGetVolume(uint32_t portId, float *volume);

///
/// Sets the specific bit rate for the specified port.
/// @param portId   [in] The port ID.
/// @param bitrate  [in] The bit rate, provided by the enumeration.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
int sceVoiceSetBitRate(uint32_t portId, SceVoiceBitRate bitrate);

///
/// Gets the actual bit-rate value for the specified port
/// @param portId       [in] The port ID.
/// @param bitrate     [out] The actual bit-rate value (not the enumeration).
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
int sceVoiceGetBitRate(uint32_t portId, uint32_t *bitrate);

///
/// Gets basic port information for the specified port.
/// @param portId       [in] The port ID.
/// @param pInfo    [in/out] The pointer to the basic port information. The application must allocate
///                          the memory for <c>pInfo->pEdge</c> if it wants the array of the port's
///                          connection edge information returned. Voice Service goes
///                          through the internal binding to get the edge connection information.
///                          This is a time-consuming process.
///                          The application should pass <c>pInfo->pEdge</c> as <c>NULL</c>
///                          if it  does not need to know the connection edges.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_SERVICE_DETACHED System Voice Service is detached.
int sceVoiceGetPortInfo(uint32_t portId, SceVoiceBasePortInfo *pInfo);

///
/// Pauses streaming of the specified port.
/// @param portId  [in] The port ID.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
int sceVoicePausePort(uint32_t portId);

///
/// Resumes streaming of the specified port.
/// @param portId  [in] The port ID.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
int sceVoiceResumePort(uint32_t portId);

///
/// Pauses streaming for all ports.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoicePausePortAll(void);

///
/// Resumes streaming for all ports.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoiceResumePortAll(void);

/// Get the resource information
/// @param pInfo    [in/out] The pointer to the voice resource information.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
int sceVoiceGetResourceInfo(SceVoiceResourceInfo *pInfo);

///
/// Sets a port's attribute to a specified value.
/// See <c>SceVoicePortAttr()</c> for a description of the attributes for which the application can
/// set values.
/// @param portId    [in] The port ID.
/// @param attr      [in] An enumeration value that specifies the port attribute
///                  for which to set a value.
/// @param attrValue [in] The new attribute value for the port.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoiceSetPortAttr(uint32_t portId, SceVoicePortAttr attr, void *attrValue);

///
/// Gets the user-set values of the specified port attribute for the specified port.
/// @param portId    [in] The port ID.
/// @param attr      [in] An enumeration value that specifies the port attribute for which to
///                     get the value.
/// @param attrValue [out] The address of the actual user-set value.
/// @retval SCE_OK Successful completion.
/// @retval SCE_VOICE_ERROR_LIBVOICE_NOT_INIT The libvoice library has not been initialized.
/// @retval SCE_VOICE_ERROR_ARGUMENT_INVALID Invalid argument.
/// @retval SCE_VOICE_ERROR_PORT_INVALID Invalid port.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoiceGetPortAttr(uint32_t portId, SceVoicePortAttr attr, void *attrValue);


///
/// Checks the created port connections to see if their topology is valid for streaming.
/// This function is provided as a helper function for developers to verify the topology layout during development.
///
/// The following cases are checked in a topology:
/// \li Every port in the topology should be connected to one or more ports. No single unconnected port is allowed in the topology.
/// \li When a \ref SCEVOICE_PORTTYPE_IN_DEVICE port and a \ref SCEVOICE_PORTTYPE_IN_PCMAUDIO port both attempt to encode the data, both ports
///    should be connected to a same #SCEVOICE_PORTTYPE_OUT_VOICE port.
/// \li When several \ref SCEVOICE_PORTTYPE_OUT_VOICE ports are created, all related voice out parameters (<c><i>bMute</i></c>,
///    <c><i>volume</i></c>, <c><i>bitrate</i></c>) must be all the same because there is only one encoder available inside libvoice.
/// @retval SCE_OK Topology is valid.
/// @retval SCE_VOICE_ERROR_TOPOLOGY Topology error.
int sceVoiceCheckTopology(void);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif	// _SCE_VOICE_H

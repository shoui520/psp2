/*
 * MotionHif: the host interface: the Vita's commands and the reports back.
 *
 * Skeleton. The methods are those the debug messages name; their signatures and
 * bodies are not written yet.
 */

#include "MotionHif.h"

void MotionHif::CmdReadVersion()
{
}

void MotionHif::CmdWriteCalib()
{
}

void MotionHif::CmdReadCalib()
{
}

void MotionHif::CmdReadCalibWritingStatus()
{
}

void MotionHif::CmdSetSampleType()
{
}

void MotionHif::CmdSwitchFrameSize()
{
}

void MotionHif::ReportResultCode()
{
}

void MotionHif::ReportDeviceInfo()
{
}

#if MOTION_IMAGE >= 1
void MotionHif::CmdSelectMeasType()
{
}
#endif

#if MOTION_IMAGE >= 1
void MotionHif::CmdReadDeviceInfo()
{
}
#endif

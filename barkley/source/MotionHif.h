/*
 * MotionHif: the host interface: the Vita's commands and the reports back.
 */
#ifndef MOTIONHIF_H
#define MOTIONHIF_H

#include "motion.h"

class MotionHif {
public:
	void CmdReadVersion();
	void CmdWriteCalib();
	void CmdReadCalib();
	void CmdReadCalibWritingStatus();
	void CmdSetSampleType();
	void CmdSwitchFrameSize();
	void ReportResultCode();
	void ReportDeviceInfo();
#if MOTION_IMAGE >= 1
	void CmdSelectMeasType();
#endif
#if MOTION_IMAGE >= 1
	void CmdReadDeviceInfo();
#endif
};

#endif

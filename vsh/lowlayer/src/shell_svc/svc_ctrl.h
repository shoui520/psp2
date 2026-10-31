#ifndef SVC_CTRL_H
#define SVC_CTRL_H

/* Our names for SceSvcCtrl's states and error codes (0x801021xx). */
#define SVC_CTRL_STATE_IDLE		0
#define SVC_CTRL_STATE_ACTIVE_1		1
#define SVC_CTRL_STATE_ACTIVE_2		2

#define SVC_CTRL_ERROR_INVALID_STATE	-2146426624	/* 0x80102100 */
#define SVC_CTRL_ERROR_ACTIVE_2		-2146426608	/* 0x80102110 */
#define SVC_CTRL_ERROR_ACTIVE_1		-2146426607	/* 0x80102111 */
#define SVC_CTRL_ERROR_UNKNOWN_STATE	-2146426369	/* 0x801021FF */

extern "C" {
int SceSvcCtrl_077F8AF1(void);
int SceSvcCtrl_8BEB8B7C(void);
int SceSvcCtrl_683998F2(void);
int SceSvcCtrl_B21BE499(void);
}

#endif

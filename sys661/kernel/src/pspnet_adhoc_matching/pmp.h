#ifndef PMP_H
#define PMP_H

/* Error codes */
#define MATCHING_ERR_STACK_SMALL        ((int)0x80410005)
#define MATCHING_ERR_PDP_WOULDBLOCK     ((int)0x80410709)
#define MATCHING_ERR_INVALID_MODE       ((int)0x80410801)
#define MATCHING_ERR_INVALID_MAXNUM     ((int)0x80410803)
#define MATCHING_ERR_RXBUF_TOO_SHORT    ((int)0x80410804)
#define MATCHING_ERR_INVALID_OPTLEN     ((int)0x80410805)
#define MATCHING_ERR_INVALID_ARG        ((int)0x80410806)
#define MATCHING_ERR_INVALID_ID         ((int)0x80410807)
#define MATCHING_ERR_ID_NOT_AVAIL       ((int)0x80410808)
#define MATCHING_ERR_NO_SPACE           ((int)0x80410809)
#define MATCHING_ERR_IS_RUNNING         ((int)0x8041080A)
#define MATCHING_ERR_NOT_RUNNING        ((int)0x8041080B)
#define MATCHING_ERR_UNKNOWN_TARGET     ((int)0x8041080C)
#define MATCHING_ERR_TARGET_NOT_READY   ((int)0x8041080D)
#define MATCHING_ERR_EXCEED_MAXNUM      ((int)0x8041080E)
#define MATCHING_ERR_REQUEST_IN_PROGRESS ((int)0x8041080F)
#define MATCHING_ERR_ALREADY_ESTABLISHED ((int)0x80410810)
#define MATCHING_ERR_BUSY               ((int)0x80410811)
#define MATCHING_ERR_ALREADY_INITIALIZED ((int)0x80410812)
#define MATCHING_ERR_NOT_INITIALIZED    ((int)0x80410813)
#define MATCHING_ERR_PORT_IN_USE        ((int)0x80410814)
#define MATCHING_ERR_STACKSIZE_TOO_SHORT ((int)0x80410815)
#define MATCHING_ERR_INVALID_DATALEN    ((int)0x80410816)
#define MATCHING_ERR_NOT_ESTABLISHED    ((int)0x80410817)
#define MATCHING_ERR_DATA_BUSY          ((int)0x80410818)
#define MATCHING_ERR_UNALIGNED          ((int)0x80410819)

/* Modes */
#define MATCHING_MODE_HOST   1
#define MATCHING_MODE_CLIENT 2
#define MATCHING_MODE_PTP    3

/* Peer states */
#define PEER_STATE_IDLE        1
#define PEER_STATE_JOIN_RECV   2
#define PEER_STATE_ACCEPTED    3
#define PEER_STATE_SELECT_SENT 4
#define PEER_STATE_ESTABLISHED 5

/* Data states */
#define DATA_STATE_IDLE    1
#define DATA_STATE_SENDING 2

/* Wire packet types */
#define PKT_HELLO          1
#define PKT_JOIN           2
#define PKT_ACCEPT         3
#define PKT_COMPLETE       4
#define PKT_CANCEL         5
#define PKT_KEEPALIVE      6
#define PKT_KEEPALIVE_ACK  7
#define PKT_BYE            8
#define PKT_JOIN_ACK       9
#define PKT_DATA          10
#define PKT_DATA_ACK      11

/* User callback events */
#define MATCHING_EV_HELLO          1
#define MATCHING_EV_JOIN           2
#define MATCHING_EV_LEFT           3
#define MATCHING_EV_REJECT         4
#define MATCHING_EV_CANCEL         5
#define MATCHING_EV_ACCEPT         6
#define MATCHING_EV_ESTABLISHED    7
#define MATCHING_EV_TIMEOUT        8
#define MATCHING_EV_ERROR          9
#define MATCHING_EV_BYE           10
#define MATCHING_EV_DATA          11
#define MATCHING_EV_DATA_CONFIRM  12
#define MATCHING_EV_DATA_TIMEOUT  13

/* Internal mailbox message types */
#define MBX_MSG_PACKET     1
#define MBX_MSG_CTRL_RETRY 2
#define MBX_MSG_KEEPALIVE  3
#define MBX_MSG_HELLO      4
#define MBX_MSG_DATA_RETRY 5


#endif /* PMP_H */

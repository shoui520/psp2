/*
 * SceNpCommerce2: the calls to the shell's store service. The shell answers through an object with
 * a table of functions whose sixth, at offset 20, takes the operation (0xe0000 and its number), the
 * buffers of the input and of the output and gives the operation's result and its own.
 */

#include "np_commerce2_internal.h"

#define NP_COMMERCE2_IPC_BASE	0xe0000

/* calls an operation of the shell's service: its own result when it fails, the operation's otherwise */
#define NP_COMMERCE2_IPC_CALL(service, op, in, inCount, out, outCount)				\
	do {											\
		int result_ = 0;								\
												\
		ret = (*(service)->vtbl->call)((service), NP_COMMERCE2_IPC_BASE + (op), (in), (inCount),	\
					       &result_, (out), (outCount));			\
		if (ret == 0)									\
			ret = result_;								\
	} while (0)

/* operation 0x0 of the shell service */
int npCommerce2IpcOp00(NpCommerce2Ipc *service)
{
	int ret;

	NP_COMMERCE2_IPC_CALL(service, 0, NULL, 0, NULL, 0);
	return ret;
}

/* operation 0x1 of the shell service */
int npCommerce2IpcOp01(NpCommerce2Ipc *service, void *value)
{
	NpCommerce2IpcOutput out[1];
	int ret;

	out[0].data = value;
	out[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 1, NULL, 0, out, 1);
	return ret;
}

/* operation 0x2 of the shell service */
int npCommerce2IpcOp02(NpCommerce2Ipc *service, int value)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = &value;
	in[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 2, in, 1, NULL, 0);
	return ret;
}

/* operation 0x3 of the shell service */
int npCommerce2IpcOp03(NpCommerce2Ipc *service, int value, void *result)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[1];
	int ret;

	in[0].data = &value;
	in[0].size = 4;
	out[0].data = result;
	out[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 3, in, 1, out, 1);
	return ret;
}

/* operation 0x4 of the shell service */
int npCommerce2IpcOp04(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 4, in, 1, NULL, 0);
	return ret;
}

/* operation 0x5 of the shell service */
int npCommerce2IpcOp05(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 5, in, 1, NULL, 0);
	return ret;
}

/* operation 0x6 of the shell service */
int npCommerce2IpcOp06(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 6, in, 1, NULL, 0);
	return ret;
}

/* operation 0x7 of the shell service */
int npCommerce2IpcOp07(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 7, in, 1, out, 7);
	return ret;
}

/* operation 0x8 of the shell service */
int npCommerce2IpcOp08(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 8, in, 1, out, 7);
	return ret;
}

/* operation 0x9 of the shell service */
int npCommerce2IpcOp09(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 9, in, 1, out, 7);
	return ret;
}

/* operation 0xa of the shell service */
int npCommerce2IpcOp0A(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 24;
	NP_COMMERCE2_IPC_CALL(service, 10, in, 1, NULL, 0);
	return ret;
}

/* operation 0xb of the shell service */
int npCommerce2IpcOp0B(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 11, in, 1, out, 7);
	return ret;
}

/* operation 0xc of the shell service */
int npCommerce2IpcOp0C(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 532;
	NP_COMMERCE2_IPC_CALL(service, 12, in, 1, NULL, 0);
	return ret;
}

/* operation 0xd of the shell service */
int npCommerce2IpcOp0D(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 13, in, 1, out, 7);
	return ret;
}

/* operation 0xe of the shell service */
int npCommerce2IpcOp0E(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 532;
	NP_COMMERCE2_IPC_CALL(service, 14, in, 1, NULL, 0);
	return ret;
}

/* operation 0xf of the shell service */
int npCommerce2IpcOp0F(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 15, in, 1, out, 7);
	return ret;
}

/* operation 0x10 of the shell service */
int npCommerce2IpcOp10(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 532;
	NP_COMMERCE2_IPC_CALL(service, 16, in, 1, NULL, 0);
	return ret;
}

/* operation 0x11 of the shell service */
int npCommerce2IpcOp11(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 17, in, 1, out, 7);
	return ret;
}

/* operation 0x12 of the shell service */
int npCommerce2IpcOp12(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 28;
	NP_COMMERCE2_IPC_CALL(service, 18, in, 1, NULL, 0);
	return ret;
}

/* operation 0x13 of the shell service */
int npCommerce2IpcOp13(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 19, in, 1, out, 7);
	return ret;
}

/* operation 0x14 of the shell service */
int npCommerce2IpcOp14(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 524;
	NP_COMMERCE2_IPC_CALL(service, 20, in, 1, NULL, 0);
	return ret;
}

/* operation 0x15 of the shell service */
int npCommerce2IpcOp15(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 21, in, 1, out, 7);
	return ret;
}

/* operation 0x16 of the shell service */
int npCommerce2IpcOp16(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 276;
	NP_COMMERCE2_IPC_CALL(service, 22, in, 1, NULL, 0);
	return ret;
}

/* operation 0x17 of the shell service */
int npCommerce2IpcOp17(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 23, in, 1, out, 7);
	return ret;
}

/* operation 0x18 of the shell service */
int npCommerce2IpcOp18(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 264;
	NP_COMMERCE2_IPC_CALL(service, 24, in, 1, NULL, 0);
	return ret;
}

/* operation 0x19 of the shell service */
int npCommerce2IpcOp19(NpCommerce2Ipc *service, void *out0, void *out1, void *out2, int in0, void *out3,
		    void *out4, int in1, void *out5, void *out6)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[7];
	int packed[2];
	int ret;

	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 4;
	out[2].data = out2;
	out[2].size = 1024;
	packed[0] = in0;
	out[3].data = out3;
	out[3].size = 4;
	out[4].data = out4;
	out[4].size = 256;
	packed[1] = in1;
	out[5].data = out5;
	out[5].size = 4;
	out[6].data = out6;
	out[6].size = 4;
	in[0].data = packed;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 25, in, 1, out, 7);
	return ret;
}

/* operation 0x1a of the shell service */
int npCommerce2IpcOp1A(NpCommerce2Ipc *service, void *request)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = request;
	in[0].size = 8;
	NP_COMMERCE2_IPC_CALL(service, 26, in, 1, NULL, 0);
	return ret;
}

/* operation 0x1b of the shell service */
int npCommerce2IpcOp1B(NpCommerce2Ipc *service, void *out0, void *out1, int in0, void *out2)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[3];
	int ret;

	in[0].data = &in0;
	in[0].size = 4;
	out[0].data = out0;
	out[0].size = 4;
	out[1].data = out1;
	out[1].size = 1024;
	out[2].data = out2;
	out[2].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 27, in, 1, out, 3);
	return ret;
}

/* operation 0x1c of the shell service */
int npCommerce2IpcOp1C(NpCommerce2Ipc *service, int value)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = &value;
	in[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 28, in, 1, NULL, 0);
	return ret;
}

/* operation 0x1d of the shell service */
int npCommerce2IpcOp1D(NpCommerce2Ipc *service, int value, void *result)
{
	NpCommerce2IpcInput in[1];
	NpCommerce2IpcOutput out[1];
	int ret;

	in[0].data = &value;
	in[0].size = 4;
	out[0].data = result;
	out[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 29, in, 1, out, 1);
	return ret;
}

/* operation 0x1e of the shell service */
int npCommerce2IpcOp1E(NpCommerce2Ipc *service, int value)
{
	NpCommerce2IpcInput in[1];
	int ret;

	in[0].data = &value;
	in[0].size = 4;
	NP_COMMERCE2_IPC_CALL(service, 30, in, 1, NULL, 0);
	return ret;
}

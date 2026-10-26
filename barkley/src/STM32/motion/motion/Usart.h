/*
 * Usart: a USART port, a byte at a time by its interrupts (RXNE, TXE), each transfer a
 * request reported to its caller. In text mode a '\n' sent goes out as "\r\n" and a
 * '\r' received comes in as '\n'.
 */
#ifndef USART_H
#define USART_H

extern "C" {
#include "stm32f10x_lib.h"
}
#include "BitBand.h"
#include "Request.h"

/* a port's registers, interrupt (vector number) and its interrupt enables (CR1's
   TXEIE and RXNEIE, bit-band) */
struct UsartPortInfo {
	USART_TypeDef *regs;
	s8 irq;
	vu32 *txeie;
	vu32 *rxneie;
};

extern UsartPortInfo usartPorts[3];

/* how the bytes go */
enum {
	USART_MODE_TEXT,
	USART_MODE_BINARY,
};

class Usart {
public:
	Usart(s8 port, s8 mode);
	void Init(u8 preemption, u8 sub, u32 baudRate, u16 wordLength, u8 stopBits, u8 parity,
		  u8 hardwareFlowControl);
	void Receive(void *buf, u32 size, Request::Done::Func func, void *arg);
	void Send(void *buf, u32 size, Request::Done::Func func, void *arg);
	u32 Write(const u8 *buf, u32 size);

private:
	typedef void (Usart::*Step)();

	void SetMode(s8 mode);
	void SendText();
	void SendBinary();
	void ReceiveText();
	void ReceiveBinary();
	void OnReceive();
	void OnSend();
	static void IsrCallback(void *arg);
	void Isr();

	void SendByte(u8 c)
	{
		USART_SendData(regs_, c);
	}

	/* a byte out, waited for */
	void PutChar(u8 c)
	{
		SendByte(c);
		while (USART_GetFlagStatus(regs_, USART_FLAG_TC) == RESET)
			;
	}

	u8 ReceiveByte()
	{
		return USART_ReceiveData(regs_);
	}

	void EnableTxIt()
	{
		*txeie_ = 1;
	}

	void DisableTxIt()
	{
		*txeie_ = 0;
	}

	void EnableRxIt()
	{
		*rxneie_ = 1;
	}

	void DisableRxIt()
	{
		*rxneie_ = 0;
	}

	s8 port_;
	USART_TypeDef *regs_;
	vu32 *txeie_;
	vu32 *rxneie_;
	u8 reserved_;
	bool crSent_;		/* text mode: the '\r' of a '\n' sent */
	Request txRequest_;
	Request *tx_;
	Request rxRequest_;
	Request *rx_;
	Step send_;
	Step receive_;
};

#endif

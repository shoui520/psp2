/*
 * Usart: a USART port driven by its interrupts. A transfer sets its request and turns
 * the interrupt on; each RXNE takes a byte in, each TXE puts the next one out, and the
 * last turns the interrupt off and completes the request. The mode's steps (member
 * pointers) move one byte each.
 */

#include "Usart.h"
#include "stm32f10x_it.h"

UsartPortInfo usartPorts[3] = {
	{ USART1, USART1_IRQChannel + 16, BITBAND(&USART1->CR1, 7), BITBAND(&USART1->CR1, 5) },
	{ USART2, USART2_IRQChannel + 16, BITBAND(&USART2->CR1, 7), BITBAND(&USART2->CR1, 5) },
	{ USART3, USART3_IRQChannel + 16, BITBAND(&USART3->CR1, 7), BITBAND(&USART3->CR1, 5) },
};

Usart::Usart(s8 port, s8 mode)
	: port_(port), regs_(usartPorts[port].regs), txeie_(BitBand((u32)&regs_->CR1, 7)),
	  rxneie_(BitBand((u32)&regs_->CR1, 5))
{
	SetMode(mode);
}

void Usart::Init(u8 preemption, u8 sub, u32 baudRate, u16 wordLength, u8 stopBits, u8 parity,
		 u8 hardwareFlowControl)
{
	USART_InitTypeDef init;

	init.USART_BaudRate = baudRate;
	init.USART_WordLength = wordLength;
	init.USART_StopBits = stopBits;
	init.USART_Parity = parity;
	init.USART_HardwareFlowControl = hardwareFlowControl;
	init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(regs_, &init);
	RegisterIrqHandler(usartPorts[port_].irq, IsrCallback, this, preemption, sub);
	USART_Cmd(regs_, ENABLE);
	ReceiveByte();		/* a byte left in DR */
}

void Usart::SetMode(s8 mode)
{
	switch (mode) {
	case USART_MODE_TEXT:
		send_ = &Usart::SendText;
		receive_ = &Usart::ReceiveText;
		crSent_ = false;
		break;
	case USART_MODE_BINARY:
		send_ = &Usart::SendBinary;
		receive_ = &Usart::ReceiveBinary;
		crSent_ = false;
		break;
	}
}

void Usart::Receive(void *buf, u32 size, Request::Done::Func func, void *arg)
{
	rxRequest_.Set(buf, size, func, arg);
	rx_ = &rxRequest_;
	if (size != 0)
		EnableRxIt();
	else
		rx_->Complete(0);
}

void Usart::Send(void *buf, u32 size, Request::Done::Func func, void *arg)
{
	txRequest_.Set(buf, size, func, arg);
	tx_ = &txRequest_;
	if (size != 0) {
		(this->*send_)();
		EnableTxIt();
	} else {
		tx_->Complete(0);
	}
}

/* a '\n' goes out after a '\r', and is passed once both are */
void Usart::SendText()
{
	u8 c = *tx_->cur_;

	if (c == '\n') {
		if (!crSent_) {
			crSent_ = true;
			c = '\r';
		} else {
			crSent_ = false;
			tx_->cur_++;
		}
	} else {
		tx_->cur_++;
	}
	SendByte(c);
}

void Usart::SendBinary()
{
	SendByte(*tx_->cur_++);
}

void Usart::ReceiveText()
{
	u8 c = ReceiveByte();

	if (c == '\r')
		*rx_->cur_++ = '\n';
	else
		*rx_->cur_++ = c;
}

void Usart::ReceiveBinary()
{
	u8 *p = rx_->cur_++;

	*p = ReceiveByte();
}

void Usart::OnReceive()
{
	(this->*receive_)();
	rx_->remaining_--;
	if (rx_->remaining_ == 0) {
		DisableRxIt();
		rx_->Complete(0);
	}
}

/* the byte before sent: the next, or the end (a '\r' put in is not counted) */
void Usart::OnSend()
{
	if (!crSent_)
		tx_->remaining_--;
	if (tx_->remaining_ != 0) {
		(this->*send_)();
	} else {
		DisableTxIt();
		tx_->Complete(0);
	}
}

void Usart::IsrCallback(void *arg)
{
	static_cast<Usart *>(arg)->Isr();
}

void Usart::Isr()
{
	if (USART_GetITStatus(regs_, USART_IT_RXNE) != RESET)
		OnReceive();
	if (USART_GetITStatus(regs_, USART_IT_TXE) != RESET)
		OnSend();
}

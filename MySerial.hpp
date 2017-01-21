#pragma once
#include "common.hpp"
#include "CharFifo.hpp"
#include "wiring_private.h" // pinPeripheral() function

template<int rx_depth=1024>
class MySerial
{
	SERCOM* sercom = nullptr;
	
	CharFifo<rx_depth, true> rx_fifo;

	void waitForDataRegister()
	{
		for (int i=0; i<100; i++) {
			delay(1);
			if (sercom->isDataRegisterEmptyUART()) return;
		}
		assert(!"isDataRegisterEmptyUART stuck at high");
	}

	String name;
	Stream& logger = Serial;
public:
	MySerial(const String& name) : name(name), rx_fifo(name+"_rx")
	{}

	// Open the link
	void beginHandshaked(
		unsigned long baudrate = 9600,//19200
		uint8_t pin_rx = 3,  /*PA09 SERCOM2.1 RX<-GSM_TX*/
		uint8_t pin_tx = 4,  /*PA08 SERCOM2.0 TX->GSM_RX*/
		uint8_t pin_rts = 2, /*PA14 SERCOM2.2 RTS*/
		uint8_t pin_cts = 5, /*PA15 SERCOM2.3 CTS*/
		_EPioType pin_type_rx = PIO_SERCOM_ALT,
		_EPioType pin_type_tx = PIO_SERCOM_ALT,
		_EPioType pin_type_rts = PIO_DIGITAL,
		_EPioType pin_type_cts = PIO_DIGITAL,
		SercomRXPad pad_rx = SERCOM_RX_PAD_1,
		SercomUartTXPad pad_tx = UART_TX_PAD_0,
		SERCOM* sercom = &sercom2
	)
	{
		this->sercom = sercom;
		pinPeripheral(pin_rx, pin_type_rx);
		pinPeripheral(pin_tx, pin_type_tx);
		pinPeripheral(pin_rts, pin_type_rts);
		pinPeripheral(pin_cts, pin_type_cts);
		sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
		sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
		sercom->initPads(pad_tx, pad_rx);
		sercom->enableUART();
		pinMode(pin_cts, INPUT);
		pinMode(pin_rts, OUTPUT);
		digitalWrite(pin_rts, 0);
	}
	
	// Open the link
	void beginNormal(
		unsigned long baudrate = 19200,
		uint8_t pin_rx = 3,  /*PA09 SERCOM2.1 RX<-GSM_TX*/
		uint8_t pin_tx = 4,  /*PA08 SERCOM2.0 TX->GSM_RX*/
		_EPioType pin_type_rx = PIO_SERCOM_ALT,
		_EPioType pin_type_tx = PIO_SERCOM_ALT,
		SercomRXPad pad_rx = SERCOM_RX_PAD_1,
		SercomUartTXPad pad_tx = UART_TX_PAD_0,
		SERCOM* sercom = &sercom2
	)
	{
		this->sercom = sercom;
		pinPeripheral(pin_rx, pin_type_rx);
		pinPeripheral(pin_tx, pin_type_tx);
		sercom->initUART(UART_INT_CLOCK, SAMPLE_RATE_x16, baudrate);
		sercom->initFrame(UART_CHAR_SIZE_8_BITS, LSB_FIRST, SERCOM_NO_PARITY, SERCOM_STOP_BIT_1);
		sercom->initPads(pad_tx, pad_rx);
		sercom->enableUART();
	}
	
	// Handle bytes on the rx
	void irqHandler()
	{
		if (!sercom) return;

		while (sercom->availableDataUART()) {
			char x = sercom->readDataUART();
			rx_fifo.push(x);
		}
		
		if (sercom->isUARTError()) {
			assert(!"uart rx error");
			sercom->acknowledgeUARTError();
			sercom->clearStatusUART();
		}
	}


	events::Channel<>& rxLineChan()
	{
		return rx_fifo.getLineChan();
	}

	bool hasLine() 
	{
		return rx_fifo.hasLine();
	}

	String popLine()
	{
		return rx_fifo.popLine();
	}

	void println(const char* x=nullptr)
	{
		if (x) {
			logger.println(name + "_tx>" + x);
			for (; *x; x++) {
				waitForDataRegister();
				sercom->writeDataUART(*x);
			}
		} else logger.println(name + "_tx>");
		waitForDataRegister();
		sercom->writeDataUART('\n');
	}

	void println(const String& x)
	{
		println(x.c_str());
	}
};

/*
 * File: TestDecaRange.c
 *
 * Contains: Test RADIO frame transmission
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Copyright:	© 1989-2023 by Guy McIlroy
 *  All rights reserved.
 *
 *  Use of this code is subject to the terms of the KoliadaESDK license.
 *  https://docs.koliada.com/KoliadaESDKLicense.pdf
 *
 *  Koliada, LLC - www.koliada.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */
#define __RADIO_CLASS_H	 // block current class/radio.h

#include "Koliada.h"
#include "interface/dw3000.h"

#define USE_RANGING
#define RF_CHANNEL 5 // test using channel 5

// In this test we do basic input/output using the installed radio adapter (if any).
// There are two build configs, one to build a sender (Tx) and one to build a receiver
// (Rx). For a more complex example see: https://docs.koliada.com/kes/examples/TestRadio

////////////////////////////////////////////////////////////////////////////////
//
// For details see: https://docs.koliada.com/kes/examples/TestRadioApi
//
////////////////////////////////////////////////////////////////////////////////

#if USE_INTERFACES
	void initDrivers();
	static RADIO radio;
#else
	// Define a UDP class object
	DefineUdp(radio);
#endif

// local frame buffer (large enough to hold _all_ anticipated frame types)
#define kTxBufSize 64
#define kRxBufSize 64
static byte rxBuf[kRxBufSize];

StaticEvent(rxEvent);
void rxEventHandler(EVENT e, byte *buf, word len)
	{
	// running in application context
	print("%s\n", (char *)buf);
	}

StaticDelegate(rxReady);
static void rxReadyHandler(byte *buf, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is recieved

	// buf[0] = RSSI
	// buf[1] = CORR
	// buf[2] = protocol type

	// At this point the frame is qualified only as having a frame signature
	// that matches the frame signature criteria defined by the kRfSetFrameSig
	// call made to establish the frame signature filter (see below in TEST())
	//
	// This callback is so the frame can be qualified in relation protocol parsing
	// and then forwarded (and possibly queued) to the application for further
	// processing.
	//
	// Since this is running on the interrupt handler we don't want to waste time!
	//
	// For this test we have a very simple 'data layer' protocol and once qualified
	// we simply pass the message on to the application as an event. For simple the
	// processing required here, an event is overkill, but more complex protocols
	// _must_ queue frames and defer processing to the application to facilitate
	// radio throughput.
	//
	// Further note, this system is event driven and without the posting of an
	// event, the _application_ will never know that anything changed!

	//debug(">");
	//dump(buf, len, 1);

	// qualify and pass up to the application
	switch (buf[2])
		{
		// post the rxEvent
		case ':':// protocol id
			// skip the protocol headers
			PostEvent(rxEvent, &rxBuf[3], len - 3);
			break;

		// default is ignored
		default:
			break;
		}

	// Here, we have an overly simple protocol and do no buffering or queuing - any
	// new incoming frame will overwrite the rxBuf before or during the handling
	// of it by the application. A 'real' application will have buffering and/or
	// queuing and buffer/queue protection will be implicitly defined by the protocol.
	//
	// For a more complex example see: https://docs.koliada.com/kes/examples/TestRadio
	}

StaticDelegate(txDoneDelegate);
void txDoneHandler(byte *frame, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is sent
	//debug("<");
	//dump(frame, len, 1);
	}

void abortHandler(int sig)
	{
	// for this test, we simply exit
	// depending on the circumstances, other options are reasonable including
	// but not limited to;
	//    - reset to OTA,
	//    - reset to APP (assuming a 'transient' abort happend),
	//    - other, application specific, recovery
	exit(-42);
	}

#ifdef USE_DW1000
	#define SELECTED_RADIO "DW1000"
#endif
#ifdef USE_DW3000
	#define SELECTED_RADIO "DW3000"
#endif
#ifndef SELECTED_RADIO
	#error You must select a radio for this test!
#endif

void TEST()
	{
	printf(">>%s\n", __func__);
	
	// first, capture the SIGABRT signal
	signal(SIGABRT, abortHandler);

#if USE_INTERFACES
	initDrivers();
	radio = IINTERFACE.Find(SELECTED_RADIO);
#else
	// create a radio endpoint instance
	objectCreate(radio, SELECTED_RADIO);
#endif

#ifdef USE_RANGING
	// set up for single-sided two way ranging
	debug("\nSetting up to RANGE from %s\n\n", typeof(radio)->Name);
	ssInit(radio);
#else
	// set up for two way radio tests
	debug("\nSetting up to Tx/Rx from %s\n\n", typeof(radio)->Name);
#endif
	
	IRADIO.Iocntl(radio, kRadioSetChannel, RF_CHANNEL);

	// set the txDone handler
	// set up the txDone delegate (shows we're transmitting)
	objectCreate(txDoneDelegate, delegateTask(txDoneHandler));
	IRADIO.Iocntl(radio, kRadioAddTxDone, txDoneDelegate);

	// specifically not using Clear Channel Avoidance (CCA) in this test

	// The rx buffer size must be large enough to contain the largest receipt
	// and small enough to maximize the buffer space available to the radio
	int maxFrameSize = IRADIO.Iocntl(radio, kRadioFrameSize);
	//print("Max frame size = %u\n", maxFrameSize);
	assert(kTxBufSize <= maxFrameSize);
	assert(kRxBufSize <= maxFrameSize);

	// create the rxEvent
	objectCreate(rxEvent);
	OnEvent(rxEvent, (HANDLER) rxEventHandler);

	// set the rxReady handler
	// create & set the Rx delegate
	objectCreate(rxReady, delegateTask(rxReadyHandler));
	IRADIO.Iocntl(radio, kRadioAddRxReady, rxReady);

	// set the receive buffer,
	memset(rxBuf, 0xa5, sizeof(rxBuf));
	IRADIO.Iocntl(radio, kRadioSetRxBuffer, rxBuf, sizeof(rxBuf));

	// the frame type,
	//	IRADIO.Iocntl((Interface) radio, kRadioSetFrameSig, FRAME_ID0 << 8 | FRAME_ID1);
	// we do not set frame type - we stay in 'promiscuous mode' to see all frames
	
	// start listening, and
	IRADIO.Iocntl(radio, kRadioEnableRx);

	print("\nhit any key to send, ESC to quit\n");
	do
		{
		int key;
		while ((key = getch()) == -1)	// get a (uart) keypress (no wait)
			EventYield();
		if (key == 0x1b)
			// ESC
			break;

#ifdef USE_RANGING

		// send a (broadcast) range request returning the result
		// to target a specific node we need it's address and this test would not
		// then be 'symetric' across all nodes
		_ssRangeData result;
		ssRangeTo(radio, BCAST_ADDR, &result);
		
		// result can be handler in ssRangTo or here

#else

		// create a tx buffer
		static char buf[kTxBufSize] = "EW:Hello World - 0x00!";

		// replace '00' with a random byte (so we know the frame is changing)
		byte i = strnbuf(kTxBufSize, buf, 19, "%02x", randomByte());

		// pad the frame out to the local maximum (starting after the '\0')
		for (i++; i < kTxBufSize; buf[i++] = randomByte());

#if 0
		print("%s - ", &buf[17]);
#else
		print("%s\n", buf);
#endif

		// raw udp send (no presentation layer protocols)
		IRADIO.Send(radio, (byte *)buf, i);
#endif
		} while (1);

	// we're done
	printf("<<%s\n", __func__);

	// When a KoliadaES program exits, control returns to the kernel and any exit
	// application defined exit delegates are run which should releases the radio
	// as part of their exit processing.
	//
	// Typically an embedded <application> program does NOT exit, however, some
	// programs are designed to exit. They may be configuration programs, or driver
	// installers, test cases and similar.
	//
	// What happens after exit completes is host/developer defined (as part of board
	// configuration. Typically, OTA loader will run.

	// For details see: https://docs.koliada.com/kes/ExitHandling
	}

/*
 * File: TestDecaRx
 *
 * Contains: Test Decawave radio receiving
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Copyright:	© 1989-2020 by Guy McIlroy
 *  All rights reserved.
 *
 *  Use of this code is subject to the terms of the KoliadaESDK license.
 *  https://docs.koliada.com/KoliadaESDKLicense.pdf
 *
 *  Koliada, LLC - www.koliada.com
 *  Support: forum.koliada.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */
#define __RADIO_CLASS_H	 // block current class/radio.h

#include "Koliada.h"
#include "interface/radio.h"

// In this test we do basic input/output using the installed radio adapter (if any).
// There are two build configs, one to build a sender (Tx) and one to build a receiver
// (Rx). For a more complex example see: https://docs.koliada.com/kes/examples/TestRadio

////////////////////////////////////////////////////////////////////////////////
//
// For details see: https://docs.koliada.com/kes/examples/TestRadioApi
//
////////////////////////////////////////////////////////////////////////////////

// frame 'data layer' ID
#if 1
#define FRAME_ID0 'E'
#define FRAME_ID1 'W'
#else
#define FRAME_ID0 'M'
#define FRAME_ID1 '0'
#endif
#define RF_CHANNEL 5

#if USE_INTERFACES
	void initDrivers();
	static RADIO radio;
#else
	// Define a RADIO class object
	DefineRadio(radio);
#endif

// local frame buffer (large enough to hold _all_ anticipated frame types)
#define kRxBufSize 64
static byte rxBuf[kRxBufSize];

StaticEvent(rxEvent);
void rxEventHandler(EVENT e, byte *buf, word len)
	{
	// running in application context
	print("%s\n", (char *)buf);
	}

StaticDelegate(rxReady);
void rxReadyHandler(byte *buf, word len)
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

	debug("read %u bytes!\n", len);
	dump(buf, len, 1);

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

void abortHandler(int sig)
	{
	// for this test, we simply exit
	// depending on the circumstances, other options are reasonable including
	// but not limited to;
	//    - reset to OTA,
	//    - reset to APP (assuming a transient abort happend),
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
	// create a RADIO endpoint instance
	objectCreate(radio, SELECTED_RADIO);
#endif

	debug("\nSetting up to RECIEVE on %s\n\n", typeof(radio)->Name);

	// The rx buffer size must be large enough to contain the largest receipt
	// and small enough to maximize the buffer space available to the radio
	int maxFrameSize = IRADIO.Iocntl(radio, kRadioFrameSize);
	print("Max frame size = %u\n", maxFrameSize);
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

	// frame type,
	IRADIO.Iocntl(radio, kRadioSetFrameSig, FRAME_ID0 << 8 | FRAME_ID1);
	
	// channel,
	IRADIO.Iocntl(radio, kRadioSetChannel, RF_CHANNEL);
	
	// start listening, and
	IRADIO.Iocntl(radio, kRadioEnableRx);

	// wait for system events (including the radio event defined above)
	WaitEvent(0, 0);	// never returns!
	
	// When a KoliadaES program exits, control returns to the kernel and any exit
	// delegates defined by the application are run.
	//
	// Typically an embedded <application> program does NOT exit, however, some
	// programs are designed to exit. They may be configuration programes, driver
	// installers, test cases and similar.
	//
	// What happens after exit completes is host/developer defined (as part of board
	// configuration. Typically, OTA will run.

	// For details see: https://docs.koliada.com/kes/ExitHandling
	}


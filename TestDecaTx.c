/*
 * File: TestDecaTx.c
 *
 * Contains: Test decawave radio frame transmission
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
#include "interface/radio.h"

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

StaticDelegate(txDoneDelegate);
void txDoneHandler(byte *frame, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is sent
	print("%s: sent %u bytes\n", frame, len);
	//dump(frame, 16, 1);
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
	// create a radio endpoint instance
	objectCreate(radio, "DW1000");
#endif
					
	debug("\nSetting up to TRANSMIT on %s\n\n", typeof(radio)->Name);

	#define kTxBufSize 64
	int maxFrameSize = IRADIO.Iocntl(radio, kRadioFrameSize);
	print("Max frame size = %u\n", maxFrameSize);
	assert(kTxBufSize <= maxFrameSize);

	IRADIO.Iocntl(radio, kRadioSetChannel, RF_CHANNEL);

	// set the txDone handler
	// set up the txDone delegate (shows we're transmitting)
	objectCreate(txDoneDelegate, delegateTask(txDoneHandler));
	IRADIO.Iocntl(radio, kRadioAddTxDone, txDoneDelegate);

	// specifically not using Clear Channel Avoidance (CCA) in this test
	// CCA requires that the receiver is on and this test requires that receiver remains off

	print("\nhit any key to send\nhit ESC to quit\n");
	do
		{
		int key; while ((key = getch()) == -1);	// get a (uart) keypress (no wait)
		if (key == 0x1b)
			// ESC
			break;

		// create a tx buffer
		static char buf[kTxBufSize] = "EW:Hello World - 0x00!";

		// replace '00' with a random byte (so we know the frame is changing)
		byte i = strnbuf(kTxBufSize, buf, 19, "%02x", randomByte());

		// pad the frame out to the local maximum (starting after the '\0')
		for (i++; i < kTxBufSize; buf[i++] = randomByte());

		print("%s[%u]\n", buf, i);

		// raw radio send (no presentation layer protocols)
		IRADIO.Send(radio, (byte *)buf, i);
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

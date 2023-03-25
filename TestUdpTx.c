/*
 * File: TestUdpTx.c
 *
 * Contains: Test UDP transmission
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
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */
#include "Koliada.h"
#include "interface/udp.h"

// In this test we do basic input/output using the installed Ethernet adapter (if any).
// There are two build configs, one to build a sender (Tx) and one to build a receiver
// (Rx). For a more complex example see: https://docs.koliada.com/kes/examples/TestUdp

////////////////////////////////////////////////////////////////////////////////
//
// For details see: https://docs.koliada.com/kes/examples/TestUdpApi
//
////////////////////////////////////////////////////////////////////////////////

#if USE_INTERFACES
	void initDrivers();
#else
	// Define a UDP class object
	DefineUdp(udp);
#endif

StaticDelegate(txDoneDelegate);
void txDoneHandler(byte *frame, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is sent
	print("%p[%u]: done!\n", frame, len);
	dump(frame, len, 1);
	}

 void TEST()
	{
	printf(">>%s\n", __func__);
	
	debug("Setting up to TRANSMIT\n\n");

#if USE_INTERFACES
	initDrivers();
	UDP udp = IINTERFACE.Find("UDP");
#else
	// create a UDP endpoint instance
	objectCreate(udp);
#endif
					
	#define kTxBufSize 64
	int maxFrameSize = IUDP.Iocntl(udp, kUdpGetMaxFrameSize);
	print("Max frame size = %u\n", maxFrameSize);
	assert(kTxBufSize <= maxFrameSize);

	// set the txDone handler
	// set up the txDone delegate (shows we're transmitting)
	objectCreate(txDoneDelegate, delegateTask(txDoneHandler));
	IUDP.Iocntl(udp, kUdpAddTxDone, txDoneDelegate);

	// establish some inet configuration
	// mac address is already set by the driver
	IUDP.Iocntl(udp, kIpSetGatewayAddr, "192.168.1.1");
	IUDP.Iocntl(udp, kIpSetSubnetMask, "255.255.255.0");
	IUDP.Iocntl(udp, kIpSetLocalAddr, "192.168.1.42");

	// open a UDP socket
	IUDP.Open(udp);
	
	// establish UDP socket configuration
	IUDP.Iocntl(udp, kUdpSetSrcPort, 5001);
	IUDP.Iocntl(udp, kUdpSetDstPort, 5000);
	IUDP.Iocntl(udp, kUdpSetDstAddr, "192.168.1.178");
	//IUDP.Iocntl(udp, kUdpSetDstEp, "192.168.1.42:5001");// single call, same as previous two calls

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

#if 0
		print("%s - ", &buf[17]);
#else
		print("%s\n", buf);
#endif

		// raw udp send (no presentation layer protocols)
		IUDP.Send(udp, (byte *)buf, i);
		} while (1);

	IUDP.Close(udp);

	// we're done
	printf("<<%s\n", __func__);

	// When a KoliadaES program exits, control returns to the kernel and any exit
	// delegates are run. using(radio) (or initUdp) installed an exit delegate,
	// which releases the radio as part of exit processing.
	//
	// Typically an embedded <application> program does NOT exit, however, some
	// programs are designed to exit. They may be configuration programs, or driver
	// installers, test cases and similar.
	//
	// What happens after exit completes is host/developer defined (as part of board
	// configuration. Typically, OTA loader will run.

	// For details see: https://docs.koliada.com/kes/ExitHandling
	}

/*
 * File: TestUdpRx
 *
 * Contains: Test UDP receiving
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
#include "Koliada.h"
#include "interface/udp.h"

// In this test we do basic input/output using the installed Ethernet adapter. There
// are two build configs, one to build a sender (Tx) and one to build a receiver (Rx).
// For a more complex example see: https://docs.koliada.com/kes/examples/TestUdp

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

// local frame buffer
#define kRxBufSize 128
static byte rxBuf[kRxBufSize];

void TEST()
	{
	printf(">>%s\n", __func__);
	debug("Setting up to RECIEVE\n\n");

#if USE_INTERFACES
	initDrivers();
	UDP udp = IINTERFACE.Find("UDP");
#else
	// create a UDP endpoint instance
	objectCreate(udp);
#endif

	// The rx buffer size must be large enough to contain the largest receipt
	// and small enough to maximize the buffer space available to the socket
	int maxFrameSize = IUDP.Iocntl(udp, kUdpGetMaxFrameSize);
	print("Max frame size = %u\n", maxFrameSize);
	assert(kRxBufSize <= maxFrameSize);

	// only polling - wiznet device has it's own buffer
	
	// establish some inet configuration
	// mac address is already set by the driver
	IUDP.Iocntl(udp, kIpSetGatewayAddr, "192.168.1.1");
	IUDP.Iocntl(udp, kIpSetSubnetMask, "255.255.255.0");
	IUDP.Iocntl(udp, kIpSetLocalAddr, "192.168.1.42");

	// open UDP socket
	IUDP.Open(udp);

	// establish UDP socket endpoint configuration
	IUDP.Iocntl(udp, kUdpSetSrcPort, 5001);

	print("\nWaiting for receipts\n");
	do
		{
		while (IUDP.Recv(udp, rxBuf, sizeof(rxBuf)) == (word)-1)
			// make sure the event queue keeps running!
			EventYield();

		// rxBuf will contain the following
		// in_struct_addr IP;   // senders enpoint (ip) address (4 bytes)
		// in_struct_port Port;	// senders endpoint port (2 bytes)
		// UInt16 len;          // length of the user data (2 bytes)
		//
		// A total of 8 bytes which maybe enumerated as follows
		debug("Header data\n");
		dump(rxBuf, 8, 1);

		// and the user data may be enumerated as;
		debug("User data\n");
		word size = Swap16(*((wyde *)&rxBuf[6]));
		debug("data size=%u\n", size);
		dump(&rxBuf[8], size, 1);

		// and/or, if we received a null terminated string, as;
		print("%s", &rxBuf[8]);
		}
	while (1);
	
	// When a KoliadaES program exits, control returns to the kernel and any exit
	// delegates are run.
	//
	// Typically an embedded <application> program does NOT exit, however, some
	// programs are designed to exit. They may be configuration programes, or driver
	// installers, test cases and similar.
	//
	// What happens after exit completes is host/developer defined (as part of board
	// configuration. Typically, OTA will run.

	// For details see: https://docs.koliada.com/kes/ExitHandling
	}


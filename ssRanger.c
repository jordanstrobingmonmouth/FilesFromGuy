/*
 *	File: ssRanger.c
 *
 *	Contains: Decawave DW3000 single sided ranger example
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Copyright © 1989-2023 by Guy McIlroy
 *  All rights reserved.
 *
 *  This module contains confidential, unpublished, proprietary source code.
 *  The copyright notice above does not evidence any actual or intended
 *  publication of such source code.
 *
 *  This code may only be used under license from Koliada, LLC - www.koliada.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */
#define __RADIO_CLASS_H	 // block current class/radio.h

#include "Koliada.h"
#include "interface/dw3000.h"

#include "ssRange.h"

#define USE_DISTANCE	// comment out if distance calc not required (see rangeEventHandler)

#define kRangeTimeoutMs 500 // should not take longer than this!

byte expectedResponse[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1};

// Frame sequence number
static byte seq;

#ifdef USE_DISTANCE	// calculate locally
static double tof;
static double distance;
static Int32 rtd_init, rtd_resp;
#endif

static UInt32 poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
static float clockOffsetRatio;

#ifdef CC8051
// 8051 printf doesn't include float support
// here is a simple one, with only basic format options
char *ftoa(double value, size_t size, char *result, word precision)
	{
	// converts a floating point number to an ascii string
	if (value < 0.0)
		// in this case, negative value is illegal!
		sys.Fatal("ftoa", __LINE__, "negative range!\n");
	word integral = (word) value;
	word fraction = (word) ((value - integral) * (precision));
	strnbuf(size, result, 0, "%d.%u", integral, fraction);
	return result;
	}
#endif

static ssRangeData rangeResult;
static byte rangeReady = 1;

static byte timeout;
StaticTimer(rangeTimer);
static void rangeTimerHandler()
	{
	timeout = 1;
	if (rangeResult)
		{
		// if result was provided in ssRangeTo, it is filled with (error) range details
		memset(rangeResult, 0, sizeof(_ssRangeData));
		}
	rangeReady = 1;
	}

StaticEvent(rangeEvent);
static void rangeEventHandler(EVENT e, byte *buf, word len)
	{
	// running in application context

	// get timestamps embedded in response message
	memcpy(&poll_rx_ts, &buf[RESP_MSG_POLL_RX_TS_IDX], RESP_MSG_TS_LEN);
	memcpy(&resp_tx_ts, &buf[RESP_MSG_RESP_TX_TS_IDX], RESP_MSG_TS_LEN);

#ifdef USE_DISTANCE
	// compute time of flight & distance

	// clock deltas
	rtd_init = resp_rx_ts - poll_tx_ts;
	rtd_resp = resp_tx_ts - poll_rx_ts;

	// clock offset ratio corrects for differing local and remote clock rates
	tof = ((rtd_init - rtd_resp * (1 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
	distance = tof * SPEED_OF_LIGHT;

#else
	
	// NOTE
	// If we don't actually use the range locally, we can simply send the results
	// of the range request to a server and have the server do the calulations
	// (local processing may be a teeny-tiny mcu with no/slow fpu)
	distance = 0.0;

#endif

	// post results ready
	if (rangeResult)
		{
		// if result was provided in ssRangeTo, it is filled with the range details
		ssRangeData result = rangeResult;

		result->ranger = *((word*)&buf[MSG_DST_IDX]);// or NodeAddr
		result->rangee = *((word*)&buf[MSG_SRC_IDX]);
		result->seq = buf[MSG_SEQ_IDX];
		
		// Here we return all the details used to calculate the range plus the
		// calculated distance. This allows any client using these details to also,
		// optionally, verify distance
		
		result->t1 = poll_tx_ts;
		result->t2 = poll_rx_ts;
		result->t3 = resp_tx_ts;
		result->t4 = resp_rx_ts;
		result->cor = clockOffsetRatio;
		result->range = distance;
		}

	// range completed
	rangeReady = 1;
	}

// NOTE
// This Rx handler is _just_ set up to handle ssRangeResponse messages
// We could have blended ssRanger and ssRangee, but having them separate makes
// it simpler to have the choice of which parts to include in any given node.

static RADIO dwRadio;// deca radio interface (should be passed to the handler via delegate)

StaticDelegate(rxReady);
static void rxReadyHandler(byte *buf, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is recieved (while ranging - set up in ssRangTo below)

	// This callback is so the frame can be qualified in relation protocol parsing
	// and then forwarded (and possibly queued) to the application for further
	// processing.
	//
	// Since this is running on the interrupt handler we don't want to waste time!
	//
	// Further note, this system is event driven and without the posting of an
	// event, the _application_ will never know that anything changed!
	//
	// rxReady delegates are called serially in the order they are added to the
	// radio. Therefore, it is important to add high priority handlers first!!

	// This handler will see all incoming frames, but there are two possible ranging
	// frame types we might be interested in;
	//    a) Ranging request  frames coming from a 'ranger', and
	//    b) Ranging response frames coming from a 'rangee' in response to our range request)
	//
	// here, we are only interested in b) Ranging response frames

	// NOTE
	// For this example, there is an implicit assumtion that we are ranging between
	// just two nodes. This allows us to test our expected response message (defined
	// above) against the incoming message (buf) to see if it is in fact, the response
	// message destined for us.
	//
	// A multi-node system whould need to check that any given incomming message
	// is/was;
	//		1) destined for us (destination address matches our NodeAddress)
	//		2) sent by the node we requested ssRangeTo (check source address)
	//    3) in fact, a ssRangeResponse rframe (E1)
	//
	// This could be done in the same way, except that the expected response message
	// would also be updated with the anticipated rangee address in ssRangeTo() for
	// each ranging request (see below where expectedResponseMsg seq is set).
	
	// qualify the frame type
	if (memcmp(buf, expectedResponse, sizeof_ssRangeRequestMsg) == 0 ||
			(len == sizeof_ssRangeResponsMsg &&
			(*((wyde *)&buf[MSG_DST_IDX]) == ((Dw3000)dwRadio)->addr || *((wyde *)&buf[MSG_DST_IDX]) == 0xFFFF) &&
			buf[9] == 0xE1))
		{
		// b) Ranging response frame coming from a 'rangee' in response to our range request)
		//
		// Retrieve poll transmission and response reception timestamps.
		//    The high order byte of each 40-bit time-stamps is discarded here. This is acceptable as, on each device, those
		//    time-stamps are not separated by more than 2**32 device time units (which is around 67 ms) which means that the
		//    calculation of the round-trip delays can be handled by a 32-bit subtraction.
		DWIFACE IDECA = *((DWIFACE *)typeof(dwRadio)->jumps);
		
		IDECA.Iocntl(dwRadio, dwGetTxTimestamp, &poll_tx_ts);
		IDECA.Iocntl(dwRadio, dwGetRxTimestamp, &resp_rx_ts);

		// Read carrier integrator value and calculate clock offset ratio.
		//    The use of the clock offset value to correct the TOF calculation, significantly improves the result of the
		//    SS-TWR where the remote responder unit's clock is a number of PPM offset from the local initiator unit's clock.
		//    As stated elsewhere a fixed offset in range will be seen unless the antenna delay is calibrated and set correctly.
		float offset;
		IDECA.Iocntl(dwRadio, dwGetClockOffset, &offset);
		clockOffsetRatio = offset / ((teta)1 << 26);

		// post the rxEvent (pass up to the application)
 		PostEvent(rangeEvent, buf, len);
		return;
		}
	// note; this ignores frames destined for us but with the wrong seq #
	// having some kind of error event for that might be useful here.
	// or we can change the test to capture them and test the seq separatly
	// or, options abound...

	// default is ignored - some other handler will process
	}

// send a ranging request to target and put the result in the provided buffer
void ssRangeTo(RADIO radio, wyde target, ssRangeData result)
	{
	// trust but verify
	assert(radio == dwRadio);
	DWIFACE IDECA = *((DWIFACE *)typeof(radio)->jumps);

	if (!rangeReady)
		{
		debug("Ranging already in progress\n");
		return;
		}
	
	// set the target addr
	//debug("target=%04x\n", target);
	*((wyde *)&ssRangeRequestMsg[MSG_DST_IDX]) =
	*((wyde *)&expectedResponse[MSG_SRC_IDX]) = target;
	
	// set & increment the seq #
	expectedResponse[MSG_SEQ_IDX] =
	ssRangeRequestMsg[MSG_SEQ_IDX] = seq++;

	// reset state details
	timeout =
	rangeReady = 0;
	rangeResult = result;

	// start ranging
	IDECA.RangeTo(radio, ssRangeRequestMsg, sizeof_ssRangeRequestMsg);
	cmStartTimer(rangeTimer, 0);

	// await the response
	while (!(timeout || rangeReady))
		EventYield();

	cmStopTimer(rangeTimer);
	if (timeout)
		{
		debug("request timeout!\n");
		return;
		}
	
	if (rangeReady)
		{
		if (result)
			{
			// if result was provided, it is filled with the range details
			// we can handle locally or simply return details to caller, but we might
			// also/instead post/send result to an event or a host server...
	
			// here, we simply show the target NodeAddr, seq# & ranged distance
		#ifdef CC8051
			// 8051 printf doesn't include float support
			char fbuf[8];
			debug("%04X[%02X]: %sm\n", result->rangee, result->seq, ftoa(result->range, sizeof(fbuf), fbuf, 100));
		#else
			debug("%04X[%02X]: %3.2fm\n", result->srcAddr, result->seq, result->range);
		#endif
			}
		else
			debug("range ready!\n");
		}
	}

void ssRangerInit(RADIO radio)
	{
	// here we set up to capture and intermediate the receive IRQ handler
	
	// remember the radio details
	dwRadio = radio;

	// set up a timeout timer
	objectCreate(rangeTimer, kIntervalTimer, TICKS(kRangeTimeoutMs));
	OnEvent(rangeTimer, (HANDLER) rangeTimerHandler);

	// here we set up to capture and intermediate the receive IRQ handler
	// create the rxEvent
	objectCreate(rangeEvent);
	OnEvent(rangeEvent, (HANDLER) rangeEventHandler);

	// set the rxReady handler
	// create & set the Rx delegate
	objectCreate(rxReady, delegateTask(rxReadyHandler));
	IRADIO.Iocntl(radio, kRadioAddRxReady, rxReady);
	}

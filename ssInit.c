/*
 *	File: ssInit.c
 *
 *	Contains: Single Sided Decawave Ranger
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Copyright © 1989-2023 by Guy McIlroy.
 *  All rights reserved.
 *
 *  This module contains confidential, unpublished, proprietary source code.
 *  The copyright notice above does not evidence any actual or intended
 *  publication of such source code.
 *
 *  This code may only be used under licence from Koliada, LLC - www.koliada.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */
#include "interface/dw3000.h"
#include "class/delegate.h"

#include "ssRange.h"

#if 0
/////////////////////////////////////////////////////////
// These should be set in the respective board configs!!!
/////////////////////////////////////////////////////////

// Default communication configuration. We use default non-STS DW mode
static dwt_config_t config =
	{
	5,               // Channel number
	DWT_PLEN_128,    // Preamble length. Used in TX only
	DWT_PAC8,        // Preamble acquisition chunk size. Used in RX only
	9,               // TX preamble code. Used in TX only
	9,               // RX preamble code. Used in RX only
	1,               // 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type
	DWT_BR_6M8,      // Data rate
	DWT_PHRMODE_STD, // PHY header mode
	DWT_PHRRATE_STD, // PHY header rate
	(129 + 8 - 8),   // SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only
	DWT_STS_MODE_OFF,// STS disabled
	DWT_STS_LEN_64,  // STS length see allowed values in Enum dwt_sts_lengths_e
	DWT_PDOA_M0      // PDOA mode off
	};
#endif

#if 1
// TX Power Configuration Settings
/////////////////////////////////////////////////////////
// These should be set in the respective board configs!!!
/////////////////////////////////////////////////////////

// Values for the PG_DELAY and TX_POWER registers reflect the bandwidth and power of the spectrum at the current
// temperature. These values can be calibrated prior to taking reference measurements.
dwt_txconfig_t txconfig_options =
	{
	0x34,           /* PG delay. */
	0xfdfdfdfd,     /* TX power. */
	0x0             /* PG count. */
	};
#endif

// Frames used in the ranging process.
//
//    The frames used here comply with the IEEE 802.15.4 standard data frame encoding.
//    The KoliadaES decawave driver 'knows' about these frames and will 'auto respond'
//    to any 'broadcast' or 'targeted' (addressed) ssRangeRequestMsg.
//
// The frames are detailed as follows:
//
//     ssRangeRequestMsg - a poll message sent by the ranger to trigger the
//                         ranging exchange.
//     ssRangeResponsMsg - a response message sent by the responder to complete
//                         the exchange and provide all information needed by the
//                         initiator to compute the time-of-flight (distance) estimate.
//
//    The first 10 bytes of those frame are common and are composed of the following fields:
//     - byte 0/1: frame control (0x8841 to indicate a data frame using 16-bit addressing).
//     - byte 2: sequence number, incremented for each new frame.
//     - byte 3/4: PAN ID (0xDECA).
//     - byte 5/6: destination address.
//     - byte 7/8: source address.
//     - byte 9: function code (specific values to indicate which message it is in the ranging process).
//
//    The remaining bytes are specific to each message as follows:
//       ssRangeRequestMsg - no more data
//       ssRangeResponsMsg:
//          - byte 10 -> 13: request message time of arrival timestamp.
//          - byte 14 -> 17: response message time of transmission timestamp.
//
byte ssRangeRequestMsg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0};
#ifdef USE_RANGEE
byte ssRangeResponsMsg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V', 'E', 'W', 'A', 0xE1, 0, 0, 0, 0, 0, 0, 0, 0, };
#endif
//
// As they are shown here, these will only work for two devices working alone.
//
// For multi-device, use device addressing (and filtering) to target frames
// to/from specific nodes (see below & ssRanger.c)
//
// By servicing the ranging message frames entirely with your own delegates, it
// is possible to avoid using 802.15.4 and use proprietary fframe formats. Going
// 'off the reservation' in this way is not for the faint of heart!

void ssInit(RADIO radio)
	{
	// confirm we are using DWXXX radio
	if (!(typeof(radio)->Name[0] == 'D' && typeof(radio)->Name[1] == 'W'))
		sys.Fatal("ssInit", __LINE__, "%s - ranging needs a range capable radio!", typeof(radio)->Name);
	
	DWIFACE IDECA = *((DWIFACE *)typeof(radio)->jumps);

	// assign a node addr - using last pair of bytes from the serial number
	byte *serial = sysSerialNumber();
	wyde addr =
		// also set in the request, response & expected response frames
		*((wyde *)&expectedResponse[MSG_DST_IDX]) =
		*((wyde *)&ssRangeRequestMsg[MSG_SRC_IDX]) =	((word) serial[14]) << 8 | serial[15];
	IDECA.Iocntl(radio, kRadioSetAddr, addr);
	
	// Configure the TX spectrum parameters (power, PG delay and PG count)
	IDECA.Iocntl(radio, dwSetTxRfConfig, &txconfig_options);

	// apply antenna delay values
	IDECA.Iocntl(radio, dwSetRxAntennaDelay, RX_ANT_DLY);
	IDECA.Iocntl(radio, dwSetTxAntennaDelay, TX_ANT_DLY);

#if 0
	// Enable frame filtering (only data frames to our address)
	IDECA.Iocntl(radio, dwSetPanId(0xDECA);
	IDECA.Iocntl(radio, dwSetAddress16(addr);
	IDECA.Iocntl(radio, dwEnableFrameFilter(DWT_FF_DATA_EN);

	tx_poll_msg[MSG_SRC_IDX + 0] = (addr >> 0) & 0xff;
	tx_poll_msg[MSG_SRC_IDX + 1] = (addr >> 8) & 0xff;
#endif

#ifdef USE_RANGEE
	ssRangeeInit(radio);
#endif
	ssRangerInit(radio);
	}

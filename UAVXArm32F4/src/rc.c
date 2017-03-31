// ===============================================================================================
// =                                UAVX Quadrocopter Controller                                 =
// =                           Copyright (c) 2008 by Prof. Greg Egan                             =
// =                 Original V3.15 Copyright (c) 2007 Ing. Wolfgang Mahringer                   =
// =                     http://code.google.com/p/uavp-mods/ http://uavp.ch                      =
// ===============================================================================================

//    This is part of UAVX.

//    UAVX is free software: you can redistribute it and/or modify it under the terms of the GNU 
//    General Public License as published by the Free Software Foundation, either version 3 of the 
//    License, or (at your option) any later version.

//    UAVX is distributed in the hope that it will be useful,but WITHOUT ANY WARRANTY; without
//    even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
//    See the GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License along with this program.  
//    If not, see http://www.gnu.org/licenses/

#include "UAVX.h"

// Spektrum

boolean SpekHiRes = false;
uint8 SpekChannelCount;

uint8 SpekChanShift;
uint8 SpekChanMask;
real32 SpekRange = 2047.0f;
uint8 SpekFrameSize = 16;
uint16 LostFrameCount = 0;
uint8 SpekFrameNo = 0;

// Futaba SBus


// The endByte is 0x00 on FrSky and some futaba RX's, on Some SBUS2 RX's the value indicates the telemetry byte that is sent after every 4th sbus frame.
// See https://github.com/cleanflight/cleanflight/issues/590#issuecomment-101027349
// and https://github.com/cleanflight/cleanflight/issues/590#issuecomment-101706023

boolean SBusFailsafe = false;
boolean SBusSignalLost = false;
boolean SBusFutabaValidFrame = false;

// Deltang

uint8 RSSIDeltang = 0;

// Common

RCInpDefStruct_t RCInp[RC_MAX_CHANNELS];

uint32 RCLastFrameuS = 0;
uint32 RCSyncWidthuS = 0;
uint32 RCFrameIntervaluS = 0;
uint8 Channel = 0;

int8 SignalCount = RC_GOOD_BUCKET_MAX;

uint8 Map[RC_MAX_CHANNELS], RMap[RC_MAX_CHANNELS];
real32 RC[RC_MAX_CHANNELS], RCp[RC_MAX_CHANNELS];

uint8 DiscoveredRCChannels = 4; // used by PPM/CPPM

real32 MaxCruiseThrottle, DesiredThrottle, IdleThrottle, InitialThrottle,
		StickThrottle;
real32 DesiredCamPitchTrim;
real32 ThrLow, ThrHigh, ThrNeutral;
real32 CurrMaxRollPitchStick;
int8 RCStart;

boolean RxLoopbackEnabled = false;

uint8 CurrComboPort1Config = ComboPort1ConfigUnknown;
uint8 CurrComboPort2Config = ComboPort2Unused;

void RCSerialISR(uint32 TimerVal) {
	int32 Temp;
	uint32 NowuS;
	int16 Width;

	NowuS = uSClock();
	Temp = RCInp[0].PrevEdge;
	if (TimerVal < Temp)
		Temp -= (int32) 0x0000ffff;
	Width = (TimerVal - Temp);
	RCInp[0].PrevEdge = TimerVal;

	if (Width > (int32) MIN_PPM_SYNC_PAUSE_US) { // A pause  > 5ms
		DiscoveredRCChannels = Channel;

		Channel = 0; // Sync pulse detected - next CH is CH1
		RCSyncWidthuS = Width;
		RCFrameIntervaluS = NowuS - RCLastFrameuS;
		RCLastFrameuS = NowuS;

		F.RCFrameOK = true;
		F.RCNewValues = false;
	} else {

		if (RCWidthOK(Width))
			RCInp[Channel].Raw = Width;
		else {
			// preserve old value i.e. default hold
			NV.Stats[RCGlitchesS]++;
			F.RCFrameOK = false;
		}

		// MUST demand rock solid RC frames for autonomous functions not
		// to be cancelled by noise-generated partially correct frames
		if (++Channel >= DiscoveredRCChannels) {
			F.RCNewValues = F.RCFrameOK;
			if (F.RCNewValues)
				SignalCount++;
			else
				SignalCount -= RC_GOOD_RATIO;
			SignalCount = Limit1(SignalCount, RC_GOOD_BUCKET_MAX);
			F.Signal = SignalCount > 0;
		}
	}

} // RCSerialISR


void RCParallelISR(TIM_TypeDef *tim) {
	static uint8 OKChannels = 0;
	uint8 c;
	uint32 TimerVal = 0;
	int32 Width;
	RCInpDefStruct_t * RCPtr;
	TIMChannelDef * u;
	uint32 NowuS;

	// scan ALL RC inputs as the channel pulses arrive
	// in arbitrary order depending on Rx
	for (c = 0; c < MAX_RC_INPS; c++) {
		u = &RCPins[c].Timer;

		if ((u->Tim == tim) && (TIM_GetITStatus(tim, u->CC) == SET)) {
			TIM_ClearITPendingBit(u->Tim, u->CC);
			switch (u->Channel) {
			case TIM_Channel_1:
				TimerVal = TIM_GetCapture1(u->Tim);
				break;
			case TIM_Channel_2:
				TimerVal = TIM_GetCapture2(u->Tim);
				break;
			case TIM_Channel_3:
				TimerVal = TIM_GetCapture3(u->Tim);
				break;
			case TIM_Channel_4:
				TimerVal = TIM_GetCapture4(u->Tim);
				break;
			} // switch

			// hard coded param DiscoveredRCChannels = Max(DiscoveredRCChannels, c+1);

			RCPtr = &RCInp[c];

			if (RCPtr->State) {
				RCPtr->FallingEdge = TimerVal & 0x0000ffff; // worst case 16 bit timer
				RCPtr->State = false;

				if (RCPtr->FallingEdge > RCPtr->RisingEdge)
					Width = (RCPtr->FallingEdge - RCPtr->RisingEdge);
				else
					//Width = ((0x0000ffff - RCPtr->RisingEdge) + RCPtr->FallingEdge);
					Width = ((RCPtr->FallingEdge + 0x0000ffff)
							- RCPtr->RisingEdge);

				if (RCWidthOK(Width)) {
					RCPtr->Raw = Width;
					OKChannels++;
				} else
					NV.Stats[RCGlitchesS]++;

				if (c == 0) {

					F.RCFrameOK = OKChannels == DiscoveredRCChannels;

					F.RCNewValues = F.RCFrameOK;
					if (F.RCNewValues) {
						NowuS = uSClock();
						RCFrameIntervaluS = NowuS - RCLastFrameuS;
						RCLastFrameuS = NowuS;
						SignalCount++;
					} else
						SignalCount -= RC_GOOD_RATIO;

					SignalCount = Limit1(SignalCount, RC_GOOD_BUCKET_MAX);
					OKChannels = 0;
					F.Signal = SignalCount > 0;
					F.RCFrameOK = false;
				}
				TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
			} else {
				RCPtr->RisingEdge = TimerVal & 0x0000ffff;
				RCPtr->State = true;
				TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
			}
			TIM_ICInitStructure.TIM_Channel = u->Channel;
			TIM_ICInit(u->Tim, &TIM_ICInitStructure);
		}
	}

} // RCParallelISR

// Futaba SBus

/*
 * Observations
 *
 * FrSky X8R
 * time between frames: 6ms.
 * time to send frame: 3ms.
 *
 * Futaba R6208SB/R6303SB
 * time between frames: 11ms.
 * time to send frame: 3ms.
 */

void DoSBus(void) {
	uint8 i;

	RCInp[0].Raw = RCFrame.u.c.c1;
	RCInp[1].Raw = RCFrame.u.c.c2;
	RCInp[2].Raw = RCFrame.u.c.c3; // Futaba Throttle
	RCInp[3].Raw = RCFrame.u.c.c4;
	RCInp[4].Raw = RCFrame.u.c.c5;
	RCInp[5].Raw = RCFrame.u.c.c6;
	RCInp[6].Raw = RCFrame.u.c.c7;
	RCInp[7].Raw = RCFrame.u.c.c8;
	RCInp[8].Raw = RCFrame.u.c.c9;
	RCInp[9].Raw = RCFrame.u.c.c10;
	RCInp[10].Raw = RCFrame.u.c.c11;
	RCInp[11].Raw = RCFrame.u.c.c12;
	RCInp[12].Raw = RCFrame.u.c.c13;
	RCInp[13].Raw = RCFrame.u.c.c14;
	RCInp[14].Raw = RCFrame.u.c.c15;
	RCInp[15].Raw = RCFrame.u.c.c16;

	for (i = 0; i < 16; i++)
		RCInp[i].Raw = (int16) ((real32) (RCInp[i].Raw - 1024) * 1.6f) + 1000;

	RCInp[16].Raw = RCFrame.u.b[22] & 0b0001 ? 2000 : 1000;
	RCInp[17].Raw = RCFrame.u.b[22] & 0b0010 ? 2000 : 1000;

	F.RCNewValues = true;

} // DoSBus


void SpektrumDecode(void) {
	int16 v;
	uint8 Channel;

	if ((RCFrame.u.b[0] & RCFrame.u.b[1]) != 0xff) {

		//uint8 SpekFrameNo = (RCFrame.u.b[index] >> 7) & 1;
		Channel = (RCFrame.u.b[0] >> SpekChanShift) & 0x0f;
		if ((Channel + 1) > DiscoveredRCChannels)
			DiscoveredRCChannels = Channel + 1;
		v = ((RCFrame.u.b[0] & SpekChanMask) << 8) | RCFrame.u.b[1];

		RCInp[Channel].Raw = ((real32) v * (SPEK_RANGE_US / SpekRange))
				+ SPEK_OFFSET_US;
	}

} // spektrumDecode

void SpektrumSBusISR(uint8 v) { // based on MultiWii

	enum {
		SBusWaitSentinel, SBusWaitData, SBusWaitEnd
	};

	uint32 Interval, NowuS;

	NowuS = uSClock();
	Interval = NowuS - RCFrame.lastByteReceived; // uS clock wraps every 71 minutes - ignore
	RCFrame.lastByteReceived = NowuS;

	switch (CurrComboPort1Config) {
	case FutabaSBus_M7to10:

		if (Interval > SBUS_MIN_SYNC_PAUSE_US) {
			RCSyncWidthuS = Interval;
			RCFrame.index = 0;
			RCFrame.state = SBusWaitSentinel;
		}

		switch (RCFrame.state) {
		case SBusWaitSentinel:
			if ((RCFrame.index == 0) && (v == SBUS_START_BYTE)) {
				RCFrame.state = SBusWaitData;
				RCFrame.index = 0;
			}
			break;
		case SBusWaitData:
			RCFrame.u.b[RCFrame.index++] = v;
			if (RCFrame.index == 23)
				RCFrame.state = SBusWaitEnd;
			break;
		case SBusWaitEnd:

			SBusFutabaValidFrame = v == SBUS_END_BYTE; // Futaba should be zero others not!
			SBusSignalLost = (RCFrame.u.b[22] & SBUS_SIGNALLOST_MASK) != 0;
			SBusFailsafe = (RCFrame.u.b[22] & SBUS_FAILSAFE_MASK) != 0;

			F.RCFrameOK = !SBusSignalLost;
			if (F.RCFrameOK) {
				F.RCFrameReceived = F.Signal = true;
				RCFrameIntervaluS = NowuS - RCLastFrameuS;
				RCLastFrameuS = NowuS;
				SignalCount++;
			} else {
				SignalCount -= RC_GOOD_RATIO;
				NV.Stats[RCGlitchesS]++;
			}

			SignalCount = Limit1(SignalCount, RC_GOOD_BUCKET_MAX);
			F.Signal = SignalCount > 0;
			RCFrame.index = 0;
			RCFrame.state = SBusWaitSentinel;

			break;
		}
		break;
	case Deltang1024_M7to10:
	case Spektrum1024_M7to10:
	case Spektrum2048_M7to10:
	case BadDM9_M7to10:
		if (Interval > (uint32) MIN_SPEK_SYNC_PAUSE_US) {
			RCSyncWidthuS = Interval;
			RCFrame.index = 0;
		}

		RCFrame.u.b[RCFrame.index++] = v;
		if (RCFrame.index >= SPEK_FRAME_SIZE) {
			RCFrameIntervaluS = NowuS - RCLastFrameuS;
			RCLastFrameuS = NowuS;
			F.RCFrameReceived = F.Signal = true;
		}
		break;
	default:
		break;
	} // switch

} // SpektrumSBusISR

boolean CheckDeltang(void) {
	// http://www.deltang.co.uk/serial.htm
	uint8 i, CheckSum;
	boolean OK = true;

	if (CurrComboPort1Config == Deltang1024_M7to10) {
		CheckSum = 0;
		for (i = 1; i < 16; i++)
			CheckSum += RCFrame.u.b[i];

		OK &= (RCFrame.u.b[0] == CheckSum) && ((RCFrame.u.b[1] & 0x80) != 0);
	}

	return (OK);
} // CheckDeltang


void CheckSpektrumSBus(void) {
	uint8 i;
	uint16 v;

	if (F.RCFrameReceived) {
		F.RCFrameReceived = false;
		switch (CurrComboPort1Config) {
		case FutabaSBus_M7to10:
			DoSBus();
			break;
		case Deltang1024_M7to10:
		case Spektrum1024_M7to10:
		case Spektrum2048_M7to10:
		case BadDM9_M7to10:

			for (i = 2; i < SPEK_FRAME_SIZE; i += 2)
				if ((RCFrame.u.b[i] & RCFrame.u.b[i + 1]) != 0xff) {

					SpekFrameNo = (i == 2) && ((RCFrame.u.b[i] >> 7) == 1);
					Channel = (RCFrame.u.b[i] >> SpekChanShift) & 0x0f;
					if ((Channel + 1) > DiscoveredRCChannels)
						DiscoveredRCChannels = Channel + 1;
					v = ((uint32) (RCFrame.u.b[i] & SpekChanMask) << 8)
							| RCFrame.u.b[i + 1];

					RCInp[Channel].Raw
							= CurrComboPort1Config == BadDM9_M7to10 ? ((v - 588)
									* 1.18) + 1500
									: ((real32) v * (SPEK_RANGE_US / SpekRange))
											+ SPEK_OFFSET_US;
				}

			if (CurrComboPort1Config == Deltang1024_M7to10)
				RSSIDeltang = RCFrame.u.b[1] & 0x1f;
			else
				LostFrameCount = ((uint16) RCFrame.u.b[0] << 8) //TODO:???
						| RCFrame.u.b[1];

			F.RCNewValues = CheckDeltang();

			break;
		default:
			break;
		}// switch
	}
} // CheckSpektrumSBus


// Code-based Spektrum satellite receiver binding for the HobbyKing Pocket Quad
// Spektrum binding code due to Andrew L.
// navigation07@gmail.com

// Merge idea due to davidea using standard bind link between GND and THR at startup

// Bind Mode Table:
// 2 low pulses: DSM2 1024/22ms
// 3 low pulses: no result
// 4 low pulses: DSM2 2048/11ms
// 5 low pulses: no result
// 6 low pulses: DSMX 22ms
// 7 low pulses: no result
// 8 low pulses: DSMX 11ms

#if (SPEKTRUM == 1024)
#define SPEK_BIND_PULSES 2
#else
#define SPEK_BIND_PULSES 4
#endif

#if defined(BIND)

void doSpektrumBinding(void) {
	uint8 pulse;

	pinMode(7, INPUT); // THR pin as input
	digitalWrite(7, HIGH); // turn on pullup resistors

	if (!digitalRead(7)) {

		pinMode(0, OUTPUT); // Tx pin for satellite
		digitalWrite(0, HIGH);

		pinMode(0, OUTPUT);

		digitalWrite(0, HIGH);
		delayMicroseconds(116);

		for (pulse = 0; pulse < SPEK_BIND_PULSES; pulse++) {
			digitalWrite(0, LOW);
			delayMicroseconds(116);
			digitalWrite(0, HIGH);
			delayMicroseconds(116);
		}

		pinMode(0, INPUT);
	}
} // checkSpektrumBinding

#endif // BIND
// Number of low pulses sent to satellite receivers for binding
#define MASTER_RX_PULSES 		5
#define SLAVE_RX_PULSES 		6

void DoSpektrumBind(void) {
	uint8 i;
	PinDef p;

	p.Port = SerialPorts[RCSerial].Port;
	p.Pin = SerialPorts[RCSerial].RxPin;
#if defined(STM32F1)
	p.Mode = GPIO_Mode_Out_PP;
#else
	p.Mode = GPIO_Mode_OUT;
	p.OType = GPIO_OType_PP;
	p.PuPd = GPIO_PuPd_UP;
#endif

	pinInit(&p);
	// need to power the Rx off one of the pins so power up can be controlled.
	digitalWrite(&p, 1);

	Delay1mS(61); // let satellites settle after power up

	for (i = 0; i < MASTER_RX_PULSES; i++) {
		digitalWrite(&p, 0);
		Delay1uS(120);
		digitalWrite(&p, 1);
		Delay1uS(120);
	}

	InitSerialPort(RCSerial, true);

	while (!F.RCFrameReceived)
		CheckSpektrumSBus();

} // DoSpektrumBind

void UpdateRCMap(void) {
	uint8 c;

	for (c = 0; c < RC_MAX_CHANNELS; c++)
		Map[c] = c;

	Map[ThrottleRC] = P(RxThrottleCh);
	Map[RollRC] = P(RxRollCh);
	Map[PitchRC] = P(RxPitchCh);
	Map[YawRC] = P(RxYawCh);

	Map[RTHRC] = P(RxGearCh);
	Map[RateControlRC] = P(RxAux1Ch);
	Map[NavGainRC] = P(RxAux2Ch);
	Map[BypassRC] = P(RxAux3Ch);
	Map[CamPitchRC] = P(RxAux4Ch);
	Map[TuneRC] = P(RxAux5Ch);
	Map[Aux6RC] = P(RxAux6Ch);
	Map[Aux7RC] = P(RxAux7Ch);

	for (c = ThrottleRC; c < NullRC; c++)
		Map[c] -= 1;

	for (c = 0; c < RC_MAX_CHANNELS; c++)
		RMap[Map[c]] = c;

} // UpdateRCMap

void InitRC(void) {
	uint32 NowmS;
	uint8 c;
	RCInpDefStruct_t * R;

	DiscoveredRCChannels = 1;

	RCLastFrameuS = uSClock();
	RCStart = RC_INIT_FRAMES;
	NowmS = mSClock();

	memset((void *) &RCFrame, 0, sizeof(RCFrame));

	switch (CurrComboPort1Config) {
	case ParallelPPM:
		DiscoveredRCChannels = P(RCChannels);
	case FutabaSBus_M7to10:
		DiscoveredRCChannels = SBUS_CHANNELS;
		RxEnabled[RCSerial] = true;
		break;
	case Deltang1024_M7to10:
	case Spektrum1024_M7to10:
	case BadDM9_M7to10:
		SpekChanShift = 2;
		SpekChanMask = 0x03;
		SpekRange = 1023.0f;
		RxEnabled[RCSerial] = true;
		break;
	case Spektrum2048_M7to10:
		SpekChanShift = 3;
		SpekChanMask = 0x07;
		SpekRange = 2047.0f;
		RxEnabled[RCSerial] = true;
		break;
	default:
		RxEnabled[RCSerial] = false;
		break;
	} // switch

	//DoSpektrumBind();

	UpdateRCMap();

	for (c = 0; c < RC_MAX_CHANNELS; c++) {
		R = &RCInp[c];

		R->State = true;
		R->PrevEdge = R->Raw = R->FallingEdge = R->RisingEdge = 0;

		RC[c] = RCp[c] = 0;
	}
	for (c = RollRC; c <= YawRC; c++)
		RC[c] = RCp[c] = RC_NEUTRAL;
	RC[CamPitchRC] = RCp[CamPitchRC] = RC_NEUTRAL;

	DesiredCamPitchTrim = 0;
	Nav.Sensitivity = 0.0f;
	F.ReturnHome = F.Navigate = F.AltControlEnabled = false;

	mS[StickChangeUpdate] = NowmS;
	mSTimer(NowmS, RxFailsafeTimeout, RC_NO_CHANGE_TIMEOUT_MS);
	F.SticksUnchangedFailsafe = false;

	DesiredThrottle = StickThrottle = 0.0f;

	Channel = NV.Stats[RCGlitchesS] = 0;
	SignalCount = -RC_GOOD_BUCKET_MAX;
	F.Signal = F.RCNewValues = false;

	NavSwState = 0;

} // InitRC

void MapRC(void) { // re-maps captured PPM to Rx channel sequence
	uint8 c, cc;
	real32 Temp;

	for (c = 0; c < RC_MAX_CHANNELS; c++) {
		cc = RMap[c];
		RCp[cc] = RC[cc];
		Temp = (RCInp[c].Raw - 1000) * 0.001;
		RC[cc] = RCFilter(RCp[cc], Temp);
	}
} // MapRC

void CheckSticksHaveChanged(void) {
	static boolean Change = false;
	uint32 NowmS;
	uint8 c;

	NowmS = mSClock();
	if (F.ReturnHome || F.Navigate) {
		Change = true;
		mS[RxFailsafeTimeout] = NowmS + RC_NO_CHANGE_TIMEOUT_MS;
		F.SticksUnchangedFailsafe = false;
	} else {
		if (NowmS > mS[StickChangeUpdate]) {
			mS[StickChangeUpdate] = NowmS + 500;

			Change = false;
			for (c = ThrottleC; c <= RTHRC; c++) {
				Change |= Abs( RC[c] - RCp[c]) > RC_MOVEMENT_STICK;
				RCp[c] = RC[c];
			}
		}

		if (Change) {
			mSTimer(NowmS, RxFailsafeTimeout, RC_NO_CHANGE_TIMEOUT_MS);
			mS[NavStateTimeout] = NowmS;
			F.SticksUnchangedFailsafe = false;

			if (FailState == Monitoring) {
				if (F.LostModel) {
					F.LostModel = false;
				}
			}
		} else if (NowmS > mS[RxFailsafeTimeout]) {
			if (!F.SticksUnchangedFailsafe && (State == InFlight)) {
				mSTimer(NowmS, NavStateTimeout, (uint32) P(DescentDelayS)
						* 1000);
				F.SticksUnchangedFailsafe = true;
			}
		}
	}

} // CheckSticksHaveChanged

void CheckRC(void) {

	switch (CurrComboPort1Config) {
	case CPPM_GPS_M7to10:
		break;
	case ParallelPPM:
		// nothing to do
		break;
	case Deltang1024_M7to10:
	case Spektrum1024_M7to10:
	case Spektrum2048_M7to10:
	case BadDM9_M7to10:
	case FutabaSBus_M7to10:
		CheckSpektrumSBus();
		break;
	default:
		F.RCNewValues = F.Signal = false;
		break;
	} // switch

	if (uSClock() > (RCLastFrameuS + RC_SIGNAL_TIMEOUT_US)) {
		F.Signal = false;
		SignalCount = -RC_GOOD_BUCKET_MAX;
	}

} // CheckRC


uint16 SBusInsert(uint16 v) {

	return ((v - 1000) * 0.625f + SBUS_CHVAL_NEUTRAL);

} // SBusInsert


void SBusLoopback(void) {

	static RCFrameStruct_t TestFrame;

	static uint32 NextUpdateuS = 0;
	static boolean Primed = false;
	static uint16 Wiggle = 0;
	static uint32 NextWigglemS = 0;
	uint8 i;

	if (!Primed) {

		TestFrame.u.c.c1 = SBusInsert(1500);
		TestFrame.u.c.c2 = SBusInsert(1500);
		TestFrame.u.c.c3 = SBusInsert(1000);
		TestFrame.u.c.c4 = SBusInsert(1500);
		TestFrame.u.c.c5 = SBusInsert(1000);
		TestFrame.u.c.c6 = SBusInsert(1100);
		TestFrame.u.c.c7 = SBusInsert(1200);
		TestFrame.u.c.c8 = SBusInsert(1300);
		TestFrame.u.c.c9 = SBusInsert(1400);
		TestFrame.u.c.c10 = SBusInsert(1500);
		TestFrame.u.c.c11 = SBusInsert(1600);
		TestFrame.u.c.c12 = SBusInsert(1700);
		TestFrame.u.c.c13 = SBusInsert(1800);
		TestFrame.u.c.c14 = SBusInsert(1900);
		TestFrame.u.c.c15 = SBusInsert(2000);
		TestFrame.u.c.c15 = SBusInsert(1000);
		TestFrame.u.c.c16 = SBusInsert(1000);

		NextUpdateuS = uSClock();

		Primed = true;
	}

	if (uSClock() > NextUpdateuS) {
		NextUpdateuS = NextUpdateuS + 14000;

		if (mSClock() > NextWigglemS) {
			NextWigglemS = mSClock() + 200;
			TestFrame.u.c.c3 = SBusInsert(1000 + Wiggle);
			Wiggle += 10;
			if (Wiggle > 1000)
				Wiggle = 0;
		}
		TxChar(RCSerial, SBUS_START_BYTE);
		for (i = 0; i < 23; i++)
			TxChar(RCSerial, TestFrame.u.b[i]);
		TxChar(RCSerial, 0);
		TxChar(RCSerial, SBUS_END_BYTE);
	}
} // SBusLoopback

void SpekLoopback(boolean HiRes) {
	const int16 SP[] = { 1000, 1500, 1500, 1500, 1000, 1100, 1200, 1300, 1400,
			1500, 1600, 1700 };
	int8 SpekByte, SpekCh;
	int16 v;
	static uint32 NextUpdateuS = 0;
	static uint8 LBFrame[56];
	static boolean Primed = false;
	//static uint16 LostFrameCount = 0;
	static boolean TicTac = true;
	static uint16 Wiggle = 0;
	static uint32 NextWigglemS = 0;
	uint8 i;

	uint8 Channels = HiRes ? 12 : 7;

	if (!Primed) {
		for (uint8 i = 0; i < 56; i++)
			LBFrame[i] = 0xff;

		for (SpekCh = 0; SpekCh < Channels; SpekCh++) {
			SpekByte = SpekCh * 2;
			// 180..512..854 + 988 offset 332 342

			v = (real32) (SP[SpekCh] - SPEK_OFFSET_US) * (SpekRange
					/ SPEK_RANGE_US);
			v = Limit(v, 0, (int16) SpekRange);

			LBFrame[SpekByte] = (SpekCh << SpekChanShift) | ((v >> 8)
					& SpekChanMask);
			LBFrame[SpekByte + 1] = v & 0xff;
		}

		NextUpdateuS = uSClock();
		Primed = true;
	}

	uint32 NowuS = uSClock();

	if (NowuS > NextUpdateuS) {
		if (TicTac) {

			if (mSClock() > NextWigglemS) {
				NextWigglemS = mSClock() + 500;
				LBFrame[1] = Wiggle;
				Wiggle += 10;
				if (Wiggle > 255)
					Wiggle = 0;
			}

			TxChar(RCSerial, 0);
			TxChar(RCSerial, HiRes ? 0x12 : 0x01);
			for (i = 0; i < 14; i++)
				TxChar(RCSerial, LBFrame[i]);
			TicTac = (Channels > 7) && !HiRes;
		} else {
			TxChar(RCSerial, 1);
			TxChar(RCSerial, HiRes ? 0x12 : 0x01);
			for (i = 14; i < 28; i++)
				TxChar(RCSerial, LBFrame[i]);
			TicTac = true;
		}
		NextUpdateuS = NextUpdateuS + (HiRes ? 11000 : 22000);
	}
} // SpekLoopback

void CheckRxLoopback(void) {

	if (RxLoopbackEnabled)
		switch (CurrComboPort1Config) {
		case CPPM_GPS_M7to10:
			break;
		case ParallelPPM:
			break;
		case BadDM9_M7to10:
		case Deltang1024_M7to10:
		case Spektrum1024_M7to10:
		case Spektrum2048_M7to10:
			SpekLoopback(CurrComboPort1Config == Spektrum2048_M7to10);
			break;
		case FutabaSBus_M7to10:
			SBusLoopback();
			break;
		default:
			break;
		} // switch

} // CheckRxLoopback


void UpdateControls(void) {
	boolean swState;

	CheckRC();
	if (F.RCNewValues) {
		F.RCNewValues = false;

		LEDOn(LEDGreenSel);

		MapRC(); // re-map channel order for specific Tx

		//_________________________________________________________________________________________

		// Attitude

		// normalise from +/- 0.5
		A[Roll].Desired = (RC[RollRC] - RC_NEUTRAL) * STICK_SCALE;
		A[Pitch].Desired = (RC[PitchRC] - RC_NEUTRAL) * STICK_SCALE;
		A[Yaw].Desired = (RC[YawRC] - RC_NEUTRAL) * STICK_SCALE;

		CurrMaxRollPitchStick
				= Max(Abs(A[Roll].Desired), Abs(A[Pitch].Desired));

		F.AttitudeHold = CurrMaxRollPitchStick < ATTITUDE_HOLD_LIMIT_STICK;

		//_________________________________________________________________________________________

		// Throttle

		StickThrottle = RC[ThrottleRC];
		F.ThrottleOpen = StickThrottle >= RC_THRES_START_STICK;

		if (F.AltControlEnabled && (NavState != HoldingStation) && (NavState
				!= PIC) && (NavState != Touchdown))
			StickThrottle = CruiseThrottle;

		DesiredThrottle = StickThrottle;

		if ((!F.HoldingAlt) && (!(F.Navigate || F.ReturnHome))) // override current altitude hold setting
			DesiredAltitude = Altitude;

		//_________________________________________________________________________________________

		// Switch Processing

		if (DiscoveredRCChannels > Map[RTHRC]) {
			NavSwState = Limit((uint8)(RC[RTHRC] * 3.0f), NavSwLow, NavSwHigh);
			UpdateRTHSwState();
		} else {
			F.ReturnHome = F.Navigate = F.NavigationEnabled
					= F.NavigationActive = false;
		}

		if (DiscoveredRCChannels > Map[NavGainRC]) {
			real32 sens = RC[NavGainRC];
			sens = Limit(sens, 0.0f, RC_MAXIMUM);
			F.AltControlEnabled = ((sens > NAV_SENS_ALT_THRESHOLD_STICK)
					&& !F.UseManualAltHold) || F.FailsafesEnabled;
			Nav.Sensitivity
					= Limit(sens - NAV_SENS_THRESHOLD_STICK, 0.0f, 1.0f);
		} else {
			Nav.Sensitivity = 0.0f; //zzzFromPercent(50);
			F.AltControlEnabled = false; //zzztrue;
		}

		DesiredCamPitchTrim
				= DiscoveredRCChannels > Map[CamPitchRC] ? RC[CamPitchRC]
						- RC_NEUTRAL : 0;

		F.UsingRateControl = (DiscoveredRCChannels > Map[RateControlRC])
				&& (RC[RateControlRC] > FromPercent(70)) && !(F.ReturnHome
				|| F.Navigate);

		swState = (DiscoveredRCChannels > Map[BypassRC]) && (RC[BypassRC]
				> FromPercent(70));

		F.Bypass
				= (IsFixedWing) && (DiscoveredRCChannels <= Map[BypassRC]) ? false
						: swState;

		TuningScale
				= ((DiscoveredRCChannels > Map[TuneRC]) && Tuning) ? Limit(RC[TuneRC] + 0.5f, 0.5f, 1.5f)
						: 1.0f;

		//_________________________________________________________________________________________

		// Rx has gone to failsafe

		CheckSticksHaveChanged();

		if (RCStart == 0)
			F.NewCommands = true;
		else
			RCStart--;

		Tune();
	}

} // UpdateControls

void CheckThrottleMoved(void) {
	uint32 NowmS;

	NowmS = mSClock();
	if (NowmS < mS[ThrottleUpdate])
		ThrNeutral = DesiredThrottle;
	else {
		ThrLow = ThrNeutral - THR_MIDDLE_STICK;
		ThrLow = Max(ThrLow, THR_MIN_ALT_HOLD_STICK);
		ThrHigh = ThrNeutral + THR_MIDDLE_STICK;
		if ((DesiredThrottle <= ThrLow) || (DesiredThrottle >= ThrHigh)) {
			mSTimer(NowmS, ThrottleUpdate, THR_UPDATE_MS);
			F.ThrottleMoving = true;
		} else
			F.ThrottleMoving = false;
	}
} // CheckThrottleMoved


/*

 * General Futaba SBus Encoding Scheme

 * The data is contained in 16 byte packets transmitted at 115200 baud.

 * The format of each frame, as know to date, is as follows

 *  byte1:  0x0f sentinel
 *  bytes1-24: packet with 11 bit channel values bit packed towards byte1.
 _______________________________________________________________________

 * General Spektrum Encoding Scheme

 * The bind function means that the satellite receivers believe they are
 * connected to a 9 channel JR-R921 24 receiver thus during the bind process
 * they try to get the transmitter to transmit at the highest resolution that
 * it can manage. The data is contained in 16 byte packets transmitted at
 * 115200 baud. Depending on the transmitter either 1 or 2 frames are required
 * to contain the data for all channels. These frames are either 11ms or 22ms
 * apart.

 * The format of each frame for the main receiver is as follows

 *  byte1:  frame loss data
 *  byte2:  transmitter information
 *  byte3:  and byte4:  channel data
 *  byte5:  and byte6:  channel data
 *  byte7:  and byte8:  channel data
 *  byte9:  and byte10: channel data
 *  byte11: and byte12: channel data
 *  byte13: and byte14: channel data
 *  byte15: and byte16: channel data

 * The format of each frame for the secondary receiver is as follows

 *  byte1:  frame loss data
 *  byte2:  frame loss data
 *  byte3:  and byte4:  channel data
 *  byte5:  and byte6:  channel data
 *  byte7:  and byte8:  channel data
 *  byte9:  and byte10: channel data
 *  byte11: and byte12: channel data
 *  byte13: and byte14: channel data
 *  byte15: and byte16: channel data

 * The frame loss data bytes starts out containing 0 as long as the
 * transmitter is switched on before the receivers. It then increments
 * whenever frames are dropped.

 * Three values for the transmitter information byte have been seen thus far

 * 0x01 From a Spektrum DX7eu which transmits a single frame containing all
 * channel data every 22ms with 10bit resolution.

 * 0x02 From a Spektrum DM9 module which transmits two frames to carry the
 * data for all channels 11ms apart with 10bit resolution.

 * 0x12 From a Spektrum DX7se which transmits two frames to carry the
 * data for all channels 11ms apart with 11bit resolution.

 * 0x12 From a JR X9503 which transmits two frames to carry the
 * data for all channels 11ms apart with 11bit resolution.

 * 0x01 From a Spektrum DX7 which transmits a single frame containing all
 * channel data every 22ms with 10bit resolution.

 * 0x12 From a JR DSX12 which transmits two frames to carry the
 * data for all channels 11ms apart with 11bit resolution.

 * 0x1 From a Spektrum DX5e which transmits a single frame containing all
 * channel data every 22ms with 10bit resolution.

 * 0x01 From a Spektrum DX6i which transmits a single frame containing all
 * channel data every 22ms with 10bit resolution.

 * Currently the assumption is that the data has the form :

 * [0 0 0 R 0 0 N1 N0]

 * where :

 * 0 means a '0' bit
 * R: 0 for 10 bit resolution 1 for 11 bit resolution channel data
 * N1 to N0 is the number of frames required to receive all channel
 * data.

 * Channels can have either 10bit or 11bit resolution. Data from a transmitter
 * with 10 bit resolution has the form:

 * [F 0 C3 C2 C1 C0 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0]

 * Data from a transmitter with 11 bit resolution has the form

 * [F C3 C2 C1 C0 D10 D9 D8 D7 D6 D5 D4 D3 D2 D1 D0]

 * where :

 * 0 means a '0' bit
 * F: Normally 0 but set to 1 for the first channel of the 2nd frame if a
 * second frame is transmitted.

 * C3 to C0 is the channel number, 4 bit, matching the numbers allocated in
 * the transmitter.

 * D9 to D0 is the channel data (10 bit) 0xaa..0x200..0x356 for
 * 100% transmitter-travel

 * D10 to D0 is the channel data (11 bit) 0x154..0x400..0x6AC for
 * 100% transmitter-travel

 * The order of the channels on a Spektrum is always as follows:
 *
 * Throttle   0
 * Aileron    1
 * Elevator   2
 * Rudder     3
 * Gear       4
 * Flap/Aux1  5
 * Aux2       6

 * Aux3       7
 * Aux4       8
 * Aux5       9
 * Aux6      10
 * Aux7      11

 */


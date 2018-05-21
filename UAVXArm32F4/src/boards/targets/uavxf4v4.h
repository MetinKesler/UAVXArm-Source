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

//    UAVX is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
//    even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//    See the GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License along with this program.
//    If not, see http://www.gnu.org/licenses/

#ifndef _pinsf4v4_h
#define _pinsf4v4_h

#include "UAVX.h"

#define MAX_SPI_DEVICES 4

PinDef RCPins[MAX_RC_INPS] = { //
		{ true, GPIOA, GPIO_Pin_0, GPIO_PinSource0, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM2, TIM_Channel_1,
						TIM_IT_CC1, 0, GPIO_AF_TIM2 }, TIM2_IRQn },//
				{ true, GPIOA, GPIO_Pin_1, GPIO_PinSource1, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM2,
								TIM_Channel_2, TIM_IT_CC2, 0, GPIO_AF_TIM2 },
						TIM2_IRQn }, //
				{ true, GPIOA, GPIO_Pin_2, GPIO_PinSource2, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM2,
								TIM_Channel_3, TIM_IT_CC3, 0, GPIO_AF_TIM2 },
						TIM2_IRQn }, //
				{ true, GPIOA, GPIO_Pin_3, GPIO_PinSource3, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM2,
								TIM_Channel_4, TIM_IT_CC4, 0, GPIO_AF_TIM2 },
						TIM2_IRQn }, //
				{ true, GPIOA, GPIO_Pin_6, GPIO_PinSource6, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3,
								TIM_Channel_1, TIM_IT_CC1, 0, GPIO_AF_TIM3 },
						TIM3_IRQn },//
				{ true, GPIOA, GPIO_Pin_7, GPIO_PinSource7, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3,
								TIM_Channel_2, TIM_IT_CC2, 0, GPIO_AF_TIM3 },
						TIM3_IRQn }, //
				{ true, GPIOB, GPIO_Pin_0, GPIO_PinSource0, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3,
								TIM_Channel_3, TIM_IT_CC3, 0, GPIO_AF_TIM3 },
						TIM3_IRQn },//
				{ true, GPIOB, GPIO_Pin_1, GPIO_PinSource1, GPIO_Mode_AF,
						GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3,
								TIM_Channel_4, TIM_IT_CC4, 0, GPIO_AF_TIM3 },
						TIM3_IRQn } };

AnalogPinDef AnalogPins[MAX_ANALOG_CHANNELS] = { //
		{ false, 0, }, // Rangefinder
				{ true, ADC1, GPIOC, GPIO_Pin_1, ADC_Channel_11, DMA_Channel_0,
						DMA2_Stream0, 2 }, // Current
				{ true, ADC1, GPIOC, GPIO_Pin_2, ADC_Channel_12, DMA_Channel_0,
						DMA2_Stream0, 3 }, // Volts

				{ false, 0, }, // Roll
				{ false, 0, }, // Pitch
				{ false, 0, } // Yaw
		};

PinDef PWMPins[MAX_PWM_OUTPUTS] = {
// Drives 1-4
		{ true, GPIOB, GPIO_Pin_9, GPIO_PinSource9, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM4, TIM_Channel_4, 0,
						&(TIM4->CCR4), GPIO_AF_TIM4 } }, //
		{ true, GPIOB, GPIO_Pin_8, GPIO_PinSource8, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM4, TIM_Channel_3, 0,
						&(TIM4->CCR3), GPIO_AF_TIM4 } }, //
		{ true, GPIOB, GPIO_Pin_7, GPIO_PinSource7, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM4, TIM_Channel_2, 0,
						&(TIM4->CCR2), GPIO_AF_TIM4 } }, //
		{ true, GPIOB, GPIO_Pin_6, GPIO_PinSource6, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM4, TIM_Channel_1, 0,
						&(TIM4->CCR1), GPIO_AF_TIM4 } },

		{ true, GPIOC, GPIO_Pin_6, GPIO_PinSource6, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM8, TIM_Channel_1, 0,
						&(TIM8->CCR1), GPIO_AF_TIM8 } }, // optionally WS2812
		{ true, GPIOC, GPIO_Pin_7, GPIO_PinSource7, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM8, TIM_Channel_2, 0,
						&(TIM8->CCR2), GPIO_AF_TIM8 } },

		// Drives 4-8 for brushless
		{ true, GPIOA, GPIO_Pin_6, GPIO_PinSource6, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3, TIM_Channel_1, 0,
						&(TIM3->CCR1), GPIO_AF_TIM3 } }, //
		{ true, GPIOA, GPIO_Pin_7, GPIO_PinSource7, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3, TIM_Channel_2, 0,
						&(TIM3->CCR2), GPIO_AF_TIM3 } }, { true, GPIOB,
				GPIO_Pin_0, GPIO_PinSource0, GPIO_Mode_AF, GPIO_OType_PP,
				GPIO_PuPd_UP, true, { TIM3, TIM_Channel_3, 0, &(TIM3->CCR3),
						GPIO_AF_TIM3 } }, //
		{ true, GPIOB, GPIO_Pin_1, GPIO_PinSource1, GPIO_Mode_AF,
				GPIO_OType_PP, GPIO_PuPd_UP, true, { TIM3, TIM_Channel_4, 0,
						&(TIM3->CCR4), GPIO_AF_TIM3 } }

};

PinDef
		GPIOPins[MAX_GPIO_PINS] =
				{
						{ true, GPIOA, GPIO_Pin_4, GPIO_PinSource4,
								GPIO_Mode_OUT, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0, }, // Beeper
						{ true, GPIOC, GPIO_Pin_10, GPIO_PinSource10,
								GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0, }, // Armed
						{ true, GPIOC, GPIO_Pin_9, GPIO_PinSource9,
								GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0 }, // Landing
						{ true, GPIOA, GPIO_Pin_5, GPIO_PinSource5,
								GPIO_Mode_OUT, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0, }, // Aux1
						{ true, GPIOA, GPIO_Pin_15, GPIO_PinSource15,
								GPIO_Mode_OUT, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0, }, // Aux2
						{ true, GPIOC, GPIO_Pin_8, GPIO_PinSource8,
								GPIO_Mode_OUT, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0, }, // Probe (Aux3)

#if defined(USE_MPU6XXX_INT)
						{	true, GPIOC, GPIO_Pin_14, GPIO_PinSource14, GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP,
							false, {0,}, EXTI15_10_IRQ}, // MPU6XXXIntSel
#else
						{ true, GPIOC, GPIO_Pin_14, GPIO_PinSource14,
								GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0 }, // MPU6XXXIntSel
#endif
#if defined(USE_HMC5XXX_INT)
						{	true, GPIOC, GPIO_Pin_15, GPIO_PinSource15, GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP,
							false, {0,}, EXTI15_10_IRQ}, // HMC5XXXRdySel
#else
						{ true, GPIOC, GPIO_Pin_15, GPIO_PinSource15,
								GPIO_Mode_IN, GPIO_OType_PP, GPIO_PuPd_UP,
								false, { 0, }, 0 }, // HMC5XXXRdySel
#endif
				};

PinDef SPISelectPins[MAX_SPI_DEVICES] = { //
		{ true, GPIOB, GPIO_Pin_12, GPIO_PinSource12, GPIO_Mode_OUT,
				GPIO_OType_PP, GPIO_PuPd_UP, false, { 0, }, 0, }, // MPU6XXX
				{ true, GPIOC, GPIO_Pin_5, GPIO_PinSource5, GPIO_Mode_OUT,
						GPIO_OType_PP, GPIO_PuPd_UP, false, { 0, }, 0, }, // MS56XX
				{ true, GPIOC, GPIO_Pin_4, GPIO_PinSource4, GPIO_Mode_OUT,
						GPIO_OType_PP, GPIO_PuPd_UP, false, { 0, }, 0, }, // HMC5XXX
				{ true, GPIOC, GPIO_Pin_3, GPIO_PinSource3, GPIO_Mode_OUT,
						GPIO_OType_PP, GPIO_PuPd_UP, false, { 0, }, 0, }, // Flash
		};

PinDef LEDPins[MAX_LED_PINS] = { //
		{ true, GPIOB, GPIO_Pin_3, GPIO_PinSource3, GPIO_Mode_OUT,
				GPIO_OType_PP, GPIO_PuPd_NOPULL, false, { 0, }, 0, }, // Yellow
				{ true, GPIOB, GPIO_Pin_4, GPIO_PinSource4, GPIO_Mode_OUT,
						GPIO_OType_PP, GPIO_PuPd_NOPULL, false, { 0, }, 0, }, // Red
				{ true, GPIOC, GPIO_Pin_11, GPIO_PinSource11, GPIO_Mode_OUT,
						GPIO_OType_PP, GPIO_PuPd_NOPULL, false, { 0, }, 0, }, // Blue
				{ true, GPIOC, GPIO_Pin_12, GPIO_PinSource12, GPIO_Mode_OUT,
						GPIO_OType_PP, GPIO_PuPd_NOPULL, false, { 0, }, 0, }// Green
		};

uint8 GPSRxSerial, GPSTxSerial, RCSerial, TelemetrySerial;
boolean RxUsingSerial;

const uint8 currIMUType = mpu6000IMU;
const uint8 currBaroType = ms5611Baro;
const uint8 currMagType = hmc5xxxMag;
const uint8 currGimbalType = noGimbal;

// imuSel, baroSel, magSel, memSel, gpsSel, rfSel, escSel, flowSel, asSel
const uint8 spiMap[] = { 2, 2, 2, 2, 2, 2, 2, 2, 2 }; // SPI2
const uint8 i2cMap[] = { 2, 2, 2, 2, 2, 2, 2, 2, 2 }; // I2C2

boolean spiDevUsed[] = { true, true, true, true, false, false, false, false,
		false };

void InitTarget(void) {

	InitSerialPort(Usart1, true, false);

	if (CurrComboPort2Config != RF_GPS_V4) {
		I2CPorts[i2cMap[baroSel] - 1].Used |= (CurrComboPort2Config
				== RF_Baro_V4) || (CurrComboPort2Config == RF_Baro_Mag_V4);
		I2CPorts[i2cMap[magSel] - 1].Used
				|= (CurrComboPort2Config == RF_Mag_V4) || (CurrComboPort2Config
						== RF_Baro_Mag_V4);

		spiDevUsed[baroSel] = (CurrComboPort2Config != RF_Baro_V4)
				&& (CurrComboPort2Config != RF_Baro_Mag_V4);
		spiDevUsed[magSel] = (CurrComboPort2Config != RF_Mag_V4)
				&& (CurrComboPort2Config != RF_Baro_Mag_V4);
	}

	switch (CurrComboPort1Config) {
	case CPPM_GPS_M7to10:
		CurrMaxPWMOutputs = (UsingDCMotors) ? 4 : 10;
		CurrNoOfRCPins = 1;
		GPSTxSerial = GPSRxSerial = Usart2;
		InitSerialPort(GPSRxSerial, false, false);
		RxUsingSerial = false;
		break;
	case ParallelPPM:
		CurrMaxPWMOutputs = (UsingDCMotors) ? 4 : 6;
		CurrNoOfRCPins = MAX_RC_INPS; //P(RCChannels);
		RxUsingSerial = false;

		if (CurrComboPort2Config == RF_GPS_V4) {
			GPSRxSerial = GPSTxSerial = Usart3;
			InitSerialPort(GPSRxSerial, false, CurrComboPort1Config
					== FutabaSBus_M7to10); // zzz
		} else {
			GPSRxSerial = Usart1;
			GPSTxSerial = SoftSerialTx;
		}

		break;
	default:
		CurrMaxPWMOutputs = (UsingDCMotors) ? 4 : 10;
		CurrNoOfRCPins = 0;
		RxUsingSerial = true;

		if (CurrComboPort2Config == RF_GPS_V4) {
			GPSRxSerial = GPSTxSerial = Usart3;
			InitSerialPort(GPSRxSerial, false, CurrComboPort1Config
					== FutabaSBus_M7to10); // zzz
		} else {
			GPSRxSerial = TelemetrySerial;
			GPSTxSerial = SoftSerialTx;
		}
		RCSerial = Usart2;
		InitSerialPort(RCSerial, false, CurrComboPort1Config
				== FutabaSBus_M7to10);
		break;
	} // switch

	if (CurrwsNoOfLeds > 0) // temporary KLUDGE - need to remap gimbal function
		CurrMaxPWMOutputs = 4;


} // uavxf4v4Init
#endif


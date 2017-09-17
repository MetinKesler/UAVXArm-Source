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

// Autonomous flight routines

#include "UAVX.h"

real32 NavdT;

uint8 NavState, AlarmState;

uint8 LandingState = InitDescent;

uint8 NavSwState = NavSwLow;
uint8 NavSwStateP = NavSwUnknown;

boolean NotDescending(void) {

	return (Abs(ROC) < ALT_MIN_DESCENT_MPS);

} // NotDescending

void InitiateDescent(void) {

	LandingState = InitDescent;
	NavState = Descending;

} // InitiateDescent


boolean DoLanding(void) {
	static uint32 LastLandUpdateuS;
	static int32 bucketmS = 0.0f;
	real32 dTmS;
	boolean HasLanded;

	HasLanded = false;

	DesiredThrottle = IsFixedWing ? 0 : CruiseThrottle;
	DesiredAltitude = -100.0f; // all the way

	switch (LandingState) {
	case InitDescent:
		if (Abs(Altitude) < NAV_LAND_M) {
			bucketmS = 1000;
			mSTimer(mSClock(), NavStateTimeout, bucketmS); // let descent actually start
			LandingState = CommenceDescent;
		}
		break;
	case CommenceDescent:
		if (mSClock() > mS[NavStateTimeout]) {
			LastLandUpdateuS = uSClock();
			bucketmS = NAV_LAND_TIMEOUT_MS;
			LandingState = Descent;
		}
		break;
	case Descent:
		dTmS = dTUpdate(uSClock(), &LastLandUpdateuS) * 1000;
		if (NotDescending()) {
			bucketmS = Max(0, bucketmS - dTmS);
			if (bucketmS <= 0.0f)
				LandingState = DescentStopped;
		} else
			bucketmS = Min(bucketmS + dTmS * 2, NAV_LAND_TIMEOUT_MS);
		mSTimer(mSClock(), NavStateTimeout, bucketmS);
		break;
	case DescentStopped:
		HasLanded = true;
		break;
	} // switch

	return (HasLanded);
} // DoLanding


void InitiateShutdown(uint8 s) {
	ZeroThrottleCompensation();
	ZeroNavCorrections();
	DesiredThrottle = 0.0f;
	StopDrives();
	NavState = s;
	AlarmState = NoAlarms;
	State = Shutdown;
} // InitiateShutdown


void DoAutoLanding(void) {

	if (DoLanding())
		InitiateShutdown(Touchdown);

} // DoAutoLanding


void CheckFence(void) {

	F.FenceAlarm = (((Nav.Distance > Nav.FenceRadius) || (Altitude
			> NAV_CEILING_M)) && F.OriginValid);

} // CheckFence


void CheckRapidDescentHazard(void) {

	F.RapidDescentHazard = F.UsingRapidDescent && ((Altitude - DesiredAltitude)
			> DESCENT_ALT_DIFF_M) && (Altitude > DESCENT_SAFETY_ALT_M);

} // CheckRapidDescentHazard

void CapturePosition(void) {

	if (F.OriginValid) {
		HP.Pos[NorthC] = Nav.C[NorthC].Pos;
		HP.Pos[EastC] = Nav.C[EastC].Pos;
	}

} // CapturePosition

void InitiateRTH(void) {

	F.RapidDescentHazard = F.NewNavUpdate = F.WayPointAchieved
			= F.WayPointCentred = false;
	CurrWPNo = 0;
	PrevWPNo = 255;
	RefreshNavWayPoint();
	NavState = ReturningHome;
	F.ReturnHome = true;

} // InitiateRTH

void InitiatePH(void) {

	CurrWPNo = 0;
	DesiredAltitude = Altitude;
	CapturePosition();
	DesiredHeading = Heading;
	NavState = HoldingStation;
	F.Navigate = true;

} // InitiatePH

void DoGliderStuff(void) {

	if (Altitude > AltMaxM) // TODO: pitch a function of Fl
		NavState = AltitudeLimiting;
	else {
		if (Altitude < AltMinM) //set best climb pitch
			NavState = BoostClimb;
		else {
			if (NavState == JustGliding) {
				if (CommenceThermalling()) { // after cruise timeout
					InitThermalling();
					CapturePosition();
					F.Soaring = true;
					mSTimer(mSClock(), ThermalTimeout, THERMAL_MIN_MS);
					NavState = UsingThermal;
				} else
					Navigate(&HP);
			} else {
				if (ResumeGlide()) {
					F.Soaring = false;
					mSTimer(mSClock(), CruiseTimeout, CRUISE_MIN_MS);
					CapturePosition();
					NavState = JustGliding;
				} else {
					UpdateThermalEstimate();
					Soar.Th[NorthC].Pos = Nav.C[NorthC].Pos + ekf.X[2];
					Soar.Th[EastC].Pos = Nav.C[EastC].Pos + ekf.X[3];
					Navigate(&TH);
				}
			}
		}
	}
} // DoGliderStuff

void UpdateRTHSwState(void) { // called in rc.c on every rx packet

	if (F.Bypass || (State != InFlight)) {

		NavSwState = NavSwLow;
		NavSwStateP = NavSwUnknown;

		F.AltControlEnabled = F.HoldingAlt = false;
		DesiredAltitude = Altitude;
		ZeroThrottleCompensation();
		ZeroNavCorrections();

		if (F.OriginValid)
			CapturePosition();

		NavState = PIC;

	} else {

		if (NavSwState != NavSwStateP) {
			F.Glide = F.Navigate = F.ReturnHome = false;
			switch (NavSwState) {
			case NavSwLow:
				ZeroNavCorrections();
				CapturePosition();
				NavState = PIC;
				break;
			case NavSwMiddle:
				CaptureHomePosition();
				if (F.OriginValid)
					InitiatePH();
				break;
			case NavSwHigh:
				CaptureHomePosition();
				if (F.OriginValid)
					InitiateRTH();

			} // switch

			NavSwStateP = NavSwState;
		}

		F.AltControlEnabled = !(F.UseManualAltHold || (Nav.Sensitivity
				< NAV_ALT_THRESHOLD_STICK)
				|| (IsFixedWing && (NavState == PIC)));

		if ((!F.HoldingAlt) && !(F.Navigate || F.ReturnHome))
			DesiredAltitude = Altitude;

		if (F.AltControlEnabled && !((NavState == HoldingStation) || (NavState
				== PIC) || (NavState == Touchdown)))
			StickThrottle = CruiseThrottle;
	}

	DesiredThrottle = StickThrottle;

} // UpdateRTHSwState


void DoNavigation(void) {

	if ((NavState != PIC) && F.NavigationEnabled
			&& (F.Navigate || F.ReturnHome)
			&& !(F.UsingRateControl || F.Bypass)) {

		if (F.NewNavUpdate) {
			F.NewNavUpdate = false;

			CheckFence();

			switch (NavState) {
			case AltitudeLimiting:
				// don't do hold as we may need to escape thermal

				if (Altitude < AltMaxM) { //set best glide pitch
					Sl = 0.0f;
					NavState = JustGliding;
				}
				break;
			case BoostClimb:
				Navigate(&HP);

				if (Altitude > AltCutoffM) //set best glide pitch
					NavState = JustGliding;
				break;
			case JustGliding:
			case UsingThermal:

				DoGliderStuff();

				break;
			case Takeoff:
				RefreshNavWayPoint();
				Navigate(&WP);

				if (Altitude > NAV_MIN_ALT_M)
					NavState = NextWPState();
				break;
			case Perching:
				if (mSClock() > mS[NavStateTimeout]) {
					DesiredAltitude = WP.Pos[DownC];
					NavState = Takeoff;
				} else {
					A[Pitch].NavCorr = A[Roll].NavCorr = 0.0f;
					DesiredAltitude = -100.0f; // override WP
				}
				break;
			case Touchdown:
				if (WP.Action == navPerch)
					NavState = Perching;
				break;
			case Descending:
				RefreshNavWayPoint();
				Navigate(&WP);

				if (WP.Action == navPerch) {
					if (DoLanding()) {
						mSTimer(mSClock(), NavStateTimeout, WP.Loiter * 1000);
						NavState = Perching;
					}
				} else
					DoAutoLanding();
				break;
			case AtHome:
				Navigate(&WP);

				if ((F.AltControlEnabled && F.UsingRTHAutoDescend)
						&& (mSClock() > mS[NavStateTimeout])) {
					F.RapidDescentHazard = false;
					if (IsFixedWing) {
					} else
						InitiateDescent();
				}
				break;
			case AcquiringAltitude:
				RefreshNavWayPoint();
				Navigate(&WP);

				CheckRapidDescentHazard();

				// need check for already on the ground

				if (F.WayPointAchieved) {

					mSTimer(mSClock(), NavStateTimeout, (int32) WP.Loiter
							* 1000);

					switch (WP.Action) {
					case navLand:
						NavState = AtHome;
						break;
					case navPerch:
						InitiateDescent();
						break;
					default:
						NavState = OrbitingPOI;
					} // switch
					F.RapidDescentHazard = false;
				}
				break;
			case Loitering:
			case OrbitingPOI:
				RefreshNavWayPoint();
				Navigate(&WP);

				switch (WP.Action) {
				case navOrbit:
					OrbitCamAngle = HALF_PI - atan2f(Altitude
							- WP.OrbitAltitude, WP.OrbitRadius);
					OrbitCamAngle = Limit(OrbitCamAngle, 0.0f, HALF_PI);
					NavState = OrbitingPOI;
					break;
				case navVia:
				case navPerch: // fixed wing just orbits
				default:
					OrbitCamAngle = 0.0f;
					break;
				} // switch

				if (mSClock() > mS[NavStateTimeout])
					NavState = NextWPState();

				break;
			case ReturningHome:
			case Transiting:
				RefreshNavWayPoint();
				Navigate(&WP);

				if (F.WayPointCentred)
					NavState = AcquiringAltitude;

				break;
			case HoldingStation:
				if (IsFixedWing && F.Glide)
					NavState = JustGliding;
				else {
					if (F.AttitudeHold) {
						if (NV.Mission.NoOfWayPoints == 0)
							Navigate(&HP); // maintain hold point
						else {
							//zzz	check that wp nav actually starts? CurrWPNo = 0;
							NavState = NextWPState(); // start navigating
						}
					} else { // allow override
						DecayNavCorrections();
						CapturePosition();
					}
				}
				break;
			default:
				// should not happen
				break;
			} // switch NavState

			F.OrbitingWP = (NavState == OrbitingPOI) && (WP.Action == navOrbit);
		}
	} else { // PIC

		NavState = PIC;
		if (F.NewNavUpdate)
			F.NewNavUpdate = false;
		Nav.WPBearing = DesiredHeading;
	}

	F.NewCommands = false; // Navigate modifies Desired Roll, Pitch and Yaw values.

} // DoNavigation


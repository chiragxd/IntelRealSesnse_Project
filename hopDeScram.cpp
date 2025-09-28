VAR_GLOBAL
    // -------------------------
    // Hopper - new
    // -------------------------
    bSmallBottleTrackHighLevel   : BOOL;   // High-level sensor
    bSmallBottleTrackLowLevel    : BOOL;   // Low-level sensor
    bLargeBottleTrackHighLevel   : BOOL;
    bLargeBottleTrackLowLevel    : BOOL;

    bStartSmallHopperMotor       : BOOL;
    bReverseSmallHopperMotor     : BOOL;
    bStartLargeHopperMotor       : BOOL;
    bReverseLargeHopperMotor     : BOOL;

    bHMI_SmallHopperStart        : BOOL;
    bHMI_SmallHopperStop         : BOOL;
    bHMI_SmallHopperReverse      : BOOL;
    bHMI_LargeHopperStart        : BOOL;
    bHMI_LargeHopperStop         : BOOL;
    bHMI_LargeHopperReverse      : BOOL;

    tSmallTrackNotFullDelayTmr   : TON;
    tSmallTrackFullDelayTmr      : TON;
    tLargeTrackNotFullDelayTmr   : TON;
    tLargeTrackFullDelayTmr      : TON;

    nTrackNotFullDelay_ms        : UDINT := 1000;
    nTrackFullDelay_ms           : UDINT := 500;

    // -------------------------
    // Descrambler - new
    // -------------------------
    bStartSmallDescrambler       : BOOL;
    bReverseSmallDescrambler     : BOOL;
    bStartLargeDescrambler       : BOOL;
    bReverseLargeDescrambler     : BOOL;

    bHMI_SmallDescrStart         : BOOL;
    bHMI_SmallDescrStop          : BOOL;
    bHMI_SmallDescrReverse       : BOOL;
    bHMI_LargeDescrStart         : BOOL;
    bHMI_LargeDescrStop          : BOOL;
    bHMI_LargeDescrReverse       : BOOL;

    bSmallDescrJam               : BOOL;
    bLargeDescrJam               : BOOL;

    tSmallDescrRevPulseTmr       : TON;
    tLargeDescrRevPulseTmr       : TON;
    nDescrRevPulse_ms            : UDINT := 800;

    nSmallDescrJamCount          : INT := 0;
    nLargeDescrJamCount          : INT := 0;
    nMaxDescrJamRetries          : INT := 3;

    bSmallDescrFault             : BOOL := FALSE;
    bLargeDescrFault             : BOOL := FALSE;

    bHMI_ResetFault              : BOOL := FALSE;
END_VAR


PROGRAM PRG_BottleHopper
VAR
    bRunPerm      : BOOL;
    bSmallNotFull : BOOL;
    bSmallFull    : BOOL;
    bLargeNotFull : BOOL;
    bLargeFull    : BOOL;
END_VAR

// Safety permissives
bRunPerm := bMCR_PowerOn AND bSorterDoorClosed AND bCapperDoorClosed AND bAirSupplyOK;

// Small track logic
bSmallNotFull := NOT bSmallBottleTrackHighLevel AND bSmallBottleTrackLowLevel;
bSmallFull    := bSmallBottleTrackHighLevel;

tSmallTrackNotFullDelayTmr(IN := (bAutoMode AND bRunPerm AND bSmallNotFull), PT := UDINT_TO_TIME(nTrackNotFullDelay_ms));
tSmallTrackFullDelayTmr   (IN := (bAutoMode AND bRunPerm AND bSmallFull),    PT := UDINT_TO_TIME(nTrackFullDelay_ms));

IF bAutoMode AND NOT bManualMode AND bRunPerm THEN
    IF tSmallTrackNotFullDelayTmr.Q THEN
        bStartSmallHopperMotor := TRUE;
    END_IF;
    IF tSmallTrackFullDelayTmr.Q THEN
        bStartSmallHopperMotor := FALSE;
    END_IF;
    bReverseSmallHopperMotor := FALSE;
END_IF;

// Large track logic
bLargeNotFull := NOT bLargeBottleTrackHighLevel AND bLargeBottleTrackLowLevel;
bLargeFull    := bLargeBottleTrackHighLevel;

tLargeTrackNotFullDelayTmr(IN := (bAutoMode AND bRunPerm AND bLargeNotFull), PT := UDINT_TO_TIME(nTrackNotFullDelay_ms));
tLargeTrackFullDelayTmr   (IN := (bAutoMode AND bRunPerm AND bLargeFull),    PT := UDINT_TO_TIME(nTrackFullDelay_ms));

IF bAutoMode AND NOT bManualMode AND bRunPerm THEN
    IF tLargeTrackNotFullDelayTmr.Q THEN
        bStartLargeHopperMotor := TRUE;
    END_IF;
    IF tLargeTrackFullDelayTmr.Q THEN
        bStartLargeHopperMotor := FALSE;
    END_IF;
    bReverseLargeHopperMotor := FALSE;
END_IF;

// Manual override
IF bManualMode AND bRunPerm THEN
    IF bHMI_SmallHopperStart THEN bStartSmallHopperMotor := TRUE; END_IF;
    IF bHMI_SmallHopperStop  THEN bStartSmallHopperMotor := FALSE; END_IF;
    bReverseSmallHopperMotor := bHMI_SmallHopperReverse;

    IF bHMI_LargeHopperStart THEN bStartLargeHopperMotor := TRUE; END_IF;
    IF bHMI_LargeHopperStop  THEN bStartLargeHopperMotor := FALSE; END_IF;
    bReverseLargeHopperMotor := bHMI_LargeHopperReverse;
END_IF;

// Safety fallback
IF NOT bRunPerm THEN
    bStartSmallHopperMotor := FALSE;
    bReverseSmallHopperMotor := FALSE;
    bStartLargeHopperMotor := FALSE;
    bReverseLargeHopperMotor := FALSE;
END_IF;


PROGRAM PRG_BottleDescrambler
VAR
    bRunPerm      : BOOL;
    bSmallNeedFeed: BOOL;
    bLargeNeedFeed: BOOL;
    prevSmallJam  : BOOL := FALSE;
    prevLargeJam  : BOOL := FALSE;
END_VAR

// Safety permissives
bRunPerm := bMCR_PowerOn AND bSorterDoorClosed AND bCapperDoorClosed AND bAirSupplyOK;

// Feed demand (track not full)
bSmallNeedFeed := NOT bSmallBottleTrackHighLevel;
bLargeNeedFeed := NOT bLargeBottleTrackHighLevel;

// AUTO
IF bAutoMode AND NOT bManualMode AND bRunPerm THEN
    // Normal forward run
    bStartSmallDescrambler := bSmallNeedFeed AND NOT bSmallDescrFault;
    bStartLargeDescrambler := bLargeNeedFeed AND NOT bLargeDescrFault;

    bReverseSmallDescrambler := FALSE;
    bReverseLargeDescrambler := FALSE;

    // --- Jam handling with retry counter ---
    // Small
    IF (bSmallDescrJam AND NOT prevSmallJam) THEN
        nSmallDescrJamCount := nSmallDescrJamCount + 1;
        IF nSmallDescrJamCount >= nMaxDescrJamRetries THEN
            bSmallDescrFault := TRUE;
        ELSE
            tSmallDescrRevPulseTmr(IN := TRUE, PT := UDINT_TO_TIME(nDescrRevPulse_ms));
        END_IF;
    END_IF;

    IF tSmallDescrRevPulseTmr.IN AND NOT tSmallDescrRevPulseTmr.Q THEN
        bStartSmallDescrambler := FALSE;
        bReverseSmallDescrambler := TRUE;
    ELSE
        tSmallDescrRevPulseTmr(IN := FALSE);
    END_IF;

    // Large
    IF (bLargeDescrJam AND NOT prevLargeJam) THEN
        nLargeDescrJamCount := nLargeDescrJamCount + 1;
        IF nLargeDescrJamCount >= nMaxDescrJamRetries THEN
            bLargeDescrFault := TRUE;
        ELSE
            tLargeDescrRevPulseTmr(IN := TRUE, PT := UDINT_TO_TIME(nDescrRevPulse_ms));
        END_IF;
    END_IF;

    IF tLargeDescrRevPulseTmr.IN AND NOT tLargeDescrRevPulseTmr.Q THEN
        bStartLargeDescrambler := FALSE;
        bReverseLargeDescrambler := TRUE;
    ELSE
        tLargeDescrRevPulseTmr(IN := FALSE);
    END_IF;
END_IF;

// Manual
IF bManualMode AND bRunPerm THEN
    IF bHMI_SmallDescrStart THEN bStartSmallDescrambler := TRUE; END_IF;
    IF bHMI_SmallDescrStop  THEN bStartSmallDescrambler := FALSE; END_IF;
    bReverseSmallDescrambler := bHMI_SmallDescrReverse;

    IF bHMI_LargeDescrStart THEN bStartLargeDescrambler := TRUE; END_IF;
    IF bHMI_LargeDescrStop  THEN bStartLargeDescrambler := FALSE; END_IF;
    bReverseLargeDescrambler := bHMI_LargeDescrReverse;

    // Cancel jam timers in manual
    tSmallDescrRevPulseTmr(IN := FALSE);
    tLargeDescrRevPulseTmr(IN := FALSE);
END_IF;

// Safety fallback
IF NOT bRunPerm THEN
    bStartSmallDescrambler := FALSE;
    bReverseSmallDescrambler := FALSE;
    bStartLargeDescrambler := FALSE;
    bReverseLargeDescrambler := FALSE;
    tSmallDescrRevPulseTmr(IN := FALSE);
    tLargeDescrRevPulseTmr(IN := FALSE);
END_IF;

// Fault reset
IF bHMI_ResetFault AND bCycleStartOneShot THEN
    bSmallDescrFault := FALSE;
    bLargeDescrFault := FALSE;
    nSmallDescrJamCount := 0;
    nLargeDescrJamCount := 0;
END_IF;

// Remember jam edges
prevSmallJam := bSmallDescrJam;
prevLargeJam := bLargeDescrJam;

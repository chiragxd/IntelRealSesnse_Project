PROGRAM _20_BottleEscapement
VAR
    // Timers
    tSmallHopperNotFullDelayTmr, tSmallHopperFullDelayTmr : TON;
    tLargeHopperNotFullDelayTmr, tLargeHopperFullDelayTmr : TON;
    tSmallDescrRevPulseTmr, tSmallDescrFwdPulseTmr : TON;
    tLargeDescrRevPulseTmr, tLargeDescrFwdPulseTmr : TON;
    tEscapementBrakeDelayTmr : TON;
    tLabelerTimeoutTmr : TON;

    // Jam tracking
    prevSmallJam, prevLargeJam : BOOL;
END_VAR

// ---------------- Hopper + Descrambler ----------------
IF bOptifillMachineInAutoMode THEN

    // Small Hopper
    IF (NOT bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        bStartSmallBottleHopperMotor := TRUE;
        bStartSmallBottleUnscramblerMotor := TRUE;
        bReverseSmallBottleUnscramblerMotor := FALSE;
    ELSIF (NOT bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        tSmallHopperNotFullDelayTmr(IN:=TRUE, PT:=T#5S);
        IF tSmallHopperNotFullDelayTmr.Q THEN bStartSmallBottleHopperMotor := TRUE; END_IF;
        bStartSmallBottleUnscramblerMotor := TRUE;
    ELSIF (bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        tSmallHopperFullDelayTmr(IN:=TRUE, PT:=T#5S);
        IF tSmallHopperFullDelayTmr.Q THEN
            bStartSmallBottleHopperMotor := FALSE;
            bStartSmallBottleUnscramblerMotor := FALSE;
        END_IF;
    ELSIF (bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        IF (bSmallDescrJam AND NOT prevSmallJam) THEN
            nSmallDescrJamCount := nSmallDescrJamCount + 1;
            IF nSmallDescrJamCount >= nMaxDescrJamRetries THEN
                bSmallDescrFault := TRUE;
            ELSE
                tSmallDescrRevPulseTmr(IN:=TRUE, PT:=T#5S);
                tSmallDescrFwdPulseTmr(IN:=TRUE, PT:=T#10S);
            END_IF;
        END_IF;
        IF tSmallDescrRevPulseTmr.IN AND NOT tSmallDescrRevPulseTmr.Q THEN
            bReverseSmallBottleUnscramblerMotor := TRUE;
        ELSIF tSmallDescrFwdPulseTmr.IN AND NOT tSmallDescrFwdPulseTmr.Q THEN
            bStartSmallBottleUnscramblerMotor := TRUE;
            bReverseSmallBottleUnscramblerMotor := FALSE;
        END_IF;
    END_IF;

    // Large Hopper
    IF (NOT bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        bStartLargeBottleHopperMotor := TRUE;
        bStartLargeBottleUnscramblerMotor := TRUE;
        bReverseLargeBottleUnscramblerMotor := FALSE;
    ELSIF (NOT bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        tLargeHopperNotFullDelayTmr(IN:=TRUE, PT:=T#5S);
        IF tLargeHopperNotFullDelayTmr.Q THEN bStartLargeBottleHopperMotor := TRUE; END_IF;
        bStartLargeBottleUnscramblerMotor := TRUE;
    ELSIF (bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        tLargeHopperFullDelayTmr(IN:=TRUE, PT:=T#5S);
        IF tLargeHopperFullDelayTmr.Q THEN
            bStartLargeBottleHopperMotor := FALSE;
            bStartLargeBottleUnscramblerMotor := FALSE;
        END_IF;
    ELSIF (bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        IF (bLargeDescrJam AND NOT prevLargeJam) THEN
            nLargeDescrJamCount := nLargeDescrJamCount + 1;
            IF nLargeDescrJamCount >= nMaxDescrJamRetries THEN
                bLargeDescrFault := TRUE;
            ELSE
                tLargeDescrRevPulseTmr(IN:=TRUE, PT:=T#5S);
                tLargeDescrFwdPulseTmr(IN:=TRUE, PT:=T#10S);
            END_IF;
        END_IF;
        IF tLargeDescrRevPulseTmr.IN AND NOT tLargeDescrRevPulseTmr.Q THEN
            bReverseLargeBottleUnscramblerMotor := TRUE;
        ELSIF tLargeDescrFwdPulseTmr.IN AND NOT tLargeDescrFwdPulseTmr.Q THEN
            bStartLargeBottleUnscramblerMotor := TRUE;
            bReverseLargeBottleUnscramblerMotor := FALSE;
        END_IF;
    END_IF;

END_IF;

// ---------------- Escapement ----------------
CASE eEscapementState OF
    Idle:
        bEscapementDiskAtDrop := TRUE;

    LoadSmall:
        bReleaseEscapementBrake := TRUE;
        bRotateEscapementCCW := TRUE;
        tEscapementBrakeDelayTmr(IN:=TRUE, PT:=T#2S);

    LoadLarge:
        bReleaseEscapementBrake := TRUE;
        bRotateEscapementCW := TRUE;
        tEscapementBrakeDelayTmr(IN:=TRUE, PT:=T#2S);

    Drop:
        bAirCushion := TRUE;
        IF bSmallBottleSelected THEN
            bRaiseLiftPlate := TRUE;
        ELSE
            bLowerLiftPlate := TRUE;
        END_IF;
        bRotateEscapementToDrop := TRUE;
        IF bBottleAtLabeler THEN
            bAirCushion := FALSE;
            bExtendPinchRoller := TRUE;
            bStartBottleRoller := TRUE;
            eEscapementState := Complete;
        END_IF;

    Complete:
        bReleaseEscapementBrake := FALSE;
        bExtendPinchRoller := FALSE;
        bStartBottleRoller := FALSE;
END_CASE;

// ---------------- Labeler ----------------
IF eEscapementState = Complete THEN
    IF bPrinterFault OR bNoLabelDetected OR bNoRibbonDetected THEN
        bLabelerFault := TRUE;
    ELSE
        bFanMotor := TRUE;
        IF bPrinterDataReady AND bLabelerInPosition THEN
            bDispenseLabel := TRUE;
            tLabelerTimeoutTmr(IN:=TRUE, PT:=T#5S);
            IF tLabelerTimeoutTmr.Q THEN
                bRejectBottle := TRUE;
            END_IF;
        END_IF;
    END_IF;
END_IF;

// ---------------- Jam reset ----------------
prevSmallJam := bSmallDescrJam;
prevLargeJam := bLargeDescrJam;

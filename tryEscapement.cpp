PROGRAM _20_BottleEscapement
VAR
    // Minimal internals (new)
    nEscState        : INT := 0;   // 0 Idle, 10 Pick, 20 Prep, 30 Drop, 40 Complete
    nLabelerState    : INT := 0;   // 0 Idle, 10 WaitData, 20 Print, 30 Done
    tEscStep, tLabelerStep : TON;

    // Reuse existing timers you already had for hopper/descrambler
    //   tSmallHopperNotFullDelayTmr, tSmallHopperFullDelayTmr : TON;
    //   tLargeHopperNotFullDelayTmr, tLargeHopperFullDelayTmr : TON;
    //   tSmallDescrRevPulseTmr, tSmallDescrFwdPulseTmr : TON;
    //   tLargeDescrRevPulseTmr, tLargeDescrFwdPulseTmr : TON;

    // Jam edge memory (reuse if already declared; else keep)
    prevSmallJam, prevLargeJam : BOOL;
END_VAR

// -------------------------------
// MANUAL MODE
// -------------------------------
IF bOptifillMachineInManualMode AND NOT bOptifillMachineInAutoMode THEN
    // Hoppers
    bStartSmallBottleHopperMotor := bStartSmallBottleHopperMotorHMIPB;
    bStartLargeBottleHopperMotor := bStartLargeBottleHopperMotorHMIPB;

    // Descramblers: Forward / Reverse
    bStartSmallBottleUnscramblerMotor := bStartSmallBottleUnscramblerMotorHMIPB;
    bStartLargeBottleUnscramblerMotor := bStartLargeBottleUnscramblerMotorHMIPB;
    bReverseSmallBottleUnscramblerMotor := bStartSmallBottleDescramblerMotorHMIPB;
    bReverseLargeBottleUnscramblerMotor := bStartLargeBottleDescramblerMotorHMIPB;

    // Escapement & Labeler can be wired to your existing HMI pass-throughs the same way, if present.

    // Hold sequencers idle in manual
    nEscState := 0;
    nLabelerState := 0;
END_IF

// -------------------------------
// AUTO MODE
// -------------------------------
IF bOptifillMachineInAutoMode AND NOT bOptifillMachineInManualMode THEN

    // ========= HOPPER: SMALL =========
    IF (NOT bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        bStartSmallBottleHopperMotor := TRUE;
        bStartSmallBottleUnscramblerMotor := TRUE;
        bReverseSmallBottleUnscramblerMotor := FALSE;

    ELSIF (NOT bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        tSmallHopperNotFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tSmallHopperNotFullDelayTmr.Q THEN
            bStartSmallBottleHopperMotor := TRUE;
        END_IF;
        bStartSmallBottleUnscramblerMotor := TRUE;
        bReverseSmallBottleUnscramblerMotor := FALSE;

    ELSIF (bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        tSmallHopperFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tSmallHopperFullDelayTmr.Q THEN
            bStartSmallBottleHopperMotor := FALSE;
            bStartSmallBottleUnscramblerMotor := FALSE;
        END_IF;
        bReverseSmallBottleUnscramblerMotor := FALSE;

    ELSIF (bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        // Jam sequence: reverse Xs, then forward Ys
        IF (bSmallDescrJam AND NOT prevSmallJam) THEN
            nSmallDescrJamCount := nSmallDescrJamCount + 1;
            IF nSmallDescrJamCount >= nMaxDescrJamRetries THEN
                bSmallDescrFault := TRUE;
            ELSE
                tSmallDescrRevPulseTmr(IN := TRUE, PT := T#5S);
                tSmallDescrFwdPulseTmr(IN := TRUE, PT := T#10S);
            END_IF;
        END_IF;

        IF tSmallDescrRevPulseTmr.IN AND NOT tSmallDescrRevPulseTmr.Q THEN
            bStartSmallBottleUnscramblerMotor := FALSE;
            bReverseSmallBottleUnscramblerMotor := TRUE;
        ELSIF tSmallDescrFwdPulseTmr.IN AND NOT tSmallDescrFwdPulseTmr.Q THEN
            bReverseSmallBottleUnscramblerMotor := FALSE;
            bStartSmallBottleUnscramblerMotor := TRUE;
        ELSE
            tSmallDescrRevPulseTmr(IN := FALSE);
            tSmallDescrFwdPulseTmr(IN := FALSE);
        END_IF;
    END_IF;

    // ========= HOPPER: LARGE =========
    IF (NOT bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        bStartLargeBottleHopperMotor := TRUE;
        bStartLargeBottleUnscramblerMotor := TRUE;
        bReverseLargeBottleUnscramblerMotor := FALSE;

    ELSIF (NOT bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        tLargeHopperNotFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tLargeHopperNotFullDelayTmr.Q THEN
            bStartLargeBottleHopperMotor := TRUE;
        END_IF;
        bStartLargeBottleUnscramblerMotor := TRUE;
        bReverseLargeBottleUnscramblerMotor := FALSE;

    ELSIF (bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        tLargeHopperFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tLargeHopperFullDelayTmr.Q THEN
            bStartLargeBottleHopperMotor := FALSE;
            bStartLargeBottleUnscramblerMotor := FALSE;
        END_IF;
        bReverseLargeBottleUnscramblerMotor := FALSE;

    ELSIF (bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        IF (bLargeDescrJam AND NOT prevLargeJam) THEN
            nLargeDescrJamCount := nLargeDescrJamCount + 1;
            IF nLargeDescrJamCount >= nMaxDescrJamRetries THEN
                bLargeDescrFault := TRUE;
            ELSE
                tLargeDescrRevPulseTmr(IN := TRUE, PT := T#5S);
                tLargeDescrFwdPulseTmr(IN := TRUE, PT := T#10S);
            END_IF;
        END_IF;

        IF tLargeDescrRevPulseTmr.IN AND NOT tLargeDescrRevPulseTmr.Q THEN
            bStartLargeBottleUnscramblerMotor := FALSE;
            bReverseLargeBottleUnscramblerMotor := TRUE;
        ELSIF tLargeDescrFwdPulseTmr.IN AND NOT tLargeDescrFwdPulseTmr.Q THEN
            bReverseLargeBottleUnscramblerMotor := FALSE;
            bStartLargeBottleUnscramblerMotor := TRUE;
        ELSE
            tLargeDescrRevPulseTmr(IN := FALSE);
            tLargeDescrFwdPulseTmr(IN := FALSE);
        END_IF;
    END_IF;

    // ========= ESCAPEMENT =========
    CASE nEscState OF
        0: // Idle at drop; wait for order & empty labeler
            bReleaseEscapementBrake := FALSE;
            bRotateEscapementToSmallPickup := FALSE;
            bRotateEscapementToLargePickup := FALSE;
            bRotateEscapementToDropPosition := FALSE;
            IF NOT bBottlePresentAtLabeler THEN
                nEscState := 10;
            END_IF;

        10: // Move to pickup by size (CCW small / CW large per your machine)
            bReleaseEscapementBrake := TRUE;
            IF bSmallBottleSelected THEN
                bRotateEscapementToSmallPickup := TRUE;
                bRotateEscapementToLargePickup := FALSE;
            ELSE
                bRotateEscapementToSmallPickup := FALSE;
                bRotateEscapementToLargePickup := TRUE;
            END_IF;
            tEscStep(IN := NOT bEscapementDiskAtLoadPosition, PT := T#3S);
            IF bEscapementDiskAtLoadPosition OR tEscStep.Q THEN
                bRotateEscapementToSmallPickup := FALSE;
                bRotateEscapementToLargePickup := FALSE;
                nEscState := 20;
                tEscStep(IN := FALSE);
            END_IF;

        20: // Prep: air cushion + plate per size
            bAirCushion := TRUE;
            bRaiseLiftPlate := bSmallBottleSelected;
            bLowerLiftPlate := NOT bSmallBottleSelected;
            nEscState := 30;

        30: // Rotate to drop; wait bottle arrives at labeler sensor
            bRotateEscapementToDropPosition := TRUE;
            IF bEscapementDiskAtDropPosition THEN
                // keep cushion until bottle seen at labeler
                IF bBottlePresentAtLabeler THEN
                    bAirCushion := FALSE;
                    bRotateEscapementToDropPosition := FALSE;
                    nEscState := 40;
                END_IF;
            END_IF;

        40: // Clamp & roller start; cycle complete
            bExtendPinchRoller := TRUE;
            bStartBottleRoller := TRUE;
            bRetractPinchRoller := FALSE;
            bReleaseEscapementBrake := FALSE;
            nEscState := 0;
    END_CASE;

    // ========= LABELER =========
    CASE nLabelerState OF
        0: // Idle → start when escapement has just finished clamp/start
            IF (nEscState = 0) AND bExtendPinchRoller AND bStartBottleRoller THEN
                bFanMotor := TRUE;
                nLabelerState := 10;
            END_IF;

        10: // Wait for data & position
            IF bPrinterFault OR bNoLabelDetected OR bNoRibbonDetected THEN
                bLabelerFault := TRUE;
                nLabelerState := 0;
            ELSIF bPrinterDataReady AND bLabelerInPrintPosition THEN
                bDispenseLabel := TRUE;
                tLabelerStep(IN := TRUE, PT := T#5S); // end-print timeout
                nLabelerState := 20;
            END_IF;

        20: // Printing / verify
            IF bEndPrint THEN
                bDispenseLabel := FALSE;
                tLabelerStep(IN := FALSE);
                nLabelerState := 30;
            ELSIF tLabelerStep.Q THEN
                bRejectBottle := TRUE; // timeout path
                bDispenseLabel := FALSE;
                tLabelerStep(IN := FALSE);
                nLabelerState := 30;
            END_IF;

        30: // Done; release clamp if your downstream allows
            // (Optionally retract pinch / stop roller elsewhere when transfer starts)
            nLabelerState := 0;
    END_CASE;

END_IF

// -------------------------------
// Housekeeping
// -------------------------------
prevSmallJam := bSmallDescrJam;
prevLargeJam := bLargeDescrJam;

PROGRAM PRG_BottleSorter
VAR
    iSorterStep     : INT := 0;
    tCmdPulse       : TON;
    tInPosTimeout   : TON;
    tGateDelay      : TON;
    iTargetLanePrev : INT := -1;
    bSorterPermissive : BOOL;
END_VAR

// ---------------------
// Safety permissive
// ---------------------
bSorterPermissive :=
    bControlPowerAndAirOn AND
    bSafetyOK AND
    bSorterDoorClosed AND
    bCapperDoorClosed AND
    bSorterServoReady AND
    NOT bSorterServoAlarm;

// ---------------------
// Manual Controls
// ---------------------
IF bOptifillMachineInManualMode THEN
    // Manual home
    bHomeBottleSorter := bManualHomeSorter;

    // Lane select
    IF    bManualIndexToLane1 THEN iTargetLane := 1;
    ELSIF bManualIndexToLane2 THEN iTargetLane := 2;
    ELSIF bManualIndexToLane3 THEN iTargetLane := 3;
    ELSIF bManualIndexToLane4 THEN iTargetLane := 4;
    ELSIF bManualIndexToLane5 THEN iTargetLane := 5;
    ELSIF bManualIndexToLane6 THEN iTargetLane := 6;
    ELSIF bManualIndexToLane7 THEN iTargetLane := 7;
    END_IF;

    // Manual gate
    bOpenSorterGateOutput  := bManualOpenSorterGate;
    bCloseSorterGateOutput := bManualCloseSorterGate;
END_IF;

// ---------------------
// Lane Command Encode
// ---------------------
bSorterServoPositionCmdBit1 := (iTargetLane AND 1) <> 0;
bSorterServoPositionCmdBit2 := (iTargetLane AND 2) <> 0;
bSorterServoPositionCmdBit3 := (iTargetLane AND 4) <> 0;

// ---------------------
// AUTO MODE
// ---------------------
IF bOptifillMachineInAutoMode AND bAutoSequenceEnable THEN
    CASE iSorterStep OF
        0: // Idle → Issue move if lane changed
            IF (iTargetLane <> iTargetLanePrev) AND bSorterPermissive AND NOT bSorterServoMoving THEN
                bSorterServoOperationMode := FALSE;
                bSorterServoJISE := FALSE;
                bSorterServoCSTR_PWRT := TRUE;
                tCmdPulse(IN := TRUE, PT := T#150ms);
                tInPosTimeout(IN := TRUE, PT := T#10s);
                iSorterStep := 10;
            END_IF;

        10: // Wait in-position
            bSorterServoCSTR_PWRT := FALSE;
            IF bSorterServoInPosition AND bBottleSorterLaneInPosition THEN
                tInPosTimeout(IN := FALSE);
                iSorterCurrentLane := iTargetLane;
                iSorterStep := 20;
            ELSIF tInPosTimeout.Q THEN
                iSorterStep := 900;
            END_IF;

        20: // Open gate
            IF bSorterPermissive THEN
                bOpenSorterGateOutput  := TRUE;
                bCloseSorterGateOutput := FALSE;
                tGateDelay(IN := TRUE, PT := T#2000ms);
                iSorterStep := 30;
            END_IF;

        30: // Close gate
            IF bSorterGateOpen OR tGateDelay.Q THEN
                bOpenSorterGateOutput  := FALSE;
                bCloseSorterGateOutput := TRUE;
                tGateDelay(IN := TRUE, PT := T#2000ms);
                iSorterStep := 40;
            END_IF;

        40: // Confirm closed
            IF bSorterGateClosed THEN
                bCloseSorterGateOutput := FALSE;
                tGateDelay(IN := FALSE);
                iSorterStep := 0;
            ELSIF tGateDelay.Q THEN
                iSorterStep := 900;
            END_IF;

        900: // Fault
            bSorterServoCSTR_PWRT := FALSE;
            bOpenSorterGateOutput := FALSE;
            bCloseSorterGateOutput := FALSE;
            tGateDelay(IN := FALSE);
            tInPosTimeout(IN := FALSE);
    END_CASE;
END_IF;

iTargetLanePrev := iTargetLane;

PROGRAM BottleSorter
VAR
    // Internal state machine
    iSorterStep : INT := 0;
    tPosTimeout : TON;
    tGateDelay  : TON;
END_VAR

// =============================
// AUTO MODE – Step Machine
// =============================
IF bAutoMode THEN
    CASE iSorterStep OF
        0: // Idle
            bSorterLaneSelected := FALSE;
            bSorterSequenceFault := FALSE;

            IF iTargetLane <> iSorterCurrentLane THEN
                // Command servo position
                bSorterServoPositionCmdBit1 := (iTargetLane AND 1) = 1;
                bSorterServoPositionCmdBit2 := ((iTargetLane >> 1) AND 1) = 1;
                bSorterServoPositionCmdBit3 := ((iTargetLane >> 2) AND 1) = 1;
                bSorterServoCSTR_PWRT := TRUE; // pulse
                tPosTimeout(IN := TRUE, PT := T#10s);
                iSorterStep := 10;
            END_IF;

        10: // Wait in position
            bSorterServoCSTR_PWRT := FALSE;

            IF bSorterServoInPosition THEN
                iSorterCurrentLane := iTargetLane;
                bSorterLaneSelected := TRUE;
                tPosTimeout(IN := FALSE);
                iSorterStep := 20;
            ELSIF tPosTimeout.Q THEN
                bSorterSequenceFault := TRUE;
                iSorterStep := 900;
            END_IF;

        20: // Open gate
            IF bSorterLaneInPosition THEN
                bOpenSorterGateOutput := TRUE;
                bCloseSorterGateOutput := FALSE;
                tGateDelay(IN := TRUE, PT := T#2000ms);
                iSorterStep := 30;
            END_IF;

        30: // Confirm open then close
            IF bSorterGateOpen OR tGateDelay.Q THEN
                bOpenSorterGateOutput := FALSE;
                bCloseSorterGateOutput := TRUE;
                iSorterStep := 40;
                tGateDelay(IN := FALSE);
            END_IF;

        40: // Confirm closed
            IF bSorterGateClosed THEN
                bCloseSorterGateOutput := FALSE;
                iSorterStep := 0;
            END_IF;

        900: // Fault
            bSorterServoCSTR_PWRT := FALSE;
            bOpenSorterGateOutput := FALSE;
            bCloseSorterGateOutput := FALSE;

            IF bResetEnable THEN
                bSorterSequenceFault := FALSE;
                iSorterStep := 0;
            END_IF;
    END_CASE;
END_IF

// =============================
// MANUAL MODE – Direct Control
// =============================
IF bManualMode THEN
    // Manual lane select
    IF bManualIndexToLane1 THEN iTargetLane := 1; END_IF;
    IF bManualIndexToLane2 THEN iTargetLane := 2; END_IF;
    IF bManualIndexToLane3 THEN iTargetLane := 3; END_IF;
    IF bManualIndexToLane4 THEN iTargetLane := 4; END_IF;
    IF bManualIndexToLane5 THEN iTargetLane := 5; END_IF;
    IF bManualIndexToLane6 THEN iTargetLane := 6; END_IF;
    IF bManualIndexToLane7 THEN iTargetLane := 7; END_IF;

    // Jog forward/reverse
    IF bSorterServoJogForward THEN
        bSorterServoJogForward := TRUE;
    ELSIF bSorterServoJogReverse THEN
        bSorterServoJogReverse := TRUE;
    ELSE
        bSorterServoJogForward := FALSE;
        bSorterServoJogReverse := FALSE;
    END_IF;

    // Manual gate
    IF bManualOpenSorterGate THEN
        bOpenSorterGateOutput := TRUE;
        bCloseSorterGateOutput := FALSE;
    ELSIF bManualCloseSorterGate THEN
        bCloseSorterGateOutput := TRUE;
        bOpenSorterGateOutput := FALSE;
    ELSE
        bOpenSorterGateOutput := FALSE;
        bCloseSorterGateOutput := FALSE;
    END_IF;
END_IF

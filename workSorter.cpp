PROGRAM BottleSorter
VAR
    // Internal step machine
    iSorterStep : INT := 0; // New internal step tracker
    tPosTimeout : TON;      // Positioning timeout
END_VAR

// ======================
// Bottle Sorter State Machine
// ======================
CASE iSorterStep OF
    0: // Idle
        bSorterLaneSelected := FALSE;
        bSorterSequenceFault := FALSE;

        IF iTargetLane <> iSorterCurrentLane THEN
            // Command servo
            bSorterServoPositionCmdBit1 := (iTargetLane AND 1) = 1;
            bSorterServoPositionCmdBit2 := ((iTargetLane >> 1) AND 1) = 1;
            bSorterServoPositionCmdBit3 := ((iTargetLane >> 2) AND 1) = 1;
            bSorterServoCSTR_PWRT := TRUE; // pulse
            tPosTimeout(IN := TRUE, PT := T#10s);
            iSorterStep := 10;
        END_IF;

    // Wait for in-position
    10:
        bSorterServoCSTR_PWRT := FALSE; // clear pulse

        IF bSorterServoInPosition THEN
            iSorterCurrentLane := iTargetLane;
            bSorterLaneSelected := TRUE;
            tPosTimeout(IN := FALSE);
            iSorterStep := 20;
        ELSIF tPosTimeout.Q THEN
            bSorterSequenceFault := TRUE;
            iSorterStep := 900;
        END_IF;

    // Open gate to drop bottle
    20:
        IF bSorterLaneInPosition THEN
            bOpenSorterGateOutput := TRUE;
            bCloseSorterGateOutput := FALSE;
            iSorterStep := 30;
        END_IF;

    // Confirm open then close
    30:
        IF bSorterGateOpen THEN
            bOpenSorterGateOutput := FALSE;
            bCloseSorterGateOutput := TRUE;
            iSorterStep := 40;
        END_IF;

    // Confirm closed
    40:
        IF bSorterGateClosed THEN
            bCloseSorterGateOutput := FALSE;
            iSorterStep := 0; // complete cycle
        END_IF;

    // Fault
    900:
        bSorterServoCSTR_PWRT := FALSE;
        bOpenSorterGateOutput := FALSE;
        bCloseSorterGateOutput := FALSE;

        IF bResetEnable THEN
            bSorterSequenceFault := FALSE;
            iSorterStep := 0;
        END_IF;
END_CASE;

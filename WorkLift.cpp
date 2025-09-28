PROGRAM BottleLift
VAR
    // Internal step machine
    iLiftStep : INT := 0; // New internal step tracker
END_VAR

// ======================
// Bottle Lift State Machine
// ======================
CASE iLiftStep OF
    0: // Idle
        bBottleLiftRaising := FALSE;
        bBottleLiftLowering := FALSE;
        bBottleLiftReady := TRUE;

        IF bCloseLiftGripperReq THEN
            bCloseBottleLiftGripper := TRUE;
            bOpenBottleLiftGripper := FALSE;
            iLiftStep := 10;
        END_IF;

    // Close gripper
    10:
        IF NOT bLiftGripperOpenAtCapper THEN
            iLiftStep := 20;
        END_IF;

    // Raise fast until slowdown
    20:
        bBottleLiftRaising := TRUE;
        bRaiseBottleLiftFast := TRUE;
        bRaiseBottleLiftSlow := FALSE;

        IF bBottleLiftAtSpeedTransition THEN
            bRaiseBottleLiftFast := FALSE;
            bRaiseBottleLiftSlow := TRUE;
            iLiftStep := 30;
        END_IF;

    // Fully up
    30:
        IF bBottleLiftUp THEN
            bRaiseBottleLiftSlow := FALSE;
            iLiftStep := 40;
        END_IF;

    // Wait for sorter lane ready
    40:
        IF bSorterLaneInPosition AND (bSorterLaneEmpty OR (iTargetLane = 7)) THEN
            bOpenBottleLiftGripper := TRUE;
            bCloseBottleLiftGripper := FALSE;
            iLiftStep := 50;
        END_IF;

    // Confirm gripper open
    50:
        IF bLiftGripperOpenAtSorter THEN
            iLiftStep := 60;
        END_IF;

    // Lower fast until slowdown
    60:
        bBottleLiftLowering := TRUE;
        bLowerBottleLiftFast := TRUE;
        bLowerBottleLiftSlow := FALSE;

        IF bBottleLiftAtSpeedTransition THEN
            bLowerBottleLiftFast := FALSE;
            bLowerBottleLiftSlow := TRUE;
            iLiftStep := 70;
        END_IF;

    // Fully down
    70:
        IF bBottleLiftDown THEN
            bLowerBottleLiftSlow := FALSE;
            bBottleLiftLowering := FALSE;
            bBottleLiftReady := TRUE;
            iLiftStep := 0;
        END_IF;
END_CASE;

PROGRAM BottleLift
VAR
    // Internal state machine
    iLiftStep : INT := 0;
    tGripOpenDelay : TON;
    tRaisedDelay   : TON;
END_VAR

// =============================
// AUTO MODE – Step Machine
// =============================
IF bAutoMode THEN
    CASE iLiftStep OF
        0: // Idle
            bBottleLiftReady := TRUE;
            bBottleLiftRaising := FALSE;
            bBottleLiftLowering := FALSE;

            IF bCloseLiftGripperReq THEN
                bCloseBottleLiftGripper := TRUE;
                bOpenBottleLiftGripper := FALSE;
                iLiftStep := 10;
            END_IF;

        10: // Close gripper
            IF NOT bLiftGripperOpenAtCapper THEN
                iLiftStep := 20;
            END_IF;

        20: // Raise fast → slow
            bBottleLiftRaising := TRUE;
            bRaiseBottleLiftFast := NOT bBottleLiftAtSpeedTransition;
            bRaiseBottleLiftSlow := bBottleLiftAtSpeedTransition;

            IF bBottleLiftUp THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := FALSE;
                iLiftStep := 30;
            END_IF;

        30: // Wait sorter lane ready
            IF bSorterLaneInPosition AND (bSorterLaneEmpty OR (iTargetLane = 7)) THEN
                bOpenBottleLiftGripper := TRUE;
                bCloseBottleLiftGripper := FALSE;
                tGripOpenDelay(IN := TRUE, PT := T#1000ms);
                iLiftStep := 40;
            END_IF;

        40: // Confirm gripper open
            IF bLiftGripperOpenAtSorter OR tGripOpenDelay.Q THEN
                bBottleLiftRaising := FALSE;
                iLiftStep := 50;
                tGripOpenDelay(IN := FALSE);
            END_IF;

        50: // Lower fast → slow
            bBottleLiftLowering := TRUE;
            bLowerBottleLiftFast := NOT bBottleLiftAtSpeedTransition;
            bLowerBottleLiftSlow := bBottleLiftAtSpeedTransition;

            IF bBottleLiftDown THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := FALSE;
                bBottleLiftLowering := FALSE;
                bBottleLiftReady := TRUE;
                iLiftStep := 0;
            END_IF;

    END_CASE;
END_IF

// =============================
// MANUAL MODE – Direct Control
// =============================
IF bManualMode THEN
    // Jog up/down
    IF bLiftManualRaise THEN
        bRaiseBottleLiftFast := TRUE;
    ELSIF bLiftManualLower THEN
        bLowerBottleLiftFast := TRUE;
    ELSE
        bRaiseBottleLiftFast := FALSE;
        bLowerBottleLiftFast := FALSE;
    END_IF;

    // Gripper open/close
    IF bManualOpenLiftGripper THEN
        bOpenBottleLiftGripper := TRUE;
        bCloseBottleLiftGripper := FALSE;
    ELSIF bManualCloseLiftGripper THEN
        bCloseBottleLiftGripper := TRUE;
        bOpenBottleLiftGripper := FALSE;
    ELSE
        bOpenBottleLiftGripper := FALSE;
        bCloseBottleLiftGripper := FALSE;
    END_IF;
END_IF

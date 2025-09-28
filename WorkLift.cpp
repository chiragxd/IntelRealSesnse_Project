PROGRAM PRG_BottleLift
VAR
    iLiftStep        : INT := 0;
    tRaiseTmo        : TON;
    tLowerTmo        : TON;
    tGripOpenConfirm : TON;
    tSettleDelay     : TON;
    bLiftPermissive  : BOOL;
END_VAR

// ---------------------
// Safety permissive
// ---------------------
bLiftPermissive :=
    bControlPowerAndAirOn AND
    bSafetyOK AND
    bCapperDoorClosed AND
    bSorterDoorClosed AND
    bBottleCapperChuckHeadAtCapEscapement;

// ---------------------
// Manual Controls (HMI gripper)
// ---------------------
IF bOptifillMachineInManualMode THEN
    IF bLiftGripperOpenCmd THEN
        bOpenBottleLiftGripper := TRUE;
        bCloseBottleLiftGripper := FALSE;
    ELSIF bLiftGripperCloseCmd THEN
        bCloseBottleLiftGripper := TRUE;
        bOpenBottleLiftGripper := FALSE;
    END_IF;
END_IF;

// ---------------------
// AUTO MODE
// ---------------------
IF bOptifillMachineInAutoMode AND bAutoSequenceEnable THEN
    CASE iLiftStep OF
        0: // Idle
            IF bBottleAtLift AND bLiftPermissive THEN
                bCloseBottleLiftGripper := TRUE;
                bOpenBottleLiftGripper := FALSE;
                // Raise fast
                bRaiseBottleLiftFast := TRUE;
                tRaiseTmo(IN := TRUE, PT := T#6s);
                iLiftStep := 10;
            END_IF;

        10: // Raising
            IF bBottleLiftAtSpeedTransition THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := TRUE;
            END_IF;

            IF bBottleLiftUp THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := FALSE;
                tRaiseTmo(IN := FALSE);
                IF bBottleSorterLaneInPosition AND (bBottleSorterLaneEmpty OR (iTargetLane = 7)) THEN
                    bOpenBottleLiftGripper := TRUE;
                    bCloseBottleLiftGripper := FALSE;
                    tGripOpenConfirm(IN := TRUE, PT := T#2s);
                    iLiftStep := 20;
                ELSE
                    iLiftStep := 900;
                END_IF;
            ELSIF tRaiseTmo.Q OR NOT bLiftPermissive THEN
                iLiftStep := 900;
            END_IF;

        20: // Confirm gripper open → settle
            IF bBottleLiftGripperOpenAtSorter OR tGripOpenConfirm.Q THEN
                tGripOpenConfirm(IN := FALSE);
                tSettleDelay(IN := TRUE, PT := T#300ms);
                iLiftStep := 30;
            END_IF;

        30: // Lowering
            IF tSettleDelay.Q THEN
                bLowerBottleLiftFast := TRUE;
                tLowerTmo(IN := TRUE, PT := T#6s);
                iLiftStep := 40;
            END_IF;

        40: // Lower with transition
            IF bBottleLiftAtSpeedTransition THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := TRUE;
            END_IF;

            IF bBottleLiftDown THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := FALSE;
                tLowerTmo(IN := FALSE);
                IF bBottleLiftGripperOpenAtCapper THEN
                    iLiftStep := 0;
                END_IF;
            ELSIF tLowerTmo.Q OR NOT bLiftPermissive THEN
                iLiftStep := 900;
            END_IF;

        900: // Fault
            bRaiseBottleLiftFast := FALSE;
            bRaiseBottleLiftSlow := FALSE;
            bLowerBottleLiftFast := FALSE;
            bLowerBottleLiftSlow := FALSE;
            tRaiseTmo(IN := FALSE);
            tLowerTmo(IN := FALSE);
            tGripOpenConfirm(IN := FALSE);
            tSettleDelay(IN := FALSE);
    END_CASE;
END_IF;

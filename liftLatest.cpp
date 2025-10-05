(*
=========================================================
 BOTTLE LIFT AUTO & MANUAL LOGIC  (FINAL VERSION)
=========================================================
*)

VAR
    // Timing
    tRaiseTmo        : TON;
    tLowerTmo        : TON;
    tGripOpenConfirm : TON;
    tSettleDelay     : TON;
    tPostLowerDelay  : TON;

    // Edge detections
    rTrigLiftStart   : R_TRIG;
    rTrigSpeedTrans  : R_TRIG;

    // Internal latch
    bSpeedTransLatched : BOOL;
END_VAR


// =====================================================
//  SAFETY PERMISSIVE
// =====================================================
bLiftPermissive := bSystemPowerOn AND bSafetyOK AND bBottleSorterGateClosed AND bBottleCapperChuckHeadAtCapEscapement;


// =====================================================
//  MANUAL MODE
// =====================================================
IF bOptifillMachineInManualMode AND NOT bOptifillMachineInAutoMode THEN

    // --- Raise / Lower ---
    IF bLiftManualRaise THEN
        bRaiseBottleLiftFast := TRUE;
        bRaiseBottleLiftSlow := FALSE;
    ELSE
        bRaiseBottleLiftFast := FALSE;
        bRaiseBottleLiftSlow := FALSE;
    END_IF;

    IF bLiftManualLower THEN
        bLowerBottleLiftSlow := TRUE;
    ELSE
        bLowerBottleLiftSlow := FALSE;
    END_IF;

    // --- Gripper ---
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

END_IF;



// =====================================================
//  AUTO MODE
// =====================================================
IF bOptifillMachineInAutoMode AND NOT bOptifillMachineInManualMode THEN

    // Edge detect transition and latch it
    rTrigSpeedTrans(CLK := bBottleLiftAtSpeedTransition);
    IF rTrigSpeedTrans.Q THEN
        bSpeedTransLatched := TRUE;
    END_IF;

    CASE iLiftStep OF

        // ------------------------------------------------
        0: // IDLE
            // Reset coils
            bRaiseBottleLiftFast := FALSE;
            bRaiseBottleLiftSlow := FALSE;
            bLowerBottleLiftFast := FALSE;
            bLowerBottleLiftSlow := FALSE;
            bOpenBottleLiftGripper := FALSE;
            bCloseBottleLiftGripper := FALSE;
            bSpeedTransLatched := FALSE;

            // Reset timers
            tRaiseTmo(IN := FALSE);
            tLowerTmo(IN := FALSE);
            tGripOpenConfirm(IN := FALSE);
            tSettleDelay(IN := FALSE);
            tPostLowerDelay(IN := FALSE);

            // Start latch
            rTrigLiftStart(CLK := bBottleAtLift AND bLiftPermissive AND 
                                      bCapPresentOnBottle AND 
                                      bBottleCapperChuckHeadAtCapEscapement AND 
                                      bHandoffComplete AND 
                                      bSorterLaneInPosition);

            IF rTrigLiftStart.Q THEN
                // Start raise fast
                bCloseBottleLiftGripper := TRUE;
                bOpenBottleLiftGripper := FALSE;
                bRaiseBottleLiftFast := TRUE;
                bRaiseBottleLiftSlow := FALSE;
                tRaiseTmo(IN := TRUE, PT := T#6s);
                iLiftStep := 10;
            END_IF;


        // ------------------------------------------------
        10: // RAISING
            // Transition from fast → slow if latched
            IF bSpeedTransLatched THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := TRUE;
            END_IF;

            // Fully up
            IF bBottleLiftUp THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := FALSE;
                tRaiseTmo(IN := FALSE);
                bSpeedTransLatched := FALSE;

                // small settle delay before opening gripper
                tSettleDelay(IN := TRUE, PT := T#300MS);
                iLiftStep := 15;
            ELSIF tRaiseTmo.Q OR NOT bLiftPermissive THEN
                iLiftStep := 900;
            END_IF;


        // ------------------------------------------------
        15: // WAIT TO STABILIZE THEN OPEN GRIPPER
            IF tSettleDelay.Q THEN
                tSettleDelay(IN := FALSE);
                IF bSorterLaneInPosition AND (bBottleSorterLaneEmpty OR (iTargetLane = 7)) THEN
                    bOpenBottleLiftGripper := TRUE;
                    bCloseBottleLiftGripper := FALSE;
                    tGripOpenConfirm(IN := TRUE, PT := T#2s);
                    iLiftStep := 20;
                ELSE
                    // lane not ready → retry next scan
                    iLiftStep := 0;
                END_IF;
            END_IF;


        // ------------------------------------------------
        20: // CONFIRM GRIPPER OPEN THEN LOWER
            IF bBottleLiftGripperOpenAtSorter OR tGripOpenConfirm.Q THEN
                tGripOpenConfirm(IN := FALSE);
                // start lowering
                bLowerBottleLiftFast := TRUE;
                bLowerBottleLiftSlow := FALSE;
                bOpenBottleLiftGripper := TRUE;   // keep open while lowering
                tLowerTmo(IN := TRUE, PT := T#6s);
                bSpeedTransLatched := FALSE;
                iLiftStep := 30;
            END_IF;


        // ------------------------------------------------
        30: // LOWERING
            IF rTrigSpeedTrans.Q THEN
                bSpeedTransLatched := TRUE;
            END_IF;

            IF bSpeedTransLatched THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := TRUE;
            END_IF;

            IF bBottleLiftDown THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := FALSE;
                bOpenBottleLiftGripper := FALSE;
                tLowerTmo(IN := FALSE);
                tPostLowerDelay(IN := TRUE, PT := T#200MS);
                iLiftStep := 40;
            ELSIF tLowerTmo.Q OR NOT bLiftPermissive THEN
                iLiftStep := 900;
            END_IF;


        // ------------------------------------------------
        40: // POST-LOWER SETTLE THEN COMPLETE
            IF tPostLowerDelay.Q THEN
                tPostLowerDelay(IN := FALSE);
                iLiftStep := 0;
            END_IF;


        // ------------------------------------------------
        900: // FAULT HANDLER
            bRaiseBottleLiftFast := FALSE;
            bRaiseBottleLiftSlow := FALSE;
            bLowerBottleLiftFast := FALSE;
            bLowerBottleLiftSlow := FALSE;
            bOpenBottleLiftGripper := FALSE;
            bCloseBottleLiftGripper := FALSE;

            tRaiseTmo(IN := FALSE);
            tLowerTmo(IN := FALSE);
            tGripOpenConfirm(IN := FALSE);
            tSettleDelay(IN := FALSE);
            tPostLowerDelay(IN := FALSE);
            bSpeedTransLatched := FALSE;

            IF bLiftPermissive AND bSorterAlarmReset THEN
                iLiftStep := 0;
            END_IF;

    END_CASE;

END_IF;


// =====================================================
//  STATUS FLAGS
// =====================================================
bLiftBusy := (iLiftStep > 0) AND (iLiftStep < 900);
bLiftDone := (iLiftStep = 0);
bLiftFault := (iLiftStep = 900);

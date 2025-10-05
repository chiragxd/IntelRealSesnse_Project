(*
============================================================
 SORTER AUTO SEQUENCE  (Final Version)
------------------------------------------------------------
 - Assigns lane when transfer gripper picks bottle
 - Rechecks bottle status at handoff complete
 - Uses PC1–PC3 + CSTR (same as manual mode)
 - Lift & gripper handled by BottleLiftLogic
============================================================
*)

VAR
    // Local helpers
    tCSTRPulse      : TON;        // ≥6 ms ON pulse for CSTR
    tPosTO          : TON;        // Position move timeout
    tGateTO         : TON;        // Gate open timeout
    rTrigNewBottle  : R_TRIG;     // Detect new bottle from labeler
    iPM             : INT;        // Current position (decoded bits)
    iPickLane       : INT;        // Selected target lane
END_VAR


// ===========================================================
// DECODE CURRENT SERVO POSITION  (same as manual)
// ===========================================================
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// ===========================================================
// DETECT WHEN TRANSFER GRIPPER PICKS NEW BOTTLE
// ===========================================================
rTrigNewBottle(CLK := bTransferGripperHasBottleFromLabeler);


// ===========================================================
// LANE ASSIGNMENT WHEN NEW BOTTLE ARRIVES
// ===========================================================
IF rTrigNewBottle.Q THEN
    iPickLane := 0;

    IF (bBottleStatus = 1) THEN       // PASS
        // Find first lane (1–6) that is not full
        FOR i := 1 TO 6 DO
            IF aSorterLaneBottleCount[i] < 4 THEN
                iPickLane := i;
                EXIT;
            END_IF;
        END_FOR
        IF iPickLane = 0 THEN
            iPickLane := 1; // fallback if all full
        END_IF;

    ELSE                               // FAIL or REJECT
        iPickLane := 7;                // Lane 7 reserved for rejects
    END_IF;

    // Save target lane
    iTargetLane := USINT(iPickLane);
END_IF;


// ===========================================================
// MAIN SORTER SEQUENCE
// ===========================================================
CASE iSorterStep OF

    // -------------------------------------------------------
    0: // IDLE
        bSorterLaneInPosition := FALSE;
        bOpenSorterGate := FALSE;
        bCloseSorterGate := FALSE;

        // Move only when we have a new bottle and target lane known
        IF bOptifillMachineInAutoMode
           AND bSorterServoDriveReady
           AND bSorterServoHomed
           AND (iTargetLane IN [1..7])
           AND bNewBottleAtSorter THEN
            iSorterStep := 10;
        END_IF;


    // -------------------------------------------------------
    10: // SET POSITION CMD BITS + PULSE CSTR
        bSorterOpMode := FALSE;  // Normal mode
        bSorterJISE := FALSE;    // Not jogging

        bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
        bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
        bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

        // Pulse CSTR to trigger motion
        tCSTRPulse(IN := TRUE, PT := T#10MS);
        bSorterCSTRPWRT := TRUE;
        tPosTO(IN := TRUE, PT := T#4S);

        iSorterStep := 20;


    // -------------------------------------------------------
    20: // WAIT FOR MOVE COMPLETE
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            tCSTRPulse(IN := FALSE);
        END_IF;

        // Successful motion complete
        IF bSorterPENDWEND AND (iPM = INT(iTargetLane)) THEN
            iSorterCurrentLane := iPM;
            bSorterLaneInPosition := TRUE;   // handshake for BottleLift
            tPosTO(IN := FALSE);
            iSorterStep := 30;

        // Move timeout or error
        ELSIF tPosTO.Q THEN
            tPosTO(IN := FALSE);
            iSorterStep := 900;
        END_IF;


    // -------------------------------------------------------
    30: // WAIT FOR LIFT HANDOFF COMPLETE
        // BottleLiftLogic runs independently now
        IF NOT bBottleAtLift AND bBottleLiftDown THEN
            bSorterLaneInPosition := FALSE;

            // Recheck bottle status after placement
            IF bBottleStatus = 1 THEN        // PASS
                IF iTargetLane IN [1..6] THEN
                    aSorterLaneBottleCount[iTargetLane] := aSorterLaneBottleCount[iTargetLane] + 1;
                END_IF;
            ELSE                             // FAIL (after recheck)
                // If it changed mid-cycle, flag for re-sort
                bResortRequired := TRUE;
            END_IF;

            iSorterStep := 40;
        END_IF;


    // -------------------------------------------------------
    40: // CHECK IF LANE FULL OR REJECT
        IF (iTargetLane = 7) THEN
            // Reject lane — always ready to unload
            iSorterStep := 100;

        ELSIF (aSorterLaneBottleCount[iTargetLane] >= 4) THEN
            // Lane full → ready to unload
            iSorterStep := 100;

        ELSE
            // Lane still has capacity — wait for next bottle
            iSorterStep := 0;
        END_IF;


    // -------------------------------------------------------
    100: // OPEN GATE TO UNLOAD (lane full or reject)
        bOpenSorterGate := TRUE;
        bCloseSorterGate := FALSE;
        tGateTO(IN := TRUE, PT := T#1500MS);

        IF bSorterGateOpen THEN
            bOpenSorterGate := FALSE;
            bCloseSorterGate := TRUE;
        END_IF;

        IF bSorterGateClosed THEN
            // Clear that lane for next use
            IF iTargetLane IN [1..6] THEN
                aSorterLaneBottleCount[iTargetLane] := 0;
            END_IF;
            bCloseSorterGate := FALSE;
            tGateTO(IN := FALSE);
            iSorterStep := 0;

        ELSIF tGateTO.Q THEN
            tGateTO(IN := FALSE);
            iSorterStep := 900;
        END_IF;


    // -------------------------------------------------------
    900: // FAULT HANDLER
        bSorterCSTRPWRT := FALSE;
        bOpenSorterGate := FALSE;
        bCloseSorterGate := FALSE;
        bSorterLaneInPosition := FALSE;
        sSorterFaultMessage := 'Sorter motion or gate timeout';
        IF bSorterAlarmReset THEN
            sSorterFaultMessage := '';
            iSorterStep := 0;
        END_IF;

END_CASE;

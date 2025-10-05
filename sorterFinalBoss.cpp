(* =========================
   SORTER AUTO SEQUENCE (ST)
   ========================= *)

(* ---- Local helpers (not in GVL) ---- *)
VAR
    // pulses & timers (local)
    tCSTRPulse  : TON;   // >= 6 ms ON for CSTR (Start)
    tGateTO     : TON;   // gate motion timeout (reuse if you like: 1.5s)
    tPosTO      : TON;   // position timeout (e.g., 4s)
    tLiftDelay  : TON;   // small dwell at top before lowering (e.g., 200 ms)

    rTrigStart  : R_TRIG;
    rTrigUnload : R_TRIG;
    rTrigNewOrd : R_TRIG;

    // feedback decode
    iPM         : INT;   // current position (from Met bits)
    iPickLane   : INT;   // lane chosen by allocator

    // simple latches
    bMoveRequest   : BOOL;
    bMoveComplete  : BOOL;
    bMoveFault     : BOOL;

    // internal
    iOrderQtyForLane : INT;
    bLaneHasTote     : BOOL;
END_VAR


(* ---- Decode servo feedback PM1..PM3 -> iPM (same as manual) ---- *)
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;
(* Manual mapping reference and pulse method kept identical. *)  // :contentReference[oaicite:4]{index=4}


(* ---- Rising edges for control requests ---- *)
rTrigStart(CLK := bCycleStart);
rTrigUnload(CLK := bUnloadSorterRequest);
rTrigNewOrd(CLK := PPS_NewOrder);


(* ===========================================================
   LANE ALLOCATION (good=1..6, reject=7; cap per lane = 4)
   =========================================================== *)
IF bOptifillMachineInAutoMode THEN
    iPickLane := 0;

    // Decide based on bottle status (0=None, >0=valid)
    IF (iBottleStatus = 3) OR (iBottleStatus = 2) THEN
        // Reject (use Lane 7 always)
        iPickLane := 7;

    ELSIF (iBottleStatus = 1) THEN
        // PASS: prefer lane already assigned to current tote with space,
        // else first empty lane 1..6
        // 1) try match tote
        FOR i := 1 TO 6 DO
            IF (aSorterLaneToteID[i] = iToteID) AND (aSorterLaneBottleCount[i] < 4) THEN
                iPickLane := i;
                EXIT;
            END_IF;
        END_FOR

        // 2) if none, try first empty lane
        IF iPickLane = 0 THEN
            FOR i := 1 TO 6 DO
                IF (aSorterLaneBottleCount[i] = 0) THEN
                    // Assign tote to this lane
                    aSorterLaneToteID[i] := iToteID;
                    iPickLane := i;
                    EXIT;
                END_IF;
            END_FOR
        END_IF;
    END_IF;

    // expose chosen target (keeps your HMI current/target regs in sync)
    IF iPickLane <> 0 THEN
        iTargetLane := USINT(iPickLane);   // working var already in your GVL
        iSorterTargetLane := iPickLane;    // optional display/diag
    END_IF;
END_IF;
// (Allocator uses your existing arrays/regs.)  // :contentReference[oaicite:5]{index=5}


(* ===========================================================
   AUTO STATE MACHINE  (iSorterStep from your GVL)
   Keeps EXACT same move-to-position method as manual:
   MODE=LOW, JISE=LOW, set PC1..3, then pulse CSTR ≥6ms.
   =========================================================== *)

CASE iSorterStep OF

    // -------------------------------------------------------
    0:  // IDLE / WAIT FOR CONDITIONS
        bSorterSequenceActive := FALSE;
        bMoveRequest := FALSE;
        bMoveComplete := FALSE;
        bMoveFault := FALSE;

        // Pre-conditions: Auto on, drive ready, homed, have a target
        IF bOptifillMachineInAutoMode
           AND bSorterServoDriveReady
           AND bSorterServoHomed
           AND (iTargetLane IN [1..7]) THEN

            // Request a move as soon as we know target
            bMoveRequest := TRUE;
            iSorterStep := 10;
        END_IF;


    // -------------------------------------------------------
    10: // PREP MOVE (Normal mode, no jogging)
        bSorterSequenceActive := TRUE;
        bSorterOpMode := FALSE;  // Normal
        bSorterJISE   := FALSE;  // Indexing off (not inching)
        // Set PC bits for iTargetLane
        bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
        bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
        bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

        // small safety: make sure CSTR currently LOW
        bSorterCSTRPWRT := FALSE;

        // start CSTR pulse
        tCSTRPulse(IN := TRUE, PT := T#10MS);
        bSorterCSTRPWRT := TRUE;     // OFF->ON edge
        // start position timeout
        tPosTO(IN := TRUE, PT := T#4S);
        iSorterStep := 20;
        // (Exact to your manual method: set bits → CSTR ≥6ms.)  // :contentReference[oaicite:6]{index=6} :contentReference[oaicite:7]{index=7}


    // -------------------------------------------------------
    20: // PULSE & WAIT FOR MOTION COMPLETE
        // finish CSTR pulse
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;         // must be LOW while traveling
            tCSTRPulse(IN := FALSE);
        END_IF;

        // success criteria = PEND/WEND = TRUE AND iPM matches target
        IF bSorterPENDWEND AND (iPM = INT(iTargetLane)) THEN
            bMoveComplete := TRUE;
            tPosTO(IN := FALSE);
            iSorterCurrentLane := iPM;        // update status
            iSorterStep := 100;               // go lift load/unload sequence

        ELSIF tPosTO.Q THEN
            bMoveFault := TRUE;               // pos timeout
            tPosTO(IN := FALSE);
            iSorterStep := 900;               // fault handler
        END_IF;


    // -------------------------------------------------------
    100: // LIFT CYCLE AT LANE (simple load/unload pattern)
        // Preconditions:
        //  - Lane should be in position already (step 20)
        //  - For PASS lanes (1..6): lane must be empty OR contains same tote with <4
        //  - For REJECT lane (7): no count limit
        IF (iTargetLane = 7) THEN
            // reject: just cycle lift to drop into reject lane
            bRaiseBottleLiftFast := TRUE;
            bRaiseBottleLiftSlow := FALSE;
            bLowerBottleLiftFast := FALSE;
            bLowerBottleLiftSlow := FALSE;

            // transition fast->slow
            IF bBottleLiftAtSlowDownPosition THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := TRUE;
            END_IF;

            IF bBottleLiftUp THEN
                // open gripper, dwell, then lower
                bOpenBottleLiftGripper := TRUE;
                tLiftDelay(IN := TRUE, PT := T#200MS);
                IF tLiftDelay.Q THEN
                    bOpenBottleLiftGripper := TRUE;  // keep open while lowering
                    bRaiseBottleLiftSlow := FALSE;
                    bLowerBottleLiftFast := TRUE;
                    tLiftDelay(IN := FALSE);
                END_IF;
            END_IF;

            IF bBottleLiftAtSlowDownPosition THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := TRUE;
            END_IF;

            IF bBottleLiftDown THEN
                // cycle complete for reject
                bLowerBottleLiftSlow := FALSE;
                bOpenBottleLiftGripper := FALSE;  // ready for next
                iSorterStep := 200;               // post-process (counts)
            END_IF;

        ELSE
            // PASS lanes (1..6)
            // Ensure lane is empty or correct tote with space
            bLaneHasTote := (aSorterLaneToteID[iTargetLane] = iToteID);
            IF ( (aSorterLaneBottleCount[iTargetLane] = 0) OR
                 (bLaneHasTote AND (aSorterLaneBottleCount[iTargetLane] < 4)) ) THEN

                // same lift motion as above
                bRaiseBottleLiftFast := TRUE;
                bRaiseBottleLiftSlow := FALSE;
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := FALSE;

                IF bBottleLiftAtSlowDownPosition THEN
                    bRaiseBottleLiftFast := FALSE;
                    bRaiseBottleLiftSlow := TRUE;
                END_IF;

                IF bBottleLiftUp THEN
                    bOpenBottleLiftGripper := TRUE;
                    tLiftDelay(IN := TRUE, PT := T#200MS);
                    IF tLiftDelay.Q THEN
                        bOpenBottleLiftGripper := TRUE;
                        bRaiseBottleLiftSlow := FALSE;
                        bLowerBottleLiftFast := TRUE;
                        tLiftDelay(IN := FALSE);
                    END_IF;
                END_IF;

                IF bBottleLiftAtSlowDownPosition THEN
                    bLowerBottleLiftFast := FALSE;
                    bLowerBottleLiftSlow := TRUE;
                END_IF;

                IF bBottleLiftDown THEN
                    bLowerBottleLiftSlow := FALSE;
                    bOpenBottleLiftGripper := FALSE;
                    iSorterStep := 200;   // post-process (counts/orders)
                END_IF;

            ELSE
                // lane not suitable -> re-allocate next scan
                iSorterStep := 0;
            END_IF;
        END_IF;
        // (Lift/gripper signals reuse your names.)  // :contentReference[oaicite:8]{index=8}


    // -------------------------------------------------------
    200: // POST-PROCESS: UPDATE COUNTS & CHECK UNLOAD
        IF iTargetLane IN [1..6] THEN
            aSorterLaneBottleCount[iTargetLane] := aSorterLaneBottleCount[iTargetLane] + 1;
            // tote assignment if not already set
            IF aSorterLaneToteID[iTargetLane] = 0 THEN
                aSorterLaneToteID[iTargetLane] := iToteID;
            END_IF;
        END_IF;

        // Decide if we should unload lane now:
        //  - If count reaches PPS_Quantity for that tote, or
        //  - If operator/system requested unload (bUnloadSorterRequest)
        iOrderQtyForLane := INT(PPS_Quantity); // using same qty for all of this tote
        IF ( (iTargetLane IN [1..6]) AND
             (aSorterLaneBottleCount[iTargetLane] >= MIN(4, iOrderQtyForLane)) )
           OR rTrigUnload.Q THEN
            iSorterStep := 300;     // open gate to finish order
        ELSE
            iSorterStep := 0;       // ready for next bottle
        END_IF;


    // -------------------------------------------------------
    300: // OPEN SORTER GATE → DUMP → CLOSE
        bOpenSorterGate := TRUE;
        bCloseSorterGate := FALSE;
        tGateTO(IN := TRUE, PT := T#1500MS);

        IF bSorterGateOpen THEN
            // emulate tote drop completed; close again
            bOpenSorterGate := FALSE;
            bCloseSorterGate := TRUE;
        END_IF;

        IF bSorterGateClosed THEN
            // clear lane (ready for new order)
            IF iTargetLane IN [1..6] THEN
                aSorterLaneBottleCount[iTargetLane] := 0;
                aSorterLaneToteID[iTargetLane] := 0;
            END_IF;
            bCloseSorterGate := FALSE;
            tGateTO(IN := FALSE);
            iSorterStep := 0;
        ELSIF tGateTO.Q THEN
            tGateTO(IN := FALSE);
            iSorterStep := 900; // gate timeout fault
        END_IF;


    // -------------------------------------------------------
    900: // FAULT HANDLER
        bSorterSequenceFault := TRUE;
        sSorterFaultMessage := 'Sorter position or gate timeout';
        // reset motion bits
        bSorterCSTRPWRT := FALSE;
        bRaiseBottleLiftFast := FALSE;
        bRaiseBottleLiftSlow := FALSE;
        bLowerBottleLiftFast := FALSE;
        bLowerBottleLiftSlow := FALSE;
        bOpenBottleLiftGripper := FALSE;
        // wait for HMI Reset
        IF bResetFaultRequestHmi OR bSorterAlarmReset THEN
            bSorterSequenceFault := FALSE;
            sSorterFaultMessage := '';
            iSorterStep := 0;
        END_IF;

END_CASE;

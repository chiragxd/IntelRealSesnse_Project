(*
============================================================
 SORTER AUTO SEQUENCE (final integrated version)
 Logic goals:
  - Use taught positions (1–6 good, 7 reject)
  - One order = one lane until complete
  - Open gate only when that order is done
  - Move uses PC1–3 bits + CSTR pulse (same as manual)
  - BottleLiftLogic handles lift/gripper
============================================================
*)

VAR
    // local helpers only
    tCSTRPulse  : TON;       // 10 ms ON pulse for CSTR
    tPosTO      : TON;       // 4 s move timeout
    tGateTO     : TON;       // 1.5 s gate timeout
    rTrigNewOrd : R_TRIG;
    iPM         : INT;       // decoded current position
    iPickLane   : INT;       // selected target lane
    iOrderQtyForLane : INT;  // PPS order quantity
END_VAR


// Decode position met bits (same as manual)
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// Rising edge when new PPS order arrives
rTrigNewOrd(CLK := PPS_NewOrder);


// ==========================================================
// LANE ALLOCATION  (executed only when a new order appears)
// ==========================================================
IF bOptifillMachineInAutoMode THEN
    IF rTrigNewOrd.Q THEN
        iPickLane := 0;

        // Rejects always use lane 7
        IF (iBottleStatus = 2) OR (iBottleStatus = 3) THEN
            iPickLane := 7;

        // Pass bottle = allocate or reuse lane
        ELSIF (iBottleStatus = 1) THEN
            // Try to reuse existing tote lane (same order)
            FOR i := 1 TO 6 DO
                IF (aSorterLaneToteID[i] = iToteID)
                   AND (aSorterLaneBottleCount[i] < 4) THEN
                    iPickLane := i;
                    EXIT;
                END_IF;
            END_FOR

            // If not found, assign first empty lane
            IF iPickLane = 0 THEN
                FOR i := 1 TO 6 DO
                    IF (aSorterLaneBottleCount[i] = 0) THEN
                        aSorterLaneToteID[i] := iToteID;
                        aSorterLaneBottleCount[i] := 0;
                        iPickLane := i;
                        EXIT;
                    END_IF;
                END_FOR
            END_IF;
        END_IF;

        // Save target lane
        IF iPickLane <> 0 THEN
            iTargetLane := USINT(iPickLane);
        END_IF;
    END_IF;
END_IF;


// ==========================================================
// MAIN SORTER STATE MACHINE
// ==========================================================
CASE iSorterStep OF

    // ------------------------------------------------------
    0: // IDLE
        bSorterLaneInPosition := FALSE;
        bOpenSorterGate := FALSE;
        bCloseSorterGate := FALSE;

        // Ready to move only if new bottle and we know its order
        IF bOptifillMachineInAutoMode
           AND bSorterServoDriveReady
           AND bSorterServoHomed
           AND (iTargetLane IN [1..7])
           AND bNewBottleAtSorter THEN
            iSorterStep := 10;
        END_IF;


    // ------------------------------------------------------
    10: // SET POSITION COMMAND + PULSE CSTR
        bSorterOpMode := FALSE;   // Normal mode
        bSorterJISE := FALSE;     // Not jogging
        bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
        bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
        bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

        tCSTRPulse(IN := TRUE, PT := T#10MS);
        bSorterCSTRPWRT := TRUE;
        tPosTO(IN := TRUE, PT := T#4S);
        iSorterStep := 20;


    // ------------------------------------------------------
    20: // WAIT FOR MOVE COMPLETE
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            tCSTRPulse(IN := FALSE);
        END_IF;

        // success condition
        IF bSorterPENDWEND AND (iPM = INT(iTargetLane)) THEN
            iSorterCurrentLane := iPM;
            bSorterLaneInPosition := TRUE;  // handshake to BottleLiftLogic
            tPosTO(IN := FALSE);
            iSorterStep := 30;

        // timeout fault
        ELSIF tPosTO.Q THEN
            tPosTO(IN := FALSE);
            iSorterStep := 900;
        END_IF;


    // ------------------------------------------------------
    30: // WAIT FOR LIFT TO COMPLETE HANDOFF
        // BottleLiftLogic runs independently
        // When bottle is dropped and lift retracts, continue
        IF NOT bBottleAtLift AND NOT bBottleLiftUp AND bBottleLiftDown THEN
            bSorterLaneInPosition := FALSE;

            // increment count for this lane (only pass lanes)
            IF iTargetLane IN [1..6] THEN
                aSorterLaneBottleCount[iTargetLane] := aSorterLaneBottleCount[iTargetLane] + 1;
            END_IF;

            iSorterStep := 40;
        END_IF;


    // ------------------------------------------------------
    40: // CHECK ORDER STATUS
        iOrderQtyForLane := INT(PPS_Quantity);

        // Only unload if this order is truly complete
        IF (iTargetLane IN [1..6])
           AND (aSorterLaneBottleCount[iTargetLane] >= MIN(4, iOrderQtyForLane))
           AND (aSorterLaneToteID[iTargetLane] = iToteID) THEN
            iSorterStep := 100;     // unload lane (open gate)
        ELSE
            // same order still active → wait for next bottle
            iSorterStep := 0;
        END_IF;


    // ------------------------------------------------------
    100: // OPEN GATE WHEN ORDER COMPLETE
        bOpenSorterGate := TRUE;
        tGateTO(IN := TRUE, PT := T#1500MS);

        IF bSorterGateOpen THEN
            bOpenSorterGate := FALSE;
            bCloseSorterGate := TRUE;
        END_IF;

        IF bSorterGateClosed THEN
            // order complete → clear lane
            IF iTargetLane IN [1..6] THEN
                aSorterLaneBottleCount[iTargetLane] := 0;
                aSorterLaneToteID[iTargetLane] := 0;
            END_IF;
            bCloseSorterGate := FALSE;
            tGateTO(IN := FALSE);
            iSorterStep := 0;

        ELSIF tGateTO.Q THEN
            tGateTO(IN := FALSE);
            iSorterStep := 900;
        END_IF;


    // ------------------------------------------------------
    900: // FAULT STATE
        bSorterCSTRPWRT := FALSE;
        bOpenSorterGate := FALSE;
        bCloseSorterGate := FALSE;
        bSorterLaneInPosition := FALSE;
        sSorterFaultMessage := 'Sorter move or gate timeout';
        IF bSorterAlarmReset THEN
            sSorterFaultMessage := '';
            iSorterStep := 0;
        END_IF;

END_CASE;

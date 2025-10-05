(*
==========================================
 SORTER AUTO SEQUENCE (integrated version)
 Only handles:
 - Lane selection / allocation
 - Move-to-position (using PC1–3 + CSTR)
 - Lane management & order completion
 Delegates lift sequence to BottleLiftLogic
==========================================
*)

VAR
    // local helpers only
    tCSTRPulse  : TON;       // 10 ms ON pulse for CSTR
    tPosTO      : TON;       // 4 s move timeout
    tGateTO     : TON;       // 1.5 s gate timeout
    rTrigUnload : R_TRIG;
    iPM         : INT;       // decoded current position
    iPickLane   : INT;       // selected target lane
END_VAR


// Decode current position bits (same as manual)
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// Rising edge for unload request
rTrigUnload(CLK := bUnloadSorterRequest);


// ============================================
// LANE ALLOCATION
// ============================================
IF bOptifillMachineInAutoMode THEN
    iPickLane := 0;

    // Reject → Lane 7
    IF (iBottleStatus = 2) OR (iBottleStatus = 3) THEN
        iPickLane := 7;
    ELSIF (iBottleStatus = 1) THEN
        // PASS bottle: find existing tote lane or empty one
        FOR i := 1 TO 6 DO
            IF (aSorterLaneToteID[i] = iToteID) AND (aSorterLaneBottleCount[i] < 4) THEN
                iPickLane := i;
                EXIT;
            END_IF;
        END_FOR

        IF iPickLane = 0 THEN
            FOR i := 1 TO 6 DO
                IF (aSorterLaneBottleCount[i] = 0) THEN
                    aSorterLaneToteID[i] := iToteID;
                    iPickLane := i;
                    EXIT;
                END_IF;
            END_FOR
        END_IF;
    END_IF;

    IF iPickLane <> 0 THEN
        iTargetLane := USINT(iPickLane);
    END_IF;
END_IF;


// ============================================
// SORTER STATE MACHINE
// ============================================
CASE iSorterStep OF

    // -------------------------------------------------
    0: // IDLE
        bSorterLaneInPosition := FALSE;
        bOpenSorterGate := FALSE;
        bCloseSorterGate := FALSE;

        IF bOptifillMachineInAutoMode
           AND bSorterServoDriveReady
           AND bSorterServoHomed
           AND (iTargetLane IN [1..7]) THEN
            iSorterStep := 10;
        END_IF;


    // -------------------------------------------------
    10: // SET POSITION COMMAND BITS + PULSE CSTR
        bSorterOpMode := FALSE;   // Normal
        bSorterJISE := FALSE;     // not jogging
        bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
        bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
        bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

        tCSTRPulse(IN := TRUE, PT := T#10MS);
        bSorterCSTRPWRT := TRUE;   // start pulse
        tPosTO(IN := TRUE, PT := T#4S);
        iSorterStep := 20;


    // -------------------------------------------------
    20: // WAIT FOR MOVE COMPLETE
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            tCSTRPulse(IN := FALSE);
        END_IF;

        // success
        IF bSorterPENDWEND AND (iPM = INT(iTargetLane)) THEN
            iSorterCurrentLane := iPM;
            bSorterLaneInPosition := TRUE;  // handshake to BottleLiftLogic
            tPosTO(IN := FALSE);
            iSorterStep := 30;

        // timeout
        ELSIF tPosTO.Q THEN
            tPosTO(IN := FALSE);
            iSorterStep := 900;
        END_IF;


    // -------------------------------------------------
    30: // WAIT FOR LIFT TO COMPLETE HANDOFF
        // BottleLiftLogic drives this: it opens gripper, lowers, resets itself.
        // We simply monitor for completion handshake.
        IF NOT bBottleAtLift AND NOT bBottleLiftUp AND bBottleLiftDown THEN
            // handoff done → bottle placed in lane
            bSorterLaneInPosition := FALSE;

            // increment bottle count
            IF iTargetLane IN [1..6] THEN
                aSorterLaneBottleCount[iTargetLane] := aSorterLaneBottleCount[iTargetLane] + 1;
            END_IF;

            iSorterStep := 40;
        END_IF;


    // -------------------------------------------------
    40: // CHECK IF LANE COMPLETE OR UNLOAD REQUEST
        IF (iTargetLane IN [1..6])
           AND (aSorterLaneBottleCount[iTargetLane] >= 4) THEN
            iSorterStep := 100; // unload
        ELSIF rTrigUnload.Q THEN
            iSorterStep := 100;
        ELSE
            iSorterStep := 0; // ready for next bottle
        END_IF;


    // -------------------------------------------------
    100: // OPEN SORTER GATE
        bOpenSorterGate := TRUE;
        tGateTO(IN := TRUE, PT := T#1500MS);

        IF bSorterGateOpen THEN
            bOpenSorterGate := FALSE;
            bCloseSorterGate := TRUE;
        END_IF;

        IF bSorterGateClosed THEN
            // clear lane
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


    // -------------------------------------------------
    900: // FAULT
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

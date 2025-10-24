VAR
    // LANE CONFIGURATION
    LANE_MIN : INT := 1;
    LANE_MAX : INT := 6;
    REJECT_LANE : INT := 7;

    // ORDER & LANE LINKING
    aSorterLaneOrderNumber : ARRAY[1..7] OF DINT;
    aSorterLaneBottleCount : ARRAY[1..7] OF INT;
    iLaneTargetQty         : ARRAY[1..7] OF INT;
    aLaneReadyToUnload     : ARRAY[1..7] OF BOOL;

    // SYSTEM STATUS
    iOrderNumber     : DINT;
    iBottleStatus    : INT;
    bTransferServoAtCapper, bPrevTransferAtCapper : BOOL;
    bSorterPENDWEND, bCSTR, bPC1, bPC2, bPC3 : BOOL;
    tCSTRPulse : TON;
    iSorterCurrentLane, iTargetLane : INT;

    // FROM ORDER ARRAY
    aTODOOrders : ARRAY[1..5000] OF ST_ToteInfo;

    // TIMERS
    tMoveTimeout : TON;
END_VAR


// ======================================================
// 1️⃣  BOTTLE ARRIVAL DETECTION
// ======================================================
IF bTransferServoAtCapper AND NOT bPrevTransferAtCapper THEN
    // --- Bottle just arrived ---
    // read current order number and bottle status from PPS
    iOrderNumber := nSys.FillBtl.OrderNumber;
    iBottleStatus := nSys.FillBtl.BottleStatus;

    // ======================================================
    // 2️⃣  DETERMINE TARGET LANE (BASED ON ORDER NUMBER)
    // ======================================================
    iTargetLane := 0;

    IF iBottleStatus <> 1 THEN
        // Rejected bottle
        iTargetLane := REJECT_LANE;
    ELSE
        // Check if this order already has a lane assigned
        FOR i := LANE_MIN TO LANE_MAX DO
            IF aSorterLaneOrderNumber[i] = iOrderNumber THEN
                iTargetLane := i;
                EXIT;
            END_IF
        END_FOR

        // If not found, assign a free lane
        IF iTargetLane = 0 THEN
            FOR i := LANE_MIN TO LANE_MAX DO
                IF aSorterLaneOrderNumber[i] = 0 THEN
                    aSorterLaneOrderNumber[i] := iOrderNumber;
                    // find target quantity from aTODOOrders
                    FOR j := 1 TO 5000 DO
                        IF aTODOOrders[j].OrderNumber = iOrderNumber THEN
                            iLaneTargetQty[i] := aTODOOrders[j].NumBottles;
                            EXIT;
                        END_IF
                    END_FOR
                    iSorterCurrentLane := i;
                    iTargetLane := i;
                    EXIT;
                END_IF
            END_FOR
        END_IF

        // If still 0, all lanes full — use reject lane
        IF iTargetLane = 0 THEN
            iTargetLane := REJECT_LANE;
        END_IF
    END_IF

    // ======================================================
    // 3️⃣  MOVE TO TARGET LANE USING SERVO LOGIC
    // ======================================================
    IF NOT bSorterPENDWEND THEN
        // Build PC bits based on target lane (1–7)
        bPC1 := (iTargetLane AND 1) <> 0;
        bPC2 := (iTargetLane AND 2) <> 0;
        bPC3 := (iTargetLane AND 4) <> 0;

        // Trigger CSTR pulse for 6 ms
        tCSTRPulse(IN := TRUE, PT := T#6MS);
        bCSTR := TRUE;
    END_IF

    IF tCSTRPulse.Q THEN
        bCSTR := FALSE;
        tCSTRPulse(IN := FALSE);
    END_IF
END_IF
bPrevTransferAtCapper := bTransferServoAtCapper;


// ======================================================
// 4️⃣  BOTTLE DROP (AT SORTER POSITION)
// ======================================================
IF bBottleLiftGripperOpenAtSorter THEN
    // increment that lane’s bottle count
    aSorterLaneBottleCount[iTargetLane] := aSorterLaneBottleCount[iTargetLane] + 1;

    // check if lane is now full
    IF aSorterLaneBottleCount[iTargetLane] >= iLaneTargetQty[iTargetLane] THEN
        aLaneReadyToUnload[iTargetLane] := TRUE;
    END_IF
END_IF


// ======================================================
// 5️⃣  LANE UNLOAD LOGIC
// ======================================================
FOR i := LANE_MIN TO LANE_MAX DO
    IF aLaneReadyToUnload[i] THEN
        // check order complete status from aTODOOrders
        FOR j := 1 TO 5000 DO
            IF aTODOOrders[j].OrderNumber = aSorterLaneOrderNumber[i] THEN
                IF aTODOOrders[j].BottlesDone >= aTODOOrders[j].NumBottles THEN
                    // Move to that lane (same servo logic)
                    iSorterCurrentLane := i;
                    // Wait for lift ready signal (add your own condition here)
                    IF bLiftReady THEN
                        bGateOpen[i] := TRUE; // open gate
                        IF bLaneEmpty[i] THEN
                            bGateOpen[i] := FALSE;
                            // clear lane
                            aSorterLaneOrderNumber[i] := 0;
                            aSorterLaneBottleCount[i] := 0;
                            iLaneTargetQty[i] := 0;
                            aLaneReadyToUnload[i] := FALSE;
                        END_IF
                    END_IF
                END_IF
            END_IF
        END_FOR
    END_IF
END_FOR

PROGRAM PRG_BottleSorter
VAR
    // ---- Constants ----
    LANE_MIN       : INT := 1;
    LANE_MAX       : INT := 6;
    REJECT_LANE    : INT := 7;
    LANE_CAPACITY  : INT := 4;

    // ---- Lane variables ----
    aSorterLaneToteID        : ARRAY[1..6] OF INT;
    aSorterLaneBottleCount   : ARRAY[1..6] OF INT;
    iLaneTargetQty           : ARRAY[1..6] OF INT;
    aLaneReadyToUnload       : ARRAY[1..6] OF BOOL;

    // ---- Sorter position ----
    iSorterCurrentLane, iTargetLane, iPM : INT;
    iSorterStep, iGateStep, iResetStep : INT;

    // ---- Sorter outputs ----
    bPositionCmdBit1, bPositionCmdBit2, bPositionCmdBit3 : BOOL;
    bSorterCSTRPWRT, bSorterOpMode, bSorterJISE : BOOL;
    bOpenSorterGateOutput, bCloseSorterGateOutput : BOOL;

    // ---- Inputs / interlocks ----
    bSorterPENDWEND, bSorterGateOpen, bSorterGateClosed,
    bSorterLaneEmpty, bTransferServoAtCapper,
    bLiftGripperOpenAtCapper, bBottleLiftDown,
    bBottleLiftGripperOpenAtSorter : BOOL;

    // ---- System vars ----
    bOptifillMachineInAutoMode, bCycleStart, bUnloadSorterRequest : BOOL;
    iBottleStatus, iToteID, PPS_Quantity : INT;

    // ---- Timers ----
    tCstrPulse, tPCStable, tGateTmr, tTimeout : TON;

    // ---- Internals ----
    pendingUnloadLane, seekLane, tgtLane : INT;
    bPrevTransferAtCapper : BOOL := FALSE;
END_VAR

// ============= Lane Assignment when Bottle arrives =============
IF bOptifillMachineInAutoMode AND bCycleStart THEN
    IF (bTransferServoAtCapper AND NOT bPrevTransferAtCapper) THEN
        // Rising edge → new bottle arrived
        IF iBottleStatus = 1 THEN
            tgtLane := 0;
            // Find existing lane for same tote
            FOR seekLane := LANE_MIN TO LANE_MAX DO
                IF (aSorterLaneToteID[seekLane] = iToteID) AND
                   (aSorterLaneBottleCount[seekLane] < iLaneTargetQty[seekLane]) THEN
                    tgtLane := seekLane;
                    EXIT;
                END_IF;
            END_FOR;

            // If none, allocate new lane
            IF tgtLane = 0 THEN
                FOR seekLane := LANE_MIN TO LANE_MAX DO
                    IF (aSorterLaneToteID[seekLane] = 0) THEN
                        aSorterLaneToteID[seekLane] := iToteID;
                        iLaneTargetQty[seekLane] := MIN(PPS_Quantity, LANE_CAPACITY);
                        aSorterLaneBottleCount[seekLane] := 0;
                        tgtLane := seekLane;
                        EXIT;
                    END_IF;
                END_FOR;
            END_IF;
            IF tgtLane <> 0 THEN iTargetLane := tgtLane; END_IF;
        ELSE
            // Reject bottle
            iTargetLane := REJECT_LANE;
        END_IF;
    END_IF;
END_IF;
bPrevTransferAtCapper := bTransferServoAtCapper;

// ============= Move To Lane Sequence =============
CASE iSorterStep OF
    0:
        IF (iTargetLane >= LANE_MIN AND iTargetLane <= LANE_MAX) OR (iTargetLane = REJECT_LANE) THEN
            IF (iSorterCurrentLane <> iTargetLane) OR (iPM <> iTargetLane) THEN
                bSorterOpMode := FALSE;  
                bSorterJISE   := FALSE;

                bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
                bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
                bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

                tPCStable(IN := TRUE, PT := T#6MS);
                iSorterStep := 5;
            END_IF;
        END_IF;

    5:
        IF tPCStable.Q THEN
            tPCStable(IN := FALSE);
            bSorterCSTRPWRT := TRUE;
            tCstrPulse(IN := TRUE, PT := T#6MS);
            tTimeout(IN := TRUE, PT := T#2000MS);
            iSorterStep := 10;
        END_IF;

    10:
        IF tCstrPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            tCstrPulse(IN := FALSE);
        END_IF;

        IF bSorterPENDWEND AND (iPM = iTargetLane) THEN
            iSorterCurrentLane := iTargetLane;
            tTimeout(IN := FALSE);
            iSorterStep := 0;
        ELSIF tTimeout.Q THEN
            // fault recovery can be added here
            tTimeout(IN := FALSE);
            iSorterStep := 0;
        END_IF;
END_CASE;

// ============= Bottle Drop Counting =============
IF bBottleLiftGripperOpenAtSorter THEN
    IF (iSorterCurrentLane >= LANE_MIN) AND (iSorterCurrentLane <= LANE_MAX) THEN
        aSorterLaneBottleCount[iSorterCurrentLane] := aSorterLaneBottleCount[iSorterCurrentLane] + 1;

        IF iLaneTargetQty[iSorterCurrentLane] = 1 THEN
            IF bUnloadSorterRequest THEN
                aLaneReadyToUnload[iSorterCurrentLane] := TRUE;
            END_IF;
        ELSE
            IF aSorterLaneBottleCount[iSorterCurrentLane] >= iLaneTargetQty[iSorterCurrentLane] THEN
                aLaneReadyToUnload[iSorterCurrentLane] := TRUE;
            END_IF;
        END_IF;
    END_IF;
END_IF;

// ============= Unload / Gate Sequence =============
IF (NOT bTransferServoAtCapper) AND bLiftGripperOpenAtCapper AND bBottleLiftDown THEN
    pendingUnloadLane := 0;
    FOR seekLane := LANE_MIN TO LANE_MAX DO
        IF aLaneReadyToUnload[seekLane] THEN
            pendingUnloadLane := seekLane;
            EXIT;
        END_IF;
    END_FOR;

    IF (pendingUnloadLane <> 0) AND (iSorterStep = 0) THEN
        IF iSorterCurrentLane <> pendingUnloadLane THEN
            iTargetLane := pendingUnloadLane;
        ELSE
            aLaneReadyToUnload[pendingUnloadLane] := FALSE;
            iGateStep := 10;
        END_IF;
    END_IF;
END_IF;

// ---- Gate Logic ----
CASE iGateStep OF
    0:
        // Idle

    10:
        IF bSorterGateClosed THEN
            bOpenSorterGateOutput := TRUE;
            bCloseSorterGateOutput := FALSE;
            tGateTmr(IN := TRUE, PT := T#2S);
            iGateStep := 12;
        END_IF;

    12:
        IF bSorterGateOpen OR tGateTmr.Q THEN
            tGateTmr(IN := FALSE);
            iGateStep := 20;
        END_IF;

    20:
        IF bSorterLaneEmpty THEN
            iGateStep := 30;
        END_IF;

    30:
        bOpenSorterGateOutput := FALSE;
        bCloseSorterGateOutput := TRUE;
        tGateTmr(IN := TRUE, PT := T#2S);

        IF bSorterGateClosed OR tGateTmr.Q THEN
            tGateTmr(IN := FALSE);
            IF (iSorterCurrentLane >= LANE_MIN) AND (iSorterCurrentLane <= LANE_MAX) THEN
                aSorterLaneToteID[iSorterCurrentLane] := 0;
                aSorterLaneBottleCount[iSorterCurrentLane] := 0;
                iLaneTargetQty[iSorterCurrentLane] := 0;
            END_IF;
            bCloseSorterGateOutput := FALSE;
            iGateStep := 0;
        END_IF;
END_CASE;

// ============= Reset Sorter Sequence =============
CASE iResetStep OF
    0:
        IF bUnloadSorterRequest THEN
            bCloseSorterGateOutput := TRUE;
            IF bSorterGateClosed THEN
                iTargetLane := REJECT_LANE;
                iResetStep := 10;
            END_IF;
        END_IF;

    10:
        IF (iSorterCurrentLane = REJECT_LANE) THEN
            iTargetLane := 1;
            iResetStep := 20;
        END_IF;

    20:
        IF (iSorterCurrentLane = 1) THEN
            iResetStep := 0;
        END_IF;
END_CASE;

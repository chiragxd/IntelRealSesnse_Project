(*
=========================================================
 SORTER AUTO (FINAL PRODUCTION VERSION)
 - Lane = Tote (each lane = 1 tote)
 - Lane capacity = 4 bottles
 - Reject lane = 7
 - Single-bottle tote unloads instantly if tote present
 - Early lane assignment when Transfer Servo reaches Capper
 - Unload only when Lift is down + Gripper open at Capper
 - Uses: PC1–PC3 + CSTR pulse for move, PEND/WEND for position done
 - No R_TRIG blocks; simple scan-safe latches
=========================================================
*)

VAR
    // ---------------- LOCAL CONFIG ----------------
    C_LANE_MIN          : USINT := 1;
    C_LANE_MAX          : USINT := 6;
    C_REJECT_LANE       : USINT := 7;
    C_LANE_CAPACITY     : USINT := 4;

    _LaneTargetQty      : ARRAY[1..6] OF USINT;   // Target qty per tote (lane)
    _bLaneReadyToUnload : ARRAY[1..6] OF BOOL;    // Lanes flagged for unload

    _prevTransferAtCapper : BOOL;
    _prevLiftGripperOpenAtCapper : BOOL;
    _prevBottleAtLift : BOOL;

    _moveStep   : USINT;    // 0=idle,10=pulse,20=wait
    _cstrPulse  : TON;
    _gateStep   : USINT;    // 0=idle,10=open,20=wait clear,30=close
    _gateTmr    : TON;

    iPM : USINT;            // Position feedback bits decode

    tgtLane, seekLane : USINT;
    pendingUnloadLane : USINT;
    havePendingUnload : BOOL;

    thisIsPass : BOOL;
    thisToteID : INT;
END_VAR


// ======================================================
// 1) Decode servo feedback bits
// ======================================================
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// ======================================================
// 2) EARLY LANE ASSIGNMENT (triggered by Transfer Servo at Capper)
// ======================================================
IF bBottleTransferAtCapperCon AND (NOT _prevTransferAtCapper) THEN
    thisIsPass := (iBottleStatus = 1);
    thisToteID := iToteID;

    IF thisIsPass THEN
        // --- Try to reuse lane for same tote ---
        tgtLane := 0;
        FOR seekLane := C_LANE_MIN TO C_LANE_MAX DO
            IF aSorterLaneToteID[seekLane] = thisToteID THEN
                IF aSorterLaneBottleCount[seekLane] < _LaneTargetQty[seekLane] THEN
                    tgtLane := seekLane;
                    EXIT;
                END_IF;
            END_IF;
        END_FOR

        // --- If none, assign a new free lane ---
        IF tgtLane = 0 THEN
            FOR seekLane := C_LANE_MIN TO C_LANE_MAX DO
                IF (aSorterLaneToteID[seekLane] = 0) AND (aSorterLaneBottleCount[seekLane] = 0) THEN
                    aSorterLaneToteID[seekLane] := thisToteID;
                    IF PPS_Quantity <= 1 THEN
                        _LaneTargetQty[seekLane] := 1;
                    ELSE
                        IF PPS_Quantity > C_LANE_CAPACITY THEN
                            _LaneTargetQty[seekLane] := C_LANE_CAPACITY;
                        ELSE
                            _LaneTargetQty[seekLane] := USINT(PPS_Quantity);
                        END_IF;
                    END_IF;
                    tgtLane := seekLane;
                    EXIT;
                END_IF;
            END_FOR
        END_IF;

        IF tgtLane <> 0 THEN
            iTargetLane := tgtLane;
        END_IF;
    ELSE
        // --- Reject bottle ---
        iTargetLane := C_REJECT_LANE;
    END_IF;
END_IF;
_prevTransferAtCapper := bBottleTransferAtCapperCon;


// ======================================================
// 3) MOVE TO TARGET LANE (same logic as manual mode)
// ======================================================
CASE _moveStep OF
    0:
        IF (iTargetLane >= C_LANE_MIN AND iTargetLane <= C_LANE_MAX) OR (iTargetLane = C_REJECT_LANE) THEN
            IF (iSorterCurrentLane <> iTargetLane) OR (iPM <> iTargetLane) THEN
                bSorterOpMode := FALSE;  // Normal
                bSorterJISE   := FALSE;

                bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
                bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
                bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

                _moveStep := 10;
            END_IF
        END_IF

    10:
        bSorterCSTRPWRT := TRUE;
        _cstrPulse(IN := TRUE, PT := T#10MS);
        _moveStep := 20;

    20:
        IF _cstrPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            _cstrPulse(IN := FALSE);
        END_IF;

        IF bSorterPENDWEND AND (iPM = iTargetLane) THEN
            iSorterCurrentLane := iPM;
            _moveStep := 0;
        END_IF;
END_CASE;


// ======================================================
// 4) COUNT BOTTLE AFTER DROP + UNLOAD FLAG
// ======================================================
IF bBottleLiftGripperOpenAtSorter THEN
    IF (iSorterCurrentLane >= C_LANE_MIN) AND (iSorterCurrentLane <= C_LANE_MAX) THEN
        aSorterLaneBottleCount[iSorterCurrentLane] := aSorterLaneBottleCount[iSorterCurrentLane] + 1;

        // If single-bottle tote and tote present
        IF _LaneTargetQty[iSorterCurrentLane] = 1 THEN
            IF bUnloadSorterRequest THEN
                _bLaneReadyToUnload[iSorterCurrentLane] := TRUE;
            END_IF;
        ELSE
            IF aSorterLaneBottleCount[iSorterCurrentLane] >= _LaneTargetQty[iSorterCurrentLane] THEN
                _bLaneReadyToUnload[iSorterCurrentLane] := TRUE;
            END_IF;
        END_IF;
    END_IF;
END_IF;


// ======================================================
// 5) UNLOAD LOGIC — only when lift is down + gripper open at capper
// ======================================================
IF (NOT bBottleTransferAtCapperCon) AND bLiftGripperOpenAtCapper AND bBottleLiftDown THEN
    pendingUnloadLane := 0;
    FOR seekLane := C_LANE_MIN TO C_LANE_MAX DO
        IF _bLaneReadyToUnload[seekLane] THEN
            pendingUnloadLane := seekLane;
            EXIT;
        END_IF;
    END_FOR;

    IF (pendingUnloadLane <> 0) AND (_moveStep = 0) THEN
        IF iSorterCurrentLane <> pendingUnloadLane THEN
            iTargetLane := pendingUnloadLane;
        ELSE
            _bLaneReadyToUnload[pendingUnloadLane] := FALSE;
            _gateStep := 10;
        END_IF;
    END_IF;
END_IF;


// ======================================================
// 6) GATE SEQUENCE (open → wait empty → close → clear lane)
// ======================================================
CASE _gateStep OF
    0:
        // Idle

    10:
        IF bUnloadSorterRequest AND bSorterGateClosed THEN
            bOpenSorterGateOutput := TRUE;
            bCloseSorterGateOutput := FALSE;
            _gateTmr(IN := TRUE, PT := T#2S);
            _gateStep := 12;
        END_IF;

    12:
        IF bSorterGateOpen OR _gateTmr.Q THEN
            _gateTmr(IN := FALSE);
            _gateStep := 20;
        END_IF;

    20:
        IF bSorterLaneEmpty THEN
            _gateStep := 30;
        END_IF;

    30:
        bOpenSorterGateOutput := FALSE;
        bCloseSorterGateOutput := TRUE;
        _gateTmr(IN := TRUE, PT := T#2S);

        IF bSorterGateClosed OR _gateTmr.Q THEN
            _gateTmr(IN := FALSE);
            IF (iSorterCurrentLane >= C_LANE_MIN) AND (iSorterCurrentLane <= C_LANE_MAX) THEN
                aSorterLaneToteID[iSorterCurrentLane] := 0;
                aSorterLaneBottleCount[iSorterCurrentLane] := 0;
                _LaneTargetQty[iSorterCurrentLane] := 0;
            END_IF;
            bCloseSorterGateOutput := FALSE;
            _gateStep := 0;
        END_IF;
END_CASE;

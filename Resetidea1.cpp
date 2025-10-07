(*
=========================================================
 MACHINE FLUSH & CONTROLLED RESET — FINAL VERSION
=========================================================
*)

VAR
    rTrig_NewFlush       : R_TRIG;
    bFlushActive         : BOOL;
    bBottleFlushRejectDone : BOOL;
    bHomeReady           : BOOL;
    bAllHomed            : BOOL;

    bFlushStep           : USINT := 0;
END_VAR;


// ======================================================
// 1️⃣ Detect New Data Arrival (Flush Trigger)
// ======================================================
rTrig_NewFlush(CLK := New_Data_Arrival);

IF rTrig_NewFlush.Q THEN
    bFlushActive := TRUE;
    bCycleStart := FALSE;
    bOptifillMachineInAutoMode := FALSE;
    bShowFlushPopup := TRUE;
    bFlushStep := 10;       // start sequence
END_IF;


// ======================================================
// 2️⃣ Controlled Flush Handling Sequence
// ======================================================
CASE bFlushStep OF

    // Step 10 — Detect if bottle exists
    10:
        IF bBottleTransferGripperClosed OR bBottleAtLift OR bBottleAtCapper THEN
            iBottleStatus := 3;   // reject current bottle
            bFlushStep := 20;
        ELSE
            bFlushStep := 40;     // nothing to reject → go home directly
        END_IF

    // Step 20 — Execute reject sequence depending on where bottle is
    20:
        // Priority: lift > capper > transfer gripper
        IF bBottleAtLift THEN
            // send lift bottle to reject
            bLiftGoToReject := TRUE;
            IF bLiftAtRejectPos THEN
                bLiftOpenGripper := TRUE;
                bBottleFlushRejectDone := TRUE;
            END_IF;

        ELSIF bBottleAtCapper THEN
            bCapperReleaseBottle := TRUE;
            IF bCapperIdle THEN
                bBottleFlushRejectDone := TRUE;
            END_IF;

        ELSIF bBottleTransferGripperClosed THEN
            bTransferToReject := TRUE;
            IF bTransferAtRejectPos THEN
                bOpenTransferGripper := TRUE;
                bBottleFlushRejectDone := TRUE;
            END_IF;
        ELSE
            bBottleFlushRejectDone := TRUE;
        END_IF;

        IF bBottleFlushRejectDone THEN
            // once bottle rejected, move to home
            bLiftGoToReject := FALSE;
            bTransferToReject := FALSE;
            bCapperReleaseBottle := FALSE;
            bFlushStep := 40;
        END_IF;

    // Step 40 — All bottles rejected → safe to home
    40:
        // command home for all stations
        bStartHomingTransfer := TRUE;
        bStartHomingLift := TRUE;
        bStartHomingEscapement := TRUE;
        bStartHomingCapper := TRUE;
        bStartHomingSorter := TRUE;

        IF bTransferHomed AND bLiftHomed AND bEscapementHomed AND 
           bCapperHomed AND bSorterHomed THEN
            bAllHomed := TRUE;
        END_IF;

        IF bAllHomed THEN
            bStartHomingTransfer := FALSE;
            bStartHomingLift := FALSE;
            bStartHomingEscapement := FALSE;
            bStartHomingCapper := FALSE;
            bStartHomingSorter := FALSE;

            // clear FIFO
            iTODO_Head := 1;
            iTODO_Tail := 0;
            iWIP_Count := 0;
            FOR i := 1 TO 100 DO
                aTODO[i].OrderID := 0;
                aTODO[i].Active := FALSE;
            END_FOR;
            FOR i := 1 TO 6 DO
                aWIP[i].OrderID := 0;
                aWIP[i].Active := FALSE;
                aWIP[i].LaneAssigned := 0;
                aWIP[i].BottleCount := 0;
                _bLaneReadyToUnload[i] := FALSE;
            END_FOR;

            bFlushStep := 50;
        END_IF;

    // Step 50 — Done
    50:
        bFlushActive := FALSE;
        bShowFlushPopup := FALSE;
        bBottleFlushRejectDone := FALSE;
        bAllHomed := FALSE;
        bResetComplete := TRUE;
        bFlushStep := 0;
END_CASE;

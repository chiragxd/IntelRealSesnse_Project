PROGRAM PRG_OFM
(*
    FINAL OFM LOGIC
    - Induct patient orders
    - Assign sorter lanes (4 bottles per lane)
    - Trigger sorter to move on bottle order
    - Track bottle drops & lane clears
    - Auto-eject tote when done
    - Auto-reset all working data when system is idle

    NOTE on "NewDataArrival" handshakes:
    - This version processes when the value == 1 and ACKs with 6.
    - If your Kepware/host is inverted (process when <>1), flip those IF conditions.
*)

VAR
    // ====== System / Status ======
    nSys_NewDataArrival, nSys_Status : INT;

    // ====== Patient Order / Tote Info ======
    nSys_ToteInfo_NewDataArrival : INT;
    nSys_ToteInfo_ToteID         : INT;
    nSys_ToteInfo_NumberOfBottles: INT;
    nSys_ToteInfo_Route          : INT;

    // ====== Bottle Order / FillBtl ======
    nSys_FillBtl_NewDataArrival  : INT;
    nSys_FillBtl_Cabinet         : INT;
    nSys_FillBtl_ToteID          : INT;
    nSys_FillBtl_Quantity        : INT;
    nSys_FillBtl_Barcode1        : INT;
    nSys_FillBtl_Barcode2        : INT;
    nSys_FillBtl_Barcode3        : INT;

    // ====== Bottle Status ======
    nSys_BtlStat_NewDataArrival  : INT;
    nSys_BtlStat_ToteID          : INT;
    nSys_BtlStat_Status          : INT;

    // ====== Tote Eject ======
    nSys_ToteEject_NewDataArrival: INT;
    nSys_ToteEject_ToteID        : INT;
    nSys_ToteEject_Route         : INT;

    // ====== Diagnostics ======
    nSys_Diag_NewDataArrival     : INT;
    nSys_Diag_LineID             : INT;
    nSys_Diag_MessageID          : INT;
    nSys_Diag_DataWord           : ARRAY[0..17] OF INT;

    // ====== Internal ======
    bInit : BOOL := TRUE;
    i, k, Lane, BottleReq, SelectedSlot, Remaining : INT;

    // Active totes (max 6)
    ActiveTotes : ARRAY[1..6] OF INT := [0,0,0,0,0,0];
    ActiveCount : INT := 0;

    // Manual FIFO (for patient order trace)
    MANUAL_FIFO_SIZE : INT := 100;
    Man_Tote_FIFO    : ARRAY[0..99] OF INT;
    Man_Route_FIFO   : ARRAY[0..99] OF INT;
    ManHead, ManTail, ManCount : INT := 0;

    // Bottle FIFO (trace only)
    BTL_FIFO_SIZE : INT := 20;
    Btl_Tote_FIFO    : ARRAY[0..19] OF INT;
    Btl_Cabinet_FIFO : ARRAY[0..19] OF INT;
    Btl_Qty_FIFO     : ARRAY[0..19] OF INT;
    Btl_BC1_FIFO     : ARRAY[0..19] OF INT;
    Btl_BC2_FIFO     : ARRAY[0..19] OF INT;
    Btl_BC3_FIFO     : ARRAY[0..19] OF INT;
    BtlHead, BtlTail, BtlCount : INT := 0;

    // ====== Error Flags ======
    err_PatientLimitExceeded   : BOOL := FALSE;   // >6 totes
    err_DuplicatePatientOrder  : BOOL := FALSE;   // same tote twice
    err_ManualPickFifoFull     : BOOL := FALSE;   // Man FIFO overflow
    err_InvalidFillerCabinet   : BOOL := FALSE;   // FillBtl cab out of range
    err_SorterCapacityExceeded : BOOL := FALSE;   // not enough lane capacity

    // ====== Sorter Lane Assignment (OFM side) ======
    LANE_CAPACITY  : INT := 4;                    // bottles per lane
    LaneTote        : ARRAY[1..6] OF INT;         // tote assigned to lane
    LaneBottleCount : ARRAY[1..6] OF INT;         // bottles remaining for lane
    LaneAssigned    : ARRAY[1..6] OF BOOL;        // lane occupied

    // ====== Sorter Interface (shared with sorter program) ======
    g_bSorterReady         : BOOL;                // sorter ready for next move
    g_iTargetLane          : INT;                 // OFM → sorter
    g_bMoveReq             : BOOL;                // OFM → sorter (one-shot)

    g_bBottleDroppedEvent  : BOOL;                // sorter → OFM (one-scan)
    g_iBottleDroppedLane   : INT;                 // sorter → OFM
    g_iBottleDroppedTote   : INT;                 // sorter → OFM

    g_bLaneClearedEvent    : BOOL;                // sorter → OFM (one-scan)
    g_iLaneCleared         : INT;                 // sorter → OFM

    // ====== Optional manual reset ======
    bManualResetRequest    : BOOL := FALSE;

    // ====== Helpers ======
    bIsDuplicate : BOOL;
    freeLanes, usedLanes : INT;
END_VAR


// ====================================================================
// 0) INITIALIZATION (runs once)
// ====================================================================
IF bInit THEN
    FOR i := 1 TO 6 DO
        ActiveTotes[i] := 0;
        LaneTote[i] := 0;
        LaneBottleCount[i] := 0;
        LaneAssigned[i] := FALSE;
    END_FOR

    ActiveCount := 0;
    ManHead := 0; ManTail := 0; ManCount := 0;
    BtlHead := 0; BtlTail := 0; BtlCount := 0;

    err_PatientLimitExceeded := FALSE;
    err_DuplicatePatientOrder := FALSE;
    err_ManualPickFifoFull := FALSE;
    err_InvalidFillerCabinet := FALSE;
    err_SorterCapacityExceeded := FALSE;

    // Handshake defaults (ACK = 6)
    nSys_ToteInfo_NewDataArrival := 6;
    nSys_FillBtl_NewDataArrival  := 6;
    nSys_BtlStat_NewDataArrival  := 6;
    nSys_ToteEject_NewDataArrival:= 6;
    nSys_Diag_NewDataArrival     := 6;

    bInit := FALSE;
END_IF


// ====================================================================
// 1) RECEIVE PATIENT ORDER / TOTE INFO
//    (flip to IF nSys_ToteInfo_NewDataArrival <> 1 THEN if your host is inverted)
// ====================================================================
IF nSys_ToteInfo_NewDataArrival = 1 THEN
    // Duplicate check
    bIsDuplicate := FALSE;
    FOR k := 1 TO 6 DO
        IF ActiveTotes[k] = nSys_ToteInfo_ToteID THEN
            bIsDuplicate := TRUE;
            EXIT;
        END_IF
    END_FOR

    IF bIsDuplicate THEN
        err_DuplicatePatientOrder := TRUE;

    ELSIF ActiveCount >= 6 THEN
        err_PatientLimitExceeded := TRUE;

    ELSE
        // compute remaining lane capacity (free lanes * 4)
        freeLanes := 0;
        FOR Lane := 1 TO 6 DO
            IF NOT LaneAssigned[Lane] THEN freeLanes := freeLanes + 1; END_IF
        END_FOR

        IF nSys_ToteInfo_NumberOfBottles > (freeLanes * LANE_CAPACITY) THEN
            err_SorterCapacityExceeded := TRUE;
        ELSE
            // Push to Manual FIFO (trace)
            IF ManCount < MANUAL_FIFO_SIZE THEN
                Man_Tote_FIFO[ManHead]  := nSys_ToteInfo_ToteID;
                Man_Route_FIFO[ManHead] := nSys_ToteInfo_Route;
                ManHead := (ManHead + 1) MOD MANUAL_FIFO_SIZE;
                ManCount := ManCount + 1;

                // Add to active list
                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = 0 THEN
                        ActiveTotes[k] := nSys_ToteInfo_ToteID;
                        ActiveCount := ActiveCount + 1;
                        EXIT;
                    END_IF
                END_FOR

                // Lane assignment (split into lanes, 4 per lane)
                BottleReq := nSys_ToteInfo_NumberOfBottles;
                SelectedSlot := nSys_ToteInfo_ToteID; // tote id for clarity

                FOR Lane := 1 TO 6 DO
                    IF (BottleReq <= 0) THEN EXIT; END_IF
                    IF NOT LaneAssigned[Lane] THEN
                        LaneTote[Lane] := SelectedSlot;
                        IF BottleReq <= LANE_CAPACITY THEN
                            LaneBottleCount[Lane] := BottleReq;
                            BottleReq := 0;
                        ELSE
                            LaneBottleCount[Lane] := LANE_CAPACITY;
                            BottleReq := BottleReq - LANE_CAPACITY;
                        END_IF
                        LaneAssigned[Lane] := TRUE;
                    END_IF
                END_FOR
                // If BottleReq > 0 here, wait until a lane clears, then reassign.
            ELSE
                err_ManualPickFifoFull := TRUE;
            END_IF
        END_IF
    END_IF

    // ACK
    nSys_ToteInfo_NewDataArrival := 6;
END_IF


// ====================================================================
// 2) BOTTLE ORDER / FILLBTL  → TRIGGER SORTER MOVE
//    (flip to IF nSys_FillBtl_NewDataArrival <> 1 THEN if inverted)
// ====================================================================
IF nSys_FillBtl_NewDataArrival = 1 THEN
    // Trigger sorter to the first lane for this tote that still needs bottles
    IF g_bSorterReady THEN
        FOR Lane := 1 TO 6 DO
            IF (LaneAssigned[Lane]) AND
               (LaneTote[Lane] = nSys_FillBtl_ToteID) AND
               (LaneBottleCount[Lane] > 0) THEN
                g_iTargetLane := Lane;
                g_bMoveReq := TRUE;       // sorter consumes this one-shot
                EXIT;
            END_IF
        END_FOR
    END_IF

    // Bottle FIFO trace & validity
    IF (nSys_FillBtl_Cabinet >= 1) AND (nSys_FillBtl_Cabinet <= 5) THEN
        IF BtlCount < BTL_FIFO_SIZE THEN
            Btl_Cabinet_FIFO[BtlHead] := nSys_FillBtl_Cabinet;
            Btl_Tote_FIFO[BtlHead]    := nSys_FillBtl_ToteID;
            Btl_Qty_FIFO[BtlHead]     := nSys_FillBtl_Quantity;
            Btl_BC1_FIFO[BtlHead]     := nSys_FillBtl_Barcode1;
            Btl_BC2_FIFO[BtlHead]     := nSys_FillBtl_Barcode2;
            Btl_BC3_FIFO[BtlHead]     := nSys_FillBtl_Barcode3;
            BtlHead := (BtlHead + 1) MOD BTL_FIFO_SIZE;
            BtlCount := BtlCount + 1;
        END_IF
    ELSE
        err_InvalidFillerCabinet := TRUE;
    END_IF

    // ACK
    nSys_FillBtl_NewDataArrival := 6;
END_IF


// ====================================================================
// 3) SORTER FEEDBACK — BOTTLE DROPPED (one-scan event from sorter)
// ====================================================================
IF g_bBottleDroppedEvent THEN
    IF (g_iBottleDroppedLane >= 1) AND (g_iBottleDroppedLane <= 6) THEN
        IF LaneAssigned[g_iBottleDroppedLane] AND
           (LaneTote[g_iBottleDroppedLane] = g_iBottleDroppedTote) AND
           (LaneBottleCount[g_iBottleDroppedLane] > 0) THEN
            LaneBottleCount[g_iBottleDroppedLane] := LaneBottleCount[g_iBottleDroppedLane] - 1;
        END_IF
    END_IF
    // consume the one-scan event
    g_bBottleDroppedEvent := FALSE;
END_IF


// ====================================================================
// 4) SORTER FEEDBACK — LANE CLEARED (gate cycle complete)
// ====================================================================
IF g_bLaneClearedEvent THEN
    IF (g_iLaneCleared >= 1) AND (g_iLaneCleared <= 6) THEN
        LaneTote[g_iLaneCleared] := 0;
        LaneBottleCount[g_iLaneCleared] := 0;
        LaneAssigned[g_iLaneCleared] := FALSE;
    END_IF
    g_bLaneClearedEvent := FALSE;
END_IF


// ====================================================================
// 5) ORDER PROCESSING — CHECK PROGRESS & AUTO-EJECT TOTES
// ====================================================================
FOR k := 1 TO 6 DO
    IF ActiveTotes[k] <> 0 THEN
        Remaining := 0;
        FOR Lane := 1 TO 6 DO
            IF LaneAssigned[Lane] AND (LaneTote[Lane] = ActiveTotes[k]) THEN
                Remaining := Remaining + LaneBottleCount[Lane];
            END_IF
        END_FOR

        IF Remaining = 0 THEN
            // All lanes for this tote are cleared → Eject
            nSys_ToteEject_ToteID := ActiveTotes[k];
            nSys_ToteEject_Route  := nSys_ToteInfo_Route;  // if per-tote route stored elsewhere, use that
            nSys_ToteEject_NewDataArrival := 1;

            // Remove from active list
            ActiveTotes[k] := 0;
            IF ActiveCount > 0 THEN ActiveCount := ActiveCount - 1; END_IF
        END_IF
    END_IF
END_FOR


// ====================================================================
// 6) AUTO-RESET / CLEAR ALL DATA WHEN SYSTEM IS IDLE
//     (or drive with bManualResetRequest if you prefer manual control)
// ====================================================================
IF (ActiveCount = 0) OR bManualResetRequest THEN
    // ensure all lanes are free
    usedLanes := 0;
    FOR Lane := 1 TO 6 DO
        IF LaneAssigned[Lane] THEN usedLanes := usedLanes + 1; END_IF
    END_FOR

    IF (usedLanes = 0) THEN
        // clear all working arrays & FIFOs
        FOR i := 1 TO 6 DO
            ActiveTotes[i] := 0;
            LaneTote[i] := 0;
            LaneBottleCount[i] := 0;
            LaneAssigned[i] := FALSE;
        END_FOR

        ActiveCount := 0;

        ManHead := 0; ManTail := 0; ManCount := 0;
        BtlHead := 0; BtlTail := 0; BtlCount := 0;

        // clear errors
        err_PatientLimitExceeded := FALSE;
        err_DuplicatePatientOrder := FALSE;
        err_ManualPickFifoFull := FALSE;
        err_InvalidFillerCabinet := FALSE;
        err_SorterCapacityExceeded := FALSE;

        // optional: clear manual reset
        bManualResetRequest := FALSE;
    END_IF
END_IF


// ====================================================================
// 7) BOTTLE STATUS FEEDBACK (optional handling)
// ====================================================================
IF nSys_BtlStat_NewDataArrival = 1 THEN
    // 0=OK, 1=No-Read, 2=Out-of-Sync (extend as needed)
    // ... your handling here if required ...
    nSys_BtlStat_NewDataArrival := 6;
END_IF


// ====================================================================
// 8) DIAGNOSTICS / ERROR MESSAGES
// ====================================================================
IF err_PatientLimitExceeded THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 100;
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6; err_PatientLimitExceeded := FALSE;
END_IF;

IF err_DuplicatePatientOrder THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 101;
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6; err_DuplicatePatientOrder := FALSE;
END_IF;

IF err_SorterCapacityExceeded THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 102;
    nSys_Diag_DataWord[0] := 0; // you can put remaining capacity here if you track it
    nSys_Diag_NewDataArrival := 6; err_SorterCapacityExceeded := FALSE;
END_IF;

IF err_InvalidFillerCabinet THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 200;
    nSys_Diag_DataWord[0] := nSys_FillBtl_Cabinet;
    nSys_Diag_NewDataArrival := 6; err_InvalidFillerCabinet := FALSE;
END_IF;

IF err_ManualPickFifoFull THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 104;
    nSys_Diag_DataWord[0] := ManCount;
    nSys_Diag_NewDataArrival := 6; err_ManualPickFifoFull := FALSE;
END_IF;

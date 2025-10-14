PROGRAM PRG_OFM
VAR
    // ---- System & Status ----
    nSys_NewDataArrival, nSys_Status : INT;

    // ---- Patient Order / Tote Info ----
    nSys_ToteInfo_NewDataArrival, nSys_ToteInfo_ToteID,
    nSys_ToteInfo_NumberOfBottles, nSys_ToteInfo_Route : INT;

    // ---- Bottle Order / FillBtl ----
    nSys_FillBtl_NewDataArrival, nSys_FillBtl_Cabinet,
    nSys_FillBtl_ToteID, nSys_FillBtl_Quantity,
    nSys_FillBtl_Barcode1, nSys_FillBtl_Barcode2, nSys_FillBtl_Barcode3 : INT;

    // ---- Bottle Status ----
    nSys_BtlStat_NewDataArrival, nSys_BtlStat_ToteID, nSys_BtlStat_Status : INT;

    // ---- Tote Eject ----
    nSys_ToteEject_NewDataArrival, nSys_ToteEject_ToteID, nSys_ToteEject_Route : INT;

    // ---- Diagnostics ----
    nSys_Diag_NewDataArrival, nSys_Diag_LineID, nSys_Diag_MessageID : INT;
    nSys_Diag_DataWord : ARRAY[0..17] OF INT;

    // ---- Internal working variables ----
    bInit                    : BOOL := TRUE;
    i, k, Lane, BottleReq, RouteCode, SelectedSlot : INT;

    ActiveTotes              : ARRAY[1..6] OF INT := [0,0,0,0,0,0];
    ActiveCount              : INT := 0;

    MANUAL_FIFO_SIZE         : INT := 100;
    Man_Tote_FIFO            : ARRAY[0..99] OF INT;
    Man_Route_FIFO           : ARRAY[0..99] OF INT;
    ManHead, ManTail, ManCount : INT := 0;

    BTL_FIFO_SIZE            : INT := 20;
    Btl_Tote_FIFO            : ARRAY[0..19] OF INT;
    Btl_Cabinet_FIFO         : ARRAY[0..19] OF INT;
    Btl_Qty_FIFO             : ARRAY[0..19] OF INT;
    Btl_BC1_FIFO             : ARRAY[0..19] OF INT;
    Btl_BC2_FIFO             : ARRAY[0..19] OF INT;
    Btl_BC3_FIFO             : ARRAY[0..19] OF INT;
    BtlHead, BtlTail, BtlCount : INT := 0;

    // ---- Error Flags ----
    err_PatientLimitExceeded, err_DuplicatePatientOrder,
    err_ManualPickFifoFull, err_InvalidFillerCabinet,
    err_SorterCapacityExceeded : BOOL := FALSE;

    // ---- Sorter Parameters ----
    SorterAssignedLanes, SorterRemainingLanes, SorterRemainingCapacity : INT := 0;

    // ---- Sorter Lane Assignment ----
    LaneTote        : ARRAY[1..6] OF INT;    // Tote ID assigned to each lane
    LaneBottleCount : ARRAY[1..6] OF INT;    // Bottles remaining to fill per lane
    LaneAssigned    : ARRAY[1..6] OF BOOL;   // TRUE if lane occupied
    bPatientOrderInducted : BOOL := FALSE;

    // ---- Communication with Sorter ----
    g_iTargetLane : INT;           // command to sorter
    g_bMoveReq    : BOOL;          // trigger sorter move
    g_bSorterReady : BOOL;         // sorter ready for next bottle
    g_bBottleDroppedEvent : BOOL;  // sorter feedback when bottle dropped
    g_iBottleDroppedLane  : INT;
    g_iBottleDroppedTote  : INT;
    g_bLaneClearedEvent   : BOOL;  // sorter feedback when lane emptied
    g_iLaneCleared        : INT;

    // ---- Temporary flags ----
    bIsDuplicate : BOOL;
END_VAR


// ====================================================================
// INITIALIZATION (rungs 0–3)
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

    nSys_ToteInfo_NewDataArrival := 6;
    nSys_FillBtl_NewDataArrival := 6;
    nSys_BtlStat_NewDataArrival := 6;
    nSys_ToteEject_NewDataArrival := 6;
    nSys_Diag_NewDataArrival := 6;

    bInit := FALSE;
END_IF;


// ====================================================================
// 1️⃣ RECEIVE PATIENT ORDER / TOTE INFO  (rungs 4–18 + ReceivePatientOrder)
// ====================================================================
IF nSys_ToteInfo_NewDataArrival <> 1 THEN
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
        SorterRemainingLanes := 6 - SorterAssignedLanes;
        SorterRemainingCapacity := SorterRemainingLanes * 4; // each lane holds 4 bottles

        IF nSys_ToteInfo_NumberOfBottles > SorterRemainingCapacity THEN
            err_SorterCapacityExceeded := TRUE;
        ELSE
            // --- Push to Manual FIFO ---
            IF ManCount < MANUAL_FIFO_SIZE THEN
                Man_Tote_FIFO[ManHead]  := nSys_ToteInfo_ToteID;
                Man_Route_FIFO[ManHead] := nSys_ToteInfo_Route;
                ManHead := (ManHead + 1) MOD MANUAL_FIFO_SIZE;
                ManCount := ManCount + 1;

                // Add tote to active list
                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = 0 THEN
                        ActiveTotes[k] := nSys_ToteInfo_ToteID;
                        ActiveCount := ActiveCount + 1;
                        EXIT;
                    END_IF
                END_FOR

                // --- Assign sorter lanes (each holds 4 bottles) ---
                SelectedSlot := 0;
                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = nSys_ToteInfo_ToteID THEN
                        SelectedSlot := k;
                        EXIT;
                    END_IF
                END_FOR

                IF SelectedSlot > 0 THEN
                    BottleReq := nSys_ToteInfo_NumberOfBottles;
                    RouteCode := nSys_ToteInfo_Route;

                    FOR Lane := 1 TO 6 DO
                        IF (LaneAssigned[Lane] = FALSE) AND (BottleReq > 0) THEN
                            LaneTote[Lane] := nSys_ToteInfo_ToteID;
                            IF BottleReq <= 4 THEN
                                LaneBottleCount[Lane] := BottleReq;
                                BottleReq := 0;
                            ELSE
                                LaneBottleCount[Lane] := 4;
                                BottleReq := BottleReq - 4;
                            END_IF
                            LaneAssigned[Lane] := TRUE;
                        END_IF
                    END_FOR

                    IF BottleReq = 0 THEN
                        bPatientOrderInducted := TRUE;
                    END_IF
                END_IF
            ELSE
                err_ManualPickFifoFull := TRUE;
            END_IF
        END_IF
    END_IF

    nSys_ToteInfo_NewDataArrival := 6;
END_IF;


// ====================================================================
// 2️⃣ BOTTLE ORDER / FILLBTL  (rungs 19–29)
// Triggers sorter move to correct lane
// ====================================================================
IF nSys_FillBtl_NewDataArrival <> 1 THEN
    // find lane for this tote
    FOR Lane := 1 TO 6 DO
        IF LaneTote[Lane] = nSys_FillBtl_ToteID THEN
            IF g_bSorterReady THEN
                g_iTargetLane := Lane;
                g_bMoveReq := TRUE;   // one-shot move request
            END_IF
            EXIT;
        END_IF
    END_FOR

    // enqueue bottle into FIFO for recordkeeping
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

    nSys_FillBtl_NewDataArrival := 6;
END_IF;


// ====================================================================
// 3️⃣ SORTER FEEDBACK: bottle dropped + lane cleared
// ====================================================================
IF g_bBottleDroppedEvent THEN
    // decrement remaining bottles for that lane
    IF (g_iBottleDroppedLane >= 1) AND (g_iBottleDroppedLane <= 6) THEN
        IF LaneBottleCount[g_iBottleDroppedLane] > 0 THEN
            LaneBottleCount[g_iBottleDroppedLane] := LaneBottleCount[g_iBottleDroppedLane] - 1;
        END_IF

        // find tote index and decrement its overall count if needed
        FOR k := 1 TO 6 DO
            IF ActiveTotes[k] = g_iBottleDroppedTote THEN
                // no dedicated tote bottle counter here, so lane counts represent progress
                EXIT;
            END_IF
        END_FOR

        // if lane empty, sorter will handle unload request automatically
    END_IF
    g_bBottleDroppedEvent := FALSE;
END_IF

// --- Lane cleared feedback from sorter ---
IF g_bLaneClearedEvent THEN
    IF (g_iLaneCleared >= 1) AND (g_iLaneCleared <= 6) THEN
        LaneTote[g_iLaneCleared] := 0;
        LaneBottleCount[g_iLaneCleared] := 0;
        LaneAssigned[g_iLaneCleared] := FALSE;
    END_IF
    g_bLaneClearedEvent := FALSE;
END_IF


// ====================================================================
// 4️⃣ BOTTLE STATUS (rungs 30–34)
// ====================================================================
IF nSys_BtlStat_NewDataArrival <> 1 THEN
    CASE nSys_BtlStat_Status OF
        0: ; // OK
        1: ; // No-Read
        2: ; // Out-of-Sync
        ELSE
            ; // Unknown
    END_CASE
    nSys_BtlStat_NewDataArrival := 6;
END_IF;


// ====================================================================
// 5️⃣ TOTE EJECT (rungs 35–39)
// ====================================================================
IF nSys_ToteEject_NewDataArrival <> 1 THEN
    FOR k := 1 TO 6 DO
        IF ActiveTotes[k] = nSys_ToteEject_ToteID THEN
            ActiveTotes[k] := 0;
            IF ActiveCount > 0 THEN ActiveCount := ActiveCount - 1; END_IF

            // free all lanes for that tote
            FOR Lane := 1 TO 6 DO
                IF LaneTote[Lane] = nSys_ToteEject_ToteID THEN
                    LaneTote[Lane] := 0;
                    LaneBottleCount[Lane] := 0;
                    LaneAssigned[Lane] := FALSE;
                END_IF
            END_FOR
            EXIT;
        END_IF
    END_FOR
    nSys_ToteEject_NewDataArrival := 6;
END_IF;


// ====================================================================
// 6️⃣ DIAGNOSTICS / ERROR MESSAGES (rungs 51–62)
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
    nSys_Diag_DataWord[0] := SorterRemainingCapacity;
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

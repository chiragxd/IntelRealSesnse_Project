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

    // ---- Sorter Lane Assignment (from _31_ReceivePatientOrder) ----
    LaneTote        : ARRAY[1..6] OF INT;    // Tote ID assigned to each lane
    LaneBottleCount : ARRAY[1..6] OF INT;    // Bottles assigned to each lane
    LaneAssigned    : ARRAY[1..6] OF BOOL;   // TRUE if lane occupied
    bPatientOrderInducted : BOOL := FALSE;

    // ---- Temporary flags ----
    bIsDuplicate : BOOL;
END_VAR


// ====================================================================
// INITIALIZATION  (equivalent to Ladder rungs 0–3)
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
// 1️⃣ RECEIVE PATIENT ORDER / TOTE INFO   (rungs 4–18 + _31_ReceivePatientOrder)
// ====================================================================
IF nSys_ToteInfo_NewDataArrival <> 1 THEN
    bIsDuplicate := FALSE;

    // --- Duplicate check ---
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
        // --- Sorter capacity check ---
        SorterRemainingLanes := 6 - SorterAssignedLanes;
        SorterRemainingCapacity := SorterRemainingLanes * 5;

        IF nSys_ToteInfo_NumberOfBottles > SorterRemainingCapacity THEN
            err_SorterCapacityExceeded := TRUE;
        ELSE
            // --- Manual FIFO push ---
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

                // ==============================================
                // >>> RECEIVE PATIENT ORDER LOGIC (Lane Assignment)
                // ==============================================
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

                    // Assign sorter lanes (each holds 5 bottles)
                    FOR Lane := 1 TO 6 DO
                        IF (LaneAssigned[Lane] = FALSE) AND (BottleReq > 0) THEN
                            LaneTote[Lane] := nSys_ToteInfo_ToteID;
                            IF BottleReq <= 5 THEN
                                LaneBottleCount[Lane] := BottleReq;
                                BottleReq := 0;
                            ELSE
                                LaneBottleCount[Lane] := 5;
                                BottleReq := BottleReq - 5;
                            END_IF
                            LaneAssigned[Lane] := TRUE;
                        END_IF
                    END_FOR

                    // Mark induction complete
                    IF BottleReq = 0 THEN
                        bPatientOrderInducted := TRUE;
                    END_IF
                END_IF
                // ==============================================
            ELSE
                err_ManualPickFifoFull := TRUE;
            END_IF
        END_IF
    END_IF

    // --- Acknowledge handshake ---
    nSys_ToteInfo_NewDataArrival := 6;
END_IF;


// ====================================================================
// 2️⃣ BOTTLE ORDER / FILLBTL   (rungs 19–29)
// ====================================================================
IF nSys_FillBtl_NewDataArrival <> 1 THEN
    IF (nSys_FillBtl_Cabinet < 1) OR (nSys_FillBtl_Cabinet > 5) THEN
        err_InvalidFillerCabinet := TRUE;
    ELSE
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
    END_IF

    nSys_FillBtl_NewDataArrival := 6;
END_IF;


// ====================================================================
// 3️⃣ BOTTLE STATUS   (rungs 30–34)
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
// 4️⃣ TOTE EJECT   (rungs 35–39)
// ====================================================================
IF nSys_ToteEject_NewDataArrival <> 1 THEN
    FOR k := 1 TO 6 DO
        IF ActiveTotes[k] = nSys_ToteEject_ToteID THEN
            ActiveTotes[k] := 0;
            IF ActiveCount > 0 THEN
                ActiveCount := ActiveCount - 1;
            END_IF
            // Also free any lanes linked to this tote
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
// 5️⃣ DIAGNOSTICS / ERROR MESSAGES  (rungs 51–62)
// ====================================================================

// Patient Limit Exceeded
IF err_PatientLimitExceeded THEN
    nSys_Diag_LineID := 1;
    nSys_Diag_MessageID := 100;
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6;
    err_PatientLimitExceeded := FALSE;
END_IF;

// Duplicate Patient Order
IF err_DuplicatePatientOrder THEN
    nSys_Diag_LineID := 1;
    nSys_Diag_MessageID := 101;
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6;
    err_DuplicatePatientOrder := FALSE;
END_IF;

// Sorter Capacity Exceeded
IF err_SorterCapacityExceeded THEN
    nSys_Diag_LineID := 1;
    nSys_Diag_MessageID := 102;
    nSys_Diag_DataWord[0] := SorterRemainingCapacity;
    nSys_Diag_NewDataArrival := 6;
    err_SorterCapacityExceeded := FALSE;
END_IF;

// Invalid Filler Cabinet
IF err_InvalidFillerCabinet THEN
    nSys_Diag_LineID := 1;
    nSys_Diag_MessageID := 200;
    nSys_Diag_DataWord[0] := nSys_FillBtl_Cabinet;
    nSys_Diag_NewDataArrival := 6;
    err_InvalidFillerCabinet := FALSE;
END_IF;

// Manual Pick FIFO Full
IF err_ManualPickFifoFull THEN
    nSys_Diag_LineID := 1;
    nSys_Diag_MessageID := 104;
    nSys_Diag_DataWord[0] := ManCount;
    nSys_Diag_NewDataArrival := 6;
    err_ManualPickFifoFull := FALSE;
END_IF;

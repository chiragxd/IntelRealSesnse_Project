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
    bInit : BOOL := TRUE;
    i, k, Lane, BottleReq, RouteCode, SelectedSlot, Remaining : INT;

    ActiveTotes : ARRAY[1..6] OF INT := [0,0,0,0,0,0];
    ActiveCount : INT := 0;

    MANUAL_FIFO_SIZE : INT := 100;
    Man_Tote_FIFO : ARRAY[0..99] OF INT;
    Man_Route_FIFO : ARRAY[0..99] OF INT;
    ManHead, ManTail, ManCount : INT := 0;

    BTL_FIFO_SIZE : INT := 20;
    Btl_Tote_FIFO : ARRAY[0..19] OF INT;
    Btl_Cabinet_FIFO : ARRAY[0..19] OF INT;
    Btl_Qty_FIFO : ARRAY[0..19] OF INT;
    Btl_BC1_FIFO : ARRAY[0..19] OF INT;
    Btl_BC2_FIFO : ARRAY[0..19] OF INT;
    Btl_BC3_FIFO : ARRAY[0..19] OF INT;
    BtlHead, BtlTail, BtlCount : INT := 0;

    // ---- Error Flags ----
    err_PatientLimitExceeded, err_DuplicatePatientOrder,
    err_ManualPickFifoFull, err_InvalidFillerCabinet,
    err_SorterCapacityExceeded : BOOL := FALSE;

    // ---- Sorter Parameters ----
    SorterAssignedLanes, SorterRemainingLanes, SorterRemainingCapacity : INT := 0;

    // ---- Sorter Lane Assignment ----
    LaneTote        : ARRAY[1..6] OF INT;
    LaneBottleCount : ARRAY[1..6] OF INT;
    LaneAssigned    : ARRAY[1..6] OF BOOL;

    // ---- Communication with Sorter ----
    g_iTargetLane : INT;           
    g_bMoveReq    : BOOL;          
    g_bSorterReady : BOOL;         
    g_bBottleDroppedEvent : BOOL;  
    g_iBottleDroppedLane  : INT;
    g_iBottleDroppedTote  : INT;
    g_bLaneClearedEvent   : BOOL;  
    g_iLaneCleared        : INT;

    // ---- Debug Bits ----
    bDbg_Init                : BOOL := FALSE;
    bDbg_ReceiveOrder        : BOOL := FALSE;
    bDbg_AssignLanes         : BOOL := FALSE;
    bDbg_FillBottleOrder     : BOOL := FALSE;
    bDbg_MoveTrigger         : BOOL := FALSE;
    bDbg_BottleDropped       : BOOL := FALSE;
    bDbg_CheckOrderProgress  : BOOL := FALSE;
    bDbg_ToteEject           : BOOL := FALSE;

    // ---- Temporary ----
    bIsDuplicate : BOOL;
END_VAR


// ====================================================================
// 0️⃣ INITIALIZATION
// ====================================================================
bDbg_Init := TRUE;
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
// 1️⃣ RECEIVE PATIENT ORDER / TOTE INFO
// ====================================================================
bDbg_ReceiveOrder := FALSE;
IF nSys_ToteInfo_NewDataArrival <> 1 THEN
    bDbg_ReceiveOrder := TRUE;
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
        SorterRemainingCapacity := SorterRemainingLanes * 4;

        IF nSys_ToteInfo_NumberOfBottles > SorterRemainingCapacity THEN
            err_SorterCapacityExceeded := TRUE;
        ELSE
            IF ManCount < MANUAL_FIFO_SIZE THEN
                Man_Tote_FIFO[ManHead]  := nSys_ToteInfo_ToteID;
                Man_Route_FIFO[ManHead] := nSys_ToteInfo_Route;
                ManHead := (ManHead + 1) MOD MANUAL_FIFO_SIZE;
                ManCount := ManCount + 1;

                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = 0 THEN
                        ActiveTotes[k] := nSys_ToteInfo_ToteID;
                        ActiveCount := ActiveCount + 1;
                        EXIT;
                    END_IF
                END_FOR

                // --- Assign sorter lanes ---
                bDbg_AssignLanes := TRUE;
                SelectedSlot := 0;
                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = nSys_ToteInfo_ToteID THEN
                        SelectedSlot := k; EXIT;
                    END_IF
                END_FOR

                IF SelectedSlot > 0 THEN
                    BottleReq := nSys_ToteInfo_NumberOfBottles;
                    RouteCode := nSys_ToteInfo_Route;

                    FOR Lane := 1 TO 6 DO
                        IF (NOT LaneAssigned[Lane]) AND (BottleReq > 0) THEN
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
                END_IF
            ELSE
                err_ManualPickFifoFull := TRUE;
            END_IF
        END_IF
    END_IF

    nSys_ToteInfo_NewDataArrival := 6;
END_IF;


// ====================================================================
// 2️⃣ BOTTLE ORDER / FILLBTL
// ====================================================================
bDbg_FillBottleOrder := FALSE;
IF nSys_FillBtl_NewDataArrival <> 1 THEN
    bDbg_FillBottleOrder := TRUE;

    FOR Lane := 1 TO 6 DO
        IF LaneTote[Lane] = nSys_FillBtl_ToteID THEN
            IF g_bSorterReady THEN
                g_iTargetLane := Lane;
                g_bMoveReq := TRUE;
                bDbg_MoveTrigger := TRUE;   // Sorter move triggered
            END_IF
            EXIT;
        END_IF
    END_FOR

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
// 3️⃣ SORTER FEEDBACK: BOTTLE DROPPED + LANE CLEARED
// ====================================================================
bDbg_BottleDropped := FALSE;
IF g_bBottleDroppedEvent THEN
    bDbg_BottleDropped := TRUE;
    IF (g_iBottleDroppedLane >= 1) AND (g_iBottleDroppedLane <= 6) THEN
        IF LaneBottleCount[g_iBottleDroppedLane] > 0 THEN
            LaneBottleCount[g_iBottleDroppedLane] := 
                LaneBottleCount[g_iBottleDroppedLane] - 1;
        END_IF
    END_IF
    g_bBottleDroppedEvent := FALSE;
END_IF

IF g_bLaneClearedEvent THEN
    IF (g_iLaneCleared >= 1) AND (g_iLaneCleared <= 6) THEN
        LaneTote[g_iLaneCleared] := 0;
        LaneBottleCount[g_iLaneCleared] := 0;
        LaneAssigned[g_iLaneCleared] := FALSE;
    END_IF
    g_bLaneClearedEvent := FALSE;
END_IF;


// ====================================================================
// 4️⃣ ORDER PROCESSING → CHECK ORDER PROGRESS & TOTE EJECT
// ====================================================================
bDbg_CheckOrderProgress := TRUE;
FOR k := 1 TO 6 DO
    IF ActiveTotes[k] <> 0 THEN
        Remaining := 0;
        FOR Lane := 1 TO 6 DO
            IF LaneTote[Lane] = ActiveTotes[k] THEN
                Remaining := Remaining + LaneBottleCount[Lane];
            END_IF
        END_FOR

        IF Remaining = 0 THEN
            bDbg_ToteEject := TRUE;
            nSys_ToteEject_ToteID := ActiveTotes[k];
            nSys_ToteEject_Route  := nSys_ToteInfo_Route;
            nSys_ToteEject_NewDataArrival := 1;

            ActiveTotes[k] := 0;
            IF ActiveCount > 0 THEN ActiveCount := ActiveCount - 1; END_IF
        END_IF
    END_IF
END_FOR;


// ====================================================================
// 5️⃣ BOTTLE STATUS FEEDBACK
// ====================================================================
IF nSys_BtlStat_NewDataArrival <> 1 THEN
    CASE nSys_BtlStat_Status OF
        0: ;  // OK
        1: ;  // No-Read
        2: ;  // Out-of-Sync
        ELSE ; // Unknown
    END_CASE
    nSys_BtlStat_NewDataArrival := 6;
END_IF;


// ====================================================================
// 6️⃣ DIAGNOSTICS / ERRORS
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

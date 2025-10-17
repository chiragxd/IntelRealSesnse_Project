PROGRAM PRG_OFM
VAR
    // ===== System Inputs =====
    nSys_ToteInfo_NewDataArrival : INT;
    nSys_ToteInfo_ToteID, nSys_ToteInfo_NumberOfBottles, nSys_ToteInfo_Route : INT;
    nSys_FillBtl_NewDataArrival, nSys_FillBtl_Cabinet, nSys_FillBtl_ToteID,
    nSys_FillBtl_Quantity, nSys_FillBtl_Barcode1, nSys_FillBtl_Barcode2, nSys_FillBtl_Barcode3 : INT;
    nSys_BtlStat_NewDataArrival, nSys_BtlStat_ToteID, nSys_BtlStat_Status : INT;
    nSys_ToteEject_NewDataArrival, nSys_ToteEject_ToteID, nSys_ToteEject_Route : INT;

    // ===== Sorter Interface =====
    g_bSorterReady : BOOL;
    g_bMoveReq : BOOL;
    g_iTargetLane : INT;
    g_bBottleDroppedEvent : BOOL;
    g_iBottleDroppedLane, g_iBottleDroppedTote : INT;
    g_bLaneClearedEvent : BOOL;
    g_iLaneCleared : INT;

    // ===== Lane / Order Data =====
    ActiveTotes : ARRAY[1..6] OF INT := [0,0,0,0,0,0];
    LaneTote : ARRAY[1..6] OF INT;
    LaneBottleCount : ARRAY[1..6] OF INT;
    LaneAssigned : ARRAY[1..6] OF BOOL;
    ActiveCount : INT := 0;

    // ===== FIFO / Indices =====
    MANUAL_FIFO_SIZE : INT := 100;
    Man_Tote_FIFO : ARRAY[0..99] OF INT;
    Man_Route_FIFO : ARRAY[0..99] OF INT;
    ManHead, ManTail, ManCount : INT := 0;

    // ===== Internal & Errors =====
    i, k, Lane, BottleReq, freeLanes : INT;
    err_DuplicatePatientOrder, err_PatientLimitExceeded,
    err_SorterCapacityExceeded, err_InvalidFillerCabinet,
    err_ManualPickFifoFull : BOOL := FALSE;

    // ===== State Machine =====
    OFM_State : INT := 0;   // 0-INIT, 10-WAIT, 20-ASSIGN, 30-FILL, 40-TRIGGER, 50-MONITOR, 60-EJECT, 70-DIAG, 80-RESET
    StepDone : BOOL := FALSE;
    bInit : BOOL := TRUE;

    // ===== Constants =====
    LANE_CAPACITY : INT := 4;
END_VAR


// ===================================================================
// STATE MACHINE
// ===================================================================
CASE OFM_State OF

//--------------------------------------------------------------------
0: // OFM_INIT
    IF bInit THEN
        FOR i := 1 TO 6 DO
            ActiveTotes[i] := 0;
            LaneTote[i] := 0;
            LaneBottleCount[i] := 0;
            LaneAssigned[i] := FALSE;
        END_FOR
        ManHead := 0; ManTail := 0; ManCount := 0;
        ActiveCount := 0;
        err_DuplicatePatientOrder := FALSE;
        err_PatientLimitExceeded := FALSE;
        err_SorterCapacityExceeded := FALSE;
        err_InvalidFillerCabinet := FALSE;
        err_ManualPickFifoFull := FALSE;
        bInit := FALSE;
    END_IF
    OFM_State := 10;

//--------------------------------------------------------------------
10: // OFM_WAIT_ORDER
    IF nSys_ToteInfo_NewDataArrival <> 1 THEN
        StepDone := FALSE;
        // Check duplicate
        FOR k := 1 TO 6 DO
            IF ActiveTotes[k] = nSys_ToteInfo_ToteID THEN
                err_DuplicatePatientOrder := TRUE; StepDone := TRUE;
            END_IF
        END_FOR

        IF NOT StepDone THEN
            IF ActiveCount >= 6 THEN
                err_PatientLimitExceeded := TRUE; StepDone := TRUE;
            END_IF
        END_IF

        IF NOT StepDone THEN
            freeLanes := 0;
            FOR Lane := 1 TO 6 DO IF NOT LaneAssigned[Lane] THEN freeLanes := freeLanes + 1; END_IF END_FOR
            IF nSys_ToteInfo_NumberOfBottles > freeLanes * LANE_CAPACITY THEN
                err_SorterCapacityExceeded := TRUE; StepDone := TRUE;
            END_IF
        END_IF

        IF NOT StepDone THEN
            // store order to FIFO and Active list
            IF ManCount < MANUAL_FIFO_SIZE THEN
                Man_Tote_FIFO[ManHead] := nSys_ToteInfo_ToteID;
                Man_Route_FIFO[ManHead] := nSys_ToteInfo_Route;
                ManHead := (ManHead + 1) MOD MANUAL_FIFO_SIZE;
                ManCount := ManCount + 1;
                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = 0 THEN
                        ActiveTotes[k] := nSys_ToteInfo_ToteID; ActiveCount := ActiveCount + 1; EXIT;
                    END_IF
                END_FOR
                OFM_State := 20; // go to assign lanes
            ELSE
                err_ManualPickFifoFull := TRUE;
            END_IF
        END_IF
        // ACK
        nSys_ToteInfo_NewDataArrival := 6;
    END_IF

//--------------------------------------------------------------------
20: // OFM_ASSIGN_LANES
    BottleReq := nSys_ToteInfo_NumberOfBottles;
    FOR Lane := 1 TO 6 DO
        IF (BottleReq <= 0) THEN EXIT; END_IF
        IF NOT LaneAssigned[Lane] THEN
            LaneTote[Lane] := nSys_ToteInfo_ToteID;
            IF BottleReq <= LANE_CAPACITY THEN
                LaneBottleCount[Lane] := BottleReq; BottleReq := 0;
            ELSE
                LaneBottleCount[Lane] := LANE_CAPACITY; BottleReq := BottleReq - LANE_CAPACITY;
            END_IF
            LaneAssigned[Lane] := TRUE;
        END_IF
    END_FOR
    OFM_State := 30;

//--------------------------------------------------------------------
30: // OFM_WAIT_FILLBOTTLE
    IF nSys_FillBtl_NewDataArrival <> 1 THEN
        IF (nSys_FillBtl_Cabinet < 1) OR (nSys_FillBtl_Cabinet > 5) THEN
            err_InvalidFillerCabinet := TRUE;
        ELSE
            OFM_State := 40;
        END_IF
        nSys_FillBtl_NewDataArrival := 6;
    END_IF

//--------------------------------------------------------------------
40: // OFM_TRIGGER_SORTER
    IF g_bSorterReady THEN
        FOR Lane := 1 TO 6 DO
            IF LaneAssigned[Lane] AND (LaneTote[Lane] = nSys_FillBtl_ToteID) AND (LaneBottleCount[Lane] > 0) THEN
                g_iTargetLane := Lane;
                g_bMoveReq := TRUE;
                EXIT;
            END_IF
        END_FOR
        OFM_State := 50;
    END_IF

//--------------------------------------------------------------------
50: // OFM_MONITOR_SORTER
    // Bottle dropped
    IF g_bBottleDroppedEvent THEN
        IF (g_iBottleDroppedLane >= 1) AND (g_iBottleDroppedLane <= 6) THEN
            IF LaneAssigned[g_iBottleDroppedLane] AND (LaneBottleCount[g_iBottleDroppedLane] > 0) THEN
                LaneBottleCount[g_iBottleDroppedLane] := LaneBottleCount[g_iBottleDroppedLane] - 1;
            END_IF
        END_IF
        g_bBottleDroppedEvent := FALSE;
    END_IF

    // Lane cleared
    IF g_bLaneClearedEvent THEN
        IF (g_iLaneCleared >= 1) AND (g_iLaneCleared <= 6) THEN
            LaneTote[g_iLaneCleared] := 0;
            LaneBottleCount[g_iLaneCleared] := 0;
            LaneAssigned[g_iLaneCleared] := FALSE;
        END_IF
        g_bLaneClearedEvent := FALSE;
    END_IF

    // Check completion
    FOR k := 1 TO 6 DO
        IF ActiveTotes[k] <> 0 THEN
            freeLanes := 0;
            FOR Lane := 1 TO 6 DO
                IF LaneAssigned[Lane] AND (LaneTote[Lane] = ActiveTotes[k]) THEN
                    freeLanes := freeLanes + LaneBottleCount[Lane];
                END_IF
            END_FOR
            IF freeLanes = 0 THEN
                nSys_ToteEject_ToteID := ActiveTotes[k];
                nSys_ToteEject_NewDataArrival := 1;
                OFM_State := 60;
            END_IF
        END_IF
    END_FOR

//--------------------------------------------------------------------
60: // OFM_EJECT_TOTE
    // Wait for host to acknowledge
    IF nSys_ToteEject_NewDataArrival = 6 THEN
        // remove tote
        FOR k := 1 TO 6 DO
            IF ActiveTotes[k] = nSys_ToteEject_ToteID THEN
                ActiveTotes[k] := 0;
                IF ActiveCount > 0 THEN ActiveCount := ActiveCount - 1; END_IF
            END_IF
        END_FOR
        OFM_State := 70;
    END_IF

//--------------------------------------------------------------------
70: // OFM_DIAGNOSTICS
    // Send errors if any
    IF err_DuplicatePatientOrder OR err_PatientLimitExceeded OR err_SorterCapacityExceeded OR err_InvalidFillerCabinet THEN
        // set diag fields here
    END_IF
    OFM_State := 80;

//--------------------------------------------------------------------
80: // OFM_RESET_IDLE
    IF ActiveCount = 0 THEN
        FOR Lane := 1 TO 6 DO
            LaneTote[Lane] := 0; LaneBottleCount[Lane] := 0; LaneAssigned[Lane] := FALSE;
        END_FOR
        OFM_State := 10; // back to wait for next order
    END_IF

END_CASE

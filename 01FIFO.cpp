// ======================================================================
// VARIABLE DECLARATIONS
// ======================================================================
VAR
    MaxTODO : INT := 5000;
    MaxWIP  : INT := 6;
    MaxProcessed : INT := 5000;

    iTODO_Tail : INT := 1;
    iProcessedTail : INT := 1;

    bNewOrderFromPPS : INT;
    zDebugAuto : ARRAY[0..100] OF BOOL;
    bMachineReady : BOOL; // signal to start next active order

    nSys_ToteInfo_NewDataArrival : INT;
    nSys_FillBtl_NewDataArrival  : INT;

    // Main data arrays
    aTODO : ARRAY [1..5000] OF ST_Order;
    aWIP  : ARRAY [1..6] OF ST_Order;
    aProcessedOrders : ARRAY [1..5000] OF ST_Order;

    // Single-order stages
    aActiveOrder : ST_Order;
    aToBeFilled  : ST_Order;

    // Loop variables
    i, j, k : INT;
END_VAR


// ======================================================================
// STEP 1: RECEIVE ORDERS FROM PPS → aTODO
// ======================================================================
CASE bNewOrderFromPPS OF
    0:
        IF (iTODO_Tail < MaxTODO) AND (nSys_ToteInfo_NewDataArrival = 1) AND zDebugAuto[59] THEN
            aTODO[iTODO_Tail].nSys_Kepware_Data.ToteInfo := nSys.ToteInfo;
            nSys_ToteInfo_NewDataArrival := 6;
            bNewOrderFromPPS := 10;
        END_IF

    10:
        IF nSys_FillBtl_NewDataArrival = 1 THEN
            aTODO[iTODO_Tail].nSys_Kepware_Data.FillBtl := nSys.FillBtl;
            aTODO[iTODO_Tail].Active := TRUE;

            nSys_FillBtl_NewDataArrival := 6;
            iTODO_Tail := iTODO_Tail + 1;
            IF iTODO_Tail >= MaxTODO THEN
                iTODO_Tail := MaxTODO - 1;
            END_IF

            zDebugAuto[59] := FALSE;
            bNewOrderFromPPS := 0;
        END_IF
END_CASE


// ======================================================================
// STEP 2: FILL WIP UP TO 6 ORDERS FROM TODO
// ======================================================================
FOR i := 1 TO MaxWIP DO
    IF NOT aWIP[i].Active THEN
        FOR j := 1 TO MaxTODO - 1 DO
            IF aTODO[j].Active THEN
                aWIP[i] := aTODO[j];
                aWIP[i].Active := TRUE;
                aTODO[j].Active := FALSE;

                // Shift TODO FIFO up
                FOR k := j TO MaxTODO - 2 DO
                    aTODO[k] := aTODO[k + 1];
                END_FOR
                aTODO[MaxTODO - 1].Active := FALSE;

                IF iTODO_Tail > 0 THEN
                    iTODO_Tail := iTODO_Tail - 1;
                END_IF
                EXIT;
            END_IF
        END_FOR
    END_IF
END_FOR


// ======================================================================
// STEP 3: MOVE WIP[1] → aActiveOrder WHEN READY
// ======================================================================
IF (NOT aActiveOrder.Active) AND aWIP[1].Active AND bMachineReady THEN
    aActiveOrder := aWIP[1];
    aActiveOrder.Active := TRUE;
    aWIP[1].Active := FALSE;

    // Shift remaining WIP up
    FOR i := 1 TO MaxWIP - 1 DO
        aWIP[i] := aWIP[i + 1];
    END_FOR
    aWIP[MaxWIP].Active := FALSE;

    // Refill last WIP slot from TODO
    FOR j := 1 TO MaxTODO - 1 DO
        IF aTODO[j].Active THEN
            aWIP[MaxWIP] := aTODO[j];
            aWIP[MaxWIP].Active := TRUE;
            aTODO[j].Active := FALSE;
            FOR k := j TO MaxTODO - 2 DO
                aTODO[k] := aTODO[k + 1];
            END_FOR
            aTODO[MaxTODO - 1].Active := FALSE;
            IF iTODO_Tail > 0 THEN
                iTODO_Tail := iTODO_Tail - 1;
            END_IF
            EXIT;
        END_IF
    END_FOR
END_IF


// ======================================================================
// STEP 4: MOVE aActiveOrder → aToBeFilled WHEN MACHINE DONE
// ======================================================================
IF aActiveOrder.Active AND aActiveOrder.StageComplete THEN
    aToBeFilled := aActiveOrder;
    aToBeFilled.Active := TRUE;
    aActiveOrder.Active := FALSE;
    aActiveOrder.StageComplete := FALSE;
END_IF


// ======================================================================
// STEP 5: MOVE aToBeFilled → aProcessedOrders WHEN FILL COMPLETE
// ======================================================================
IF aToBeFilled.Active AND aToBeFilled.FillComplete THEN
    aProcessedOrders[iProcessedTail] := aToBeFilled;
    aProcessedOrders[iProcessedTail].Active := TRUE;
    iProcessedTail := iProcessedTail + 1;
    IF iProcessedTail >= MaxProcessed THEN
        iProcessedTail := 1; // wrap around or stop as needed
    END_IF

    // Clear aToBeFilled
    aToBeFilled.Active := FALSE;
    aToBeFilled.FillComplete := FALSE;

    // After processing, promote next WIP[1] → aActiveOrder automatically
    IF aWIP[1].Active THEN
        aActiveOrder := aWIP[1];
        aActiveOrder.Active := TRUE;
        aWIP[1].Active := FALSE;

        // Shift remaining WIP up
        FOR i := 1 TO MaxWIP - 1 DO
            aWIP[i] := aWIP[i + 1];
        END_FOR
        aWIP[MaxWIP].Active := FALSE;

        // Refill last WIP slot from TODO
        FOR j := 1 TO MaxTODO - 1 DO
            IF aTODO[j].Active THEN
                aWIP[MaxWIP] := aTODO[j];
                aWIP[MaxWIP].Active := TRUE;
                aTODO[j].Active := FALSE;

                FOR k := j TO MaxTODO - 2 DO
                    aTODO[k] := aTODO[k + 1];
                END_FOR
                aTODO[MaxTODO - 1].Active := FALSE;

                IF iTODO_Tail > 0 THEN
                    iTODO_Tail := iTODO_Tail - 1;
                END_IF
                EXIT;
            END_IF
        END_FOR
    END_IF
END_IF

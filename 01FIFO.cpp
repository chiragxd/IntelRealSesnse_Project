// ======================================================================
// VARIABLE DECLARATIONS
// ======================================================================
VAR
    MaxTODO : INT := 5000;
    MaxWIP  : INT := 6;
    MaxProcessed : INT := 5000;

    // Index counters
    iTODO_Tail : INT := 1;
    iProcessedTail : INT := 1;

    // State variables for each process
    bNewOrderFromPPS : INT;   // Handles TODO creation
    bTODOtoWIP : INT;         // Handles TODO → WIP
    bWIPtoActive : INT;       // Handles WIP → Active
    bActiveToFill : INT;      // Handles Active → ToBeFilled
    bFillToProcessed : INT;   // Handles ToBeFilled → Processed

    // Flags
    bMachineReady : BOOL;
    nSys_ToteInfo_NewDataArrival : INT;
    nSys_FillBtl_NewDataArrival  : INT;

    // Order arrays
    aTODO : ARRAY [1..5000] OF ST_Order;
    aWIP  : ARRAY [1..6] OF ST_Order;
    aProcessedOrders : ARRAY [1..5000] OF ST_Order;

    // Single stages
    aActiveOrder : ST_Order;
    aToBeFilled  : ST_Order;

    // Loop vars
    i, j, k : INT;
END_VAR


// ======================================================================
// CASE 1: ADD ORDERS FROM PPS → aTODO
// ======================================================================
CASE bNewOrderFromPPS OF
    0:
        IF (iTODO_Tail < MaxTODO) AND (nSys_ToteInfo_NewDataArrival = 1) THEN
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

            bNewOrderFromPPS := 0;
        END_IF
END_CASE



// ======================================================================
// CASE 2: MOVE TODO → WIP (KEEP WIP FILLED UP TO 6)
// ======================================================================
CASE bTODOtoWIP OF
    0:
        // Always try to fill WIP when there’s space
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
END_CASE



// ======================================================================
// CASE 3: MOVE WIP[1] → aActiveOrder
// ======================================================================
CASE bWIPtoActive OF
    0:
        IF (NOT aActiveOrder.Active) AND aWIP[1].Active AND bMachineReady THEN
            aActiveOrder := aWIP[1];
            aActiveOrder.Active := TRUE;
            aWIP[1].Active := FALSE;

            // Shift WIP FIFO up
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
END_CASE



// ======================================================================
// CASE 4: MOVE aActiveOrder → aToBeFilled (WHEN MACHINE DONE)
// ======================================================================
CASE bActiveToFill OF
    0:
        IF aActiveOrder.Active AND aActiveOrder.StageComplete THEN
            aToBeFilled := aActiveOrder;
            aToBeFilled.Active := TRUE;
            aActiveOrder.Active := FALSE;
            aActiveOrder.StageComplete := FALSE;
        END_IF
END_CASE



// ======================================================================
// CASE 5: MOVE aToBeFilled → aProcessedOrders
// ======================================================================
CASE bFillToProcessed OF
    0:
        IF aToBeFilled.Active AND aToBeFilled.FillComplete THEN
            aProcessedOrders[iProcessedTail] := aToBeFilled;
            aProcessedOrders[iProcessedTail].Active := TRUE;
            iProcessedTail := iProcessedTail + 1;
            IF iProcessedTail >= MaxProcessed THEN
                iProcessedTail := 1;
            END_IF

            // Clear ToBeFilled
            aToBeFilled.Active := FALSE;
            aToBeFilled.FillComplete := FALSE;

            // After finishing, promote next WIP → Active
            IF aWIP[1].Active THEN
                aActiveOrder := aWIP[1];
                aActiveOrder.Active := TRUE;
                aWIP[1].Active := FALSE;

                // Shift WIP FIFO up
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
END_CASE

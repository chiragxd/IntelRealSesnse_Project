// ======================================================================
// VARIABLE DECLARATIONS
// ======================================================================
VAR
    MaxTODO : INT := 5000;
    MaxWIP  : INT := 6;

    iTODO_Tail : INT := 1;

    // Flags
    bNewOrderFromPPS : INT;
    zDebugAuto : ARRAY[0..100] OF BOOL;
    bMachineReady : BOOL; // Optional - signal when system can move to next active order

    // PPS communication flags
    nSys_ToteInfo_NewDataArrival : INT;
    nSys_FillBtl_NewDataArrival  : INT;

    // Order arrays
    aTODO : ARRAY [1..5000] OF ST_Order;
    aWIP  : ARRAY [1..6] OF ST_Order;
    aActiveOrder : ST_Order;

    // Temporary / control
    i, j, k : INT;
END_VAR

// ======================================================================
// ADDING NEW ORDERS FROM PPS INTO aTODO
// ======================================================================
CASE bNewOrderFromPPS OF
    0: // Step 1: Tote Info
        IF (iTODO_Tail < MaxTODO) AND (nSys_ToteInfo_NewDataArrival = 1) AND zDebugAuto[59] THEN
            aTODO[iTODO_Tail].nSys_Kepware_Data.ToteInfo := nSys.ToteInfo;
            nSys_ToteInfo_NewDataArrival := 6;   // Ack
            bNewOrderFromPPS := 10;
        END_IF

    10: // Step 2: Fill Bottle Info
        IF nSys_FillBtl_NewDataArrival = 1 THEN
            aTODO[iTODO_Tail].nSys_Kepware_Data.FillBtl := nSys.FillBtl;
            aTODO[iTODO_Tail].Active := TRUE;

            nSys_FillBtl_NewDataArrival := 6; // Ack
            iTODO_Tail := iTODO_Tail + 1;
            IF iTODO_Tail >= MaxTODO THEN
                iTODO_Tail := MaxTODO - 1;
            END_IF

            zDebugAuto[59] := FALSE;
            bNewOrderFromPPS := 0; // Ready for next order
        END_IF
END_CASE


// ======================================================================
// FILL WIP SLOTS (UP TO 6) FROM TODO WHENEVER THERE'S SPACE
// ======================================================================
FOR i := 1 TO MaxWIP DO
    IF NOT aWIP[i].Active THEN
        // Find next valid order in TODO
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
                EXIT; // Stop once one order moved
            END_IF
        END_FOR
    END_IF
END_FOR


// ======================================================================
// MOVE FIRST WIP → ACTIVE ORDER WHEN ACTIVE IS FREE
// ======================================================================
IF (NOT aActiveOrder.Active) AND aWIP[1].Active AND bMachineReady THEN
    // Move to active
    aActiveOrder := aWIP[1];
    aActiveOrder.Active := TRUE;
    aWIP[1].Active := FALSE;

    // Shift remaining WIP orders up
    FOR i := 1 TO MaxWIP - 1 DO
        aWIP[i] := aWIP[i + 1];
    END_FOR
    aWIP[MaxWIP].Active := FALSE;

    // Refill last WIP slot from TODO if available
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
// WHEN ACTIVE ORDER COMPLETES → CLEAR AND PROMOTE NEXT WIP
// ======================================================================
IF aActiveOrder.Active AND aActiveOrder.Completed THEN
    aActiveOrder.Active := FALSE;
    aActiveOrder.Completed := FALSE;

    // Move next available WIP order into Active
    IF aWIP[1].Active THEN
        aActiveOrder := aWIP[1];
        aActiveOrder.Active := TRUE;
        aWIP[1].Active := FALSE;

        // Shift remaining WIP up
        FOR i := 1 TO MaxWIP - 1 DO
            aWIP[i] := aWIP[i + 1];
        END_FOR
        aWIP[MaxWIP].Active := FALSE;

        // Refill last WIP slot from TODO if available
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

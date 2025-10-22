PROGRAM PRG_OFM_FIFO
VAR
    // ====== Input Signals from PPS / Kepware ======
    nSys_ToteInfo_NewDataArrival : INT;   // 1 = New order available
    nSys_FillBtl_NewDataArrival  : INT;   // 1 = Fill details available
    nFBC_NewDataArrival          : INT;   // Fill complete feedback

    // ====== Order Data ======
    TYPE ST_Order :
    STRUCT
        OrderID      : DINT;
        Barcode      : STRING(50);
        Quantity     : DINT;
        BottleSize   : INT;   // 0 = small, 1 = large
        CabinetNo    : INT;
        Valid        : BOOL;
    END_STRUCT
    END_TYPE

    MaxTODO : INT := 10;
    MaxWIP  : INT := 4;

    aTODO : ARRAY [0..9] OF ST_Order; // Waiting orders
    aWIP  : ARRAY [0..3] OF ST_Order; // Active orders
    ActiveOrder : ST_Order;

    // ====== Process status ======
    bSmallBottleSelected : BOOL;
    bLargeBottleSelected : BOOL;
    BarcodeGoodRead : BOOL;
    BarcodeNoRead : BOOL;
    sBarcode : STRING(50);
    iBottleStatus : INT; // 1=good, 2=bad print, 3=cap reject

    // ====== Testing inputs ======
    PPSOrder : ST_Order;       // incoming new order
    DummyCompleted : BOOL;     // simulate completion
END_VAR


// =======================================================
// STEP 1: ACCEPT NEW ORDERS FROM PPS → ADD TO TODO FIFO
// =======================================================

IF nSys_ToteInfo_NewDataArrival = 1 THEN
    // Find first empty slot in TODO
    FOR i := 0 TO MaxTODO - 1 DO
        IF NOT aTODO[i].Valid THEN
            aTODO[i] := PPSOrder;
            aTODO[i].Valid := TRUE;
            EXIT;
        END_IF
    END_FOR
    nSys_ToteInfo_NewDataArrival := 6; // Acknowledge received
END_IF


// =======================================================
// STEP 2: MOVE TODO → WIP (if WIP has space)
// =======================================================

FOR i := 0 TO MaxWIP - 1 DO
    IF NOT aWIP[i].Valid THEN
        // Find first valid TODO
        FOR j := 0 TO MaxTODO - 1 DO
            IF aTODO[j].Valid THEN
                aWIP[i] := aTODO[j];
                aWIP[i].Valid := TRUE;
                aTODO[j].Valid := FALSE;

                // Shift remaining TODO up (FIFO behavior)
                FOR k := j TO MaxTODO - 2 DO
                    aTODO[k] := aTODO[k + 1];
                END_FOR
                aTODO[MaxTODO - 1].Valid := FALSE;
                EXIT;
            END_IF
        END_FOR
    END_IF
END_FOR


// =======================================================
// STEP 3: PROCESS ACTIVE WIP ORDER (FIRST ONE)
// =======================================================

IF aWIP[0].Valid THEN
    ActiveOrder := aWIP[0];

    // Select bottle size
    IF ActiveOrder.BottleSize = 0 THEN
        bSmallBottleSelected := TRUE;
        bLargeBottleSelected := FALSE;
    ELSE
        bLargeBottleSelected := TRUE;
        bSmallBottleSelected := FALSE;
    END_IF

    // ----- Simulate print + barcode step -----
    IF BarcodeGoodRead THEN
        IF ActiveOrder.Barcode = sBarcode THEN
            iBottleStatus := 1; // Good print
        ELSE
            iBottleStatus := 2; // Wrong barcode
        END_IF
    ELSIF BarcodeNoRead THEN
        iBottleStatus := 2; // No read = bad
    END_IF

    // ----- Simulate fill process -----
    IF nFBC_NewDataArrival = 1 THEN
        // Compare expected vs actual quantity
        IF ActiveOrder.Quantity = nFBC_Quantity THEN
            iBottleStatus := 1; // Good fill
        ELSE
            iBottleStatus := 3; // Cap reject
        END_IF
        nFBC_NewDataArrival := 0;
    END_IF

    // ----- If order completed -----
    IF DummyCompleted OR (iBottleStatus IN [1,2,3]) THEN
        // Remove order from WIP (FIFO shift up)
        FOR m := 0 TO MaxWIP - 2 DO
            aWIP[m] := aWIP[m + 1];
        END_FOR
        aWIP[MaxWIP - 1].Valid := FALSE;
        iBottleStatus := 0;
        DummyCompleted := FALSE;
    END_IF
END_IF


// =======================================================
// STEP 4: REFILL WIP IF EMPTY (always keep full if possible)
// =======================================================

FOR i := 0 TO MaxWIP - 1 DO
    IF NOT aWIP[i].Valid THEN
        FOR j := 0 TO MaxTODO - 1 DO
            IF aTODO[j].Valid THEN
                aWIP[i] := aTODO[j];
                aWIP[i].Valid := TRUE;
                aTODO[j].Valid := FALSE;

                // Shift TODO FIFO up
                FOR k := j TO MaxTODO - 2 DO
                    aTODO[k] := aTODO[k + 1];
                END_FOR
                aTODO[MaxTODO - 1].Valid := FALSE;
                EXIT;
            END_IF
        END_FOR
    END_IF
END_FOR

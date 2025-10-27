VAR
    MaxOrders : INT := 5000;
    MaxWIP    : INT := 10;

    aTODOOrders : ARRAY[1..5000] OF ST_ToteInfo;  // incoming orders
    aWIPOrders  : ARRAY[1..10] OF ST_ToteInfo;    // active orders in progress

    i, j : INT;
END_VAR


// =====================================================
// MOVE FIRST ORDER (FIFO) FROM TODO → WIP
// =====================================================
IF aTODOOrders[1].ToteID <> 0 THEN
    // Find a free slot in WIP
    FOR i := 1 TO MaxWIP DO
        IF aWIPOrders[i].ToteID = 0 THEN
            // Move first TODO order into WIP
            aWIPOrders[i] := aTODOOrders[1];

            // Clear first TODO order
            aTODOOrders[1].ToteID := 0;

            // Shift all remaining orders up by one
            FOR j := 1 TO MaxOrders - 1 DO
                aTODOOrders[j] := aTODOOrders[j + 1];
            END_FOR

            // Clear the last element after shift
            aTODOOrders[MaxOrders].ToteID := 0;

            EXIT; // stop after moving one order
        END_IF
    END_FOR
END_IF

VAR
    MaxDetails : INT := 5000;
    MaxWIP     : INT := 10;

    aTODOOrderDetails : ARRAY[1..5000] OF ST_OrderDetail;
    aWIP              : ARRAY[1..10] OF ST_OrderDetail;

    i, j : INT;
END_VAR

// ============================================================
// MOVE FIRST ACTIVE ORDER DETAIL FROM TODO → WIP
// ============================================================
FOR i := 1 TO MaxDetails DO
    IF aTODOOrderDetails[i].Active THEN
        // 1️⃣ Find an empty WIP slot
        FOR j := 1 TO MaxWIP DO
            IF NOT aWIP[j].Active THEN
                aWIP[j] := aTODOOrderDetails[i];   // copy full detail record
                aWIP[j].Active := TRUE;
                EXIT;                              // leave WIP loop
            END_IF
        END_FOR

        // 2️⃣ Clear this slot in TODO
        aTODOOrderDetails[i].Active := FALSE;

        // 3️⃣ Shift remaining orders up to fill the gap
        FOR j := i TO MaxDetails - 1 DO
            aTODOOrderDetails[j] := aTODOOrderDetails[j + 1];
        END_FOR

        // 4️⃣ Clear the last slot after shifting
        aTODOOrderDetails[MaxDetails].Active := FALSE;

        // 5️⃣ Exit after moving one record
        EXIT;
    END_IF
END_FOR

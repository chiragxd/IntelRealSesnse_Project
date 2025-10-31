//===========================================================
// FINAL OPTIFILL ORDER & BOTTLE FIFO HANDLER
//===========================================================

VAR
    // ==== FLAGS & INIT TRIGGERS ====
    rInitTrig : R_TRIG;
    bInit     : BOOL := FALSE;
    bInit2    : BOOL := FALSE;

    // ==== ORDER FIFO ====
    fbTODOOrdersFifo : FB_MemRingBuffer;
    arrTODOBuffer    : ARRAY[0..SIZEOF(ST_Order) * 1000] OF BYTE;
    stNewOrder       : ST_Order;
    stNextOrder      : ST_Order;
    NextOrderPreview : ST_Order;
    bOk, bOk2, bOk3  : BOOL;

    // ==== ORDER DETAILS FIFO ====
    fbTODOOrderDetailsFifo : FB_MemRingBuffer;
    arrTODODetailsBuffer   : ARRAY[0..SIZEOF(ST_OrderDetails) * 1000] OF BYTE;
    stNewOrderDetails      : ST_OrderDetails;
    stNextOrderDetails     : ST_OrderDetails;

    // ==== FILLED ORDERS FIFO ====
    fbFilledOrdersFifo     : FB_MemRingBuffer;
    arrFilledOrdersBuffer  : ARRAY[0..SIZEOF(ST_Order) * 5000] OF BYTE;
    stFilledOrder          : ST_Order;
    stNextFilled           : ST_Order;
    iFilledCount           : INT := 1;
    MaxFilled              : INT := 5000;

    // ==== ARRAYS ====
    aWIPOrders             : ARRAY[1..6] OF ST_Order;     // per lane active orders
    aActiveOrder           : ST_OrderDetails;             // active bottle order
    aWIP                   : ARRAY[1..6] OF ST_OrderDetails; // bottle WIP for cabinet transfer
    aFilledOrders          : ARRAY[1..5000] OF ST_Order;   // static display copy

    // ==== STATUS ====
    TotalTODOCount : UDINT;
    TotalActiveWIP : INT;
    TotalFreeLanes : INT;
    LaneStatusText : ARRAY[1..6] OF STRING(40);

END_VAR


//===========================================================
// 1️⃣ INITIATION TRIGGER
//===========================================================
rInitTrig(CLK := bCycleStart);

IF rInitTrig.Q THEN
    bInit := TRUE;
    bInit2 := TRUE;
END_IF


//===========================================================
// 2️⃣ INITIALIZE FIFOs ON STARTUP
//===========================================================
IF bInit THEN
    fbTODOOrdersFifo(
        pBuffer := ADR(arrTODOBuffer),
        cbBuffer := SIZEOF(arrTODOBuffer)
    );
    fbTODOOrdersFifo.A_Reset(bOk => bOk);
    bInit := FALSE;
END_IF

IF bInit2 THEN
    fbTODOOrderDetailsFifo(
        pBuffer := ADR(arrTODODetailsBuffer),
        cbBuffer := SIZEOF(arrTODODetailsBuffer)
    );
    fbTODOOrderDetailsFifo.A_Reset(bOk => bOk2);

    fbFilledOrdersFifo(
        pBuffer := ADR(arrFilledOrdersBuffer),
        cbBuffer := SIZEOF(arrFilledOrdersBuffer)
    );
    fbFilledOrdersFifo.A_Reset(bOk => bOk3);

    bInit2 := FALSE;
END_IF


//===========================================================
// 3️⃣ ADD ORDERS TO FIFO FROM PPS
//===========================================================
IF nSys_ToteInfo_NewDataArrival = 1 THEN
    stNewOrder.ToteID          := nSys.ToteInfo.ToteID;
    stNewOrder.NumberOfBottles := nSys.ToteInfo.NumberOfBottles;
    stNewOrder.Route           := nSys.ToteInfo.Route;
    stNewOrder.LaneAssigned    := 0;
    stNewOrder.Active          := FALSE;

    fbTODOOrdersFifo.A_AddTail(
        pWrite  := ADR(stNewOrder),
        cbWrite := SIZEOF(stNewOrder),
        bOk     => bOk
    );

    nSys_ToteInfo_NewDataArrival := 6;
END_IF


//===========================================================
// 4️⃣ MOVE ORDERS FROM FIFO → WIP (AUTO MODE)
//===========================================================
IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) OR bCycleStart THEN
    FOR iLane := 1 TO 6 DO
        IF NOT aWIPOrders[iLane].Active THEN
            fbTODOOrdersFifo.A_RemoveHead(
                pRead  := ADR(stNextOrder),
                cbRead := SIZEOF(stNextOrder),
                bOk    => bOk
            );

            IF bOk THEN
                aWIPOrders[iLane].nSys_OrderInfo := stNextOrder;
                aWIPOrders[iLane].Active := TRUE;
                aWIPOrders[iLane].LaneAssigned := iLane;

                aSorterLaneToteID[iLane] := aWIPOrders[iLane].nSys_OrderInfo.ToteID;
                aSorterLaneAssigned[iLane] := TRUE;
                bSorterLaneOn[iLane] := TRUE;
            END_IF
        END_IF
    END_FOR
END_IF


//===========================================================
// 5️⃣ ADD ORDER DETAILS FROM PPS (BOTTLES DATA)
//===========================================================
IF nSys_FillBtl_NewDataArrival = 1 THEN
    stNewOrderDetails.nSys_FillBtlData := nSys.FillBtl;
    stNewOrderDetails.Active := FALSE;

    fbTODOOrderDetailsFifo.A_AddTail(
        pWrite  := ADR(stNewOrderDetails),
        cbWrite := SIZEOF(stNewOrderDetails),
        bOk     => bOk2
    );

    nSys_FillBtl_NewDataArrival := 6;
END_IF


//===========================================================
// 6️⃣ LOAD NEXT ACTIVE BOTTLE INTO aActiveOrder
//===========================================================
IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) OR bCycleStart THEN
    IF NOT aActiveOrder.Active THEN
        fbTODOOrderDetailsFifo.A_RemoveHead(
            pRead  := ADR(stNextOrderDetails),
            cbRead := SIZEOF(stNextOrderDetails),
            bOk    => bOk2
        );

        IF bOk2 THEN
            aActiveOrder.nSys_Kepware_Data.FillBtl := stNextOrderDetails.nSys_FillBtlData;
            aActiveOrder.Active := TRUE;
        END_IF
    END_IF
END_IF


//===========================================================
// 7️⃣ TRANSFER FROM LABELER TO WIP[1] (BOTTLE STAGE)
//===========================================================
IF aActiveOrder.Active AND bLabelerDone AND bEscapementDone AND bTransferServoAtLabeler AND bBottleTransferGripperClosed THEN
    aWIP[1] := aActiveOrder;
    aWIP[1].Active := TRUE;
    iBottleStatusAtCabinet := iBottleStatusAtLabeler;
    iBottleStatusAtLabeler := 0;
    aActiveOrder.Active := FALSE;
END_IF


//===========================================================
// 8️⃣ AT CAPPER — MOVE TO FILLED FIFO & INCREMENT COUNT
//===========================================================
IF aWIP[1].Active AND bTransferServoAtCapper THEN
    FOR iLane := 1 TO 6 DO
        IF aWIPOrders[iLane].Active THEN
            // Increment processed bottle count
            aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed :=
                aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed + 1;

            // Add filled bottle data to Filled FIFO
            stFilledOrder := aWIPOrders[iLane].nSys_OrderInfo;
            stFilledOrder.Active := TRUE;

            fbFilledOrdersFifo.A_AddTail(
                pWrite  := ADR(stFilledOrder),
                cbWrite := SIZEOF(stFilledOrder),
                bOk     => bOk3
            );

            // Order complete? release lane
            IF aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed >=
               aWIPOrders[iLane].nSys_OrderInfo.NumberOfBottles THEN
                aWIPOrders[iLane].Active := FALSE;
                aSorterLaneAssigned[iLane] := FALSE;
                bSorterLaneOn[iLane] := FALSE;
                aSorterLaneToteID[iLane] := 0;
            END_IF
        END_IF
    END_FOR

    // Copy to display array (optional)
    fbFilledOrdersFifo.A_RemoveHead(
        pRead  := ADR(stNextFilled),
        cbRead := SIZEOF(stNextFilled),
        bOk    => bOk3
    );

    IF bOk3 THEN
        aFilledOrders[iFilledCount] := stNextFilled;
        aFilledOrders[iFilledCount].Active := TRUE;

        iFilledCount := iFilledCount + 1;
        IF iFilledCount > MaxFilled THEN
            iFilledCount := 1;
        END_IF
    END_IF

    aWIP[1].Active := FALSE;
END_IF


//===========================================================
// 9️⃣ RELEASE LANE WHEN ORDER COMPLETE
//===========================================================
FOR iLane := 1 TO 6 DO
    IF aWIPOrders[iLane].Active THEN
        IF aWIPOrders[iLane].nSys_OrderInfo.NumberOfBottles <=
           aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed THEN
            aWIPOrders[iLane].Active := FALSE;
            aWIPOrders[iLane].LaneAssigned := 0;

            aSorterLaneAssigned[iLane] := FALSE;
            bSorterLaneOn[iLane] := FALSE;
            aSorterLaneToteID[iLane] := 0;
        END_IF
    END_IF
END_FOR


//===========================================================
// 🔟 STATUS FEEDBACK FOR HMI
//===========================================================
TotalTODOCount := fbTODOOrdersFifo.nCount;
TotalActiveWIP := 0;
TotalFreeLanes := 0;

FOR iLane := 1 TO 6 DO
    IF aWIPOrders[iLane].Active THEN
        TotalActiveWIP := TotalActiveWIP + 1;
        LaneStatusText[iLane] := CONCAT(
            CONCAT('Lane ', INT_TO_STRING(iLane)),
            CONCAT(': Active | ToteID ', DINT_TO_STRING(aWIPOrders[iLane].nSys_OrderInfo.ToteID))
        );
    ELSE
        TotalFreeLanes := TotalFreeLanes + 1;
        LaneStatusText[iLane] := CONCAT('Lane ', CONCAT(INT_TO_STRING(iLane), ': Free'));
    END_IF
END_FOR;

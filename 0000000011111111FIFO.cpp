//Initiation Trigger When CycleStart Is Pressed

rInitTrig(CLK:=bCycleStart);

IF rInitTrig.Q THEN
    bInit := TRUE;
    bInit2 := TRUE;
END_IF
            

//======================================================================
// RING BUFFER INIT / RESET
//======================================================================

// Orders FIFO init
IF bInit THEN
    fbTODOOrdersFifo(
        pBuffer := ADR(arrTODOBuffer),
        cbBuffer := SIZEOF(arrTODOBuffer)
    );
    fbTODOOrdersFifo.A_Reset(bOk => bOk);
    bInit := FALSE;
END_IF;

// Order Details & Filled Orders FIFO init
IF bInit2 THEN
    fbTODOOrderDetailsFifo(
        pBuffer := ADR(arrTODODetailsBuffer),
        cbBuffer := SIZEOF(arrTODODetailsBuffer)
    );
    fbTODOOrderDetailsFifo.A_Reset(bOk => bOk2);    // FIX: reset the correct FIFO

    fbFilledOrdersFifo(
        pBuffer := ADR(arrFilledOrdersBuffer),
        cbBuffer := SIZEOF(arrFilledOrdersBuffer)
    );
    fbFilledOrdersFifo.A_Reset(bOk => bOk3);

    bInit2 := FALSE;
END_IF;


//======================================================================
// RING BUFFER CYCLIC CALLS  (REQUIRED FOR FB_MemRingBuffer)
//======================================================================
// These MUST run every PLC cycle, otherwise A_AddTail / A_RemoveHead / A_GetHead
// will not work reliably.

fbTODOOrdersFifo(
    pBuffer := ADR(arrTODOBuffer),
    cbBuffer := SIZEOF(arrTODOBuffer)
);

fbTODOOrderDetailsFifo(
    pBuffer := ADR(arrTODODetailsBuffer),
    cbBuffer := SIZEOF(arrTODODetailsBuffer)
);

fbFilledOrdersFifo(
    pBuffer := ADR(arrFilledOrdersBuffer),
    cbBuffer := SIZEOF(arrFilledOrdersBuffer)
);


//======================================================================
// ADD ORDERS TO TODO ORDERS FIFO
//======================================================================

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
END_IF;


//======================================================================
// MULTI-LANE ORDER ASSIGNMENT FROM TODO FIFO → WIP ORDERS
//======================================================================

IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) OR bCycleStart THEN

   // Peek first (optional)
   fbTODOOrdersFifo.A_GetHead(
       pRead  := ADR(stNextOrder),
       cbRead := SIZEOF(stNextOrder),
       bOk    => bOk
   );

   IF bOk THEN
       // Decide lanes required
       IF stNextOrder.NumberOfBottles <= 5 THEN
           iRequiredLanes := 1;
       ELSIF stNextOrder.NumberOfBottles > 5 AND stNextOrder.NumberOfBottles <= 10 THEN
           iRequiredLanes := 2;
       ELSIF stNextOrder.NumberOfBottles > 10 AND stNextOrder.NumberOfBottles <= 15 THEN
           iRequiredLanes := 3;
       ELSE
           iRequiredLanes := 0;
       END_IF

       // Count free lanes
       iFreeCount := 0;
       FOR iLane := 1 TO 6 DO
           IF NOT aSorterLaneAssigned[iLane] THEN
               iFreeCount := iFreeCount + 1;
           END_IF
       END_FOR

       // Only assign if enough lanes are available
       IF iFreeCount >= iRequiredLanes THEN
           // Now actually remove from FIFO (once)
           fbTODOOrdersFifo.A_RemoveHead(
               pRead  := ADR(stNextOrder),
               cbRead := SIZEOF(stNextOrder),
               bOk    => bOk
           );

           IF bOk THEN
               // Find one empty WIP slot to hold this entire order
               FOR iWIP := 1 TO 6 DO
                   IF NOT aWIPOrders[iWIP].Active THEN
                       aWIPOrders[iWIP].nSys_OrderInfo := stNextOrder;
                       aWIPOrders[iWIP].Active := TRUE;

                       // Assign multiple lanes to this single WIP order
                       laneAssignedCount := 0;
                       FOR iLane := 1 TO 6 DO
                           IF NOT aSorterLaneAssigned[iLane] THEN
                               laneAssignedCount := laneAssignedCount + 1;

                               aWIPOrders[iWIP].LaneAssigned[laneAssignedCount] := iLane;

                               // mark sorter lane as used
                               aSorterLaneAssigned[iLane] := TRUE;
                               aSorterLaneToteID[iLane] := stNextOrder.ToteID;
                               bSorterLaneOn[iLane] := TRUE;

                               IF laneAssignedCount >= iRequiredLanes THEN
                                   EXIT;
                               END_IF
                           END_IF
                       END_FOR

                       EXIT; // stop after one WIP entry
                   END_IF
               END_FOR
           END_IF
       END_IF
   END_IF
END_IF;


//======================================================================
// RELEASE LANE WHEN ORDER COMPLETE
//======================================================================

FOR iLane := 1 TO 6 DO
    IF aWIPOrders[iLane].Active THEN
        IF aWIPOrders[iLane].nSys_OrderInfo.NumberOfBottles <= aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed THEN
            aWIPOrders[iLane].Active := FALSE;
            aWIPOrders[iLane].LaneAssigned[1] := 0;
            aWIPOrders[iLane].LaneAssigned[2] := 0;
            aWIPOrders[iLane].LaneAssigned[3] := 0;
            aWIPOrders[iLane].nSys_OrderInfo.ToteID := 0;
            aWIPOrders[iLane].nSys_OrderInfo.NumberOfBottles := 0;
            aWIPOrders[iLane].nSys_OrderInfo.Route := 0;

            aSorterLaneAssigned[iLane] := FALSE;
            bSorterLaneOn[iLane] := FALSE;
            aSorterLaneToteID[iLane] := 0;
        END_IF
    END_IF
END_FOR;


//======================================================================
// FIFO STATS + PREVIEW
//======================================================================

TotalTODOCount := fbTODOOrdersFifo.nCount;

// Peek at next order (for HMI)
fbTODOOrdersFifo.A_GetHead(
    pRead  := ADR(NextOrderPreview),
    cbRead := SIZEOF(NextOrderPreview),
    bOk    => bOk
);


//======================================================================
// ORDER DETAILS FIFO (BOTTLES)
//======================================================================

// Add bottle records into TODO OrderDetails FIFO
IF nSys_FillBtl_NewDataArrival = 1 THEN
    stNewOrderDetails.nSys_FillBtlData := nSys.FillBtl;
    stNewOrderDetails.Active := FALSE;

    fbTODOOrderDetailsFifo.A_AddTail(
        pWrite  := ADR(stNewOrderDetails),
        cbWrite := SIZEOF(stNewOrderDetails),
        bOk     => bOk2
    );

    nSys_FillBtl_NewDataArrival := 6;
END_IF;


// Take one bottle into aActiveOrder when label-side is ready
IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) OR bCycleStart AND ((NOT(bBottleTransferGripperClosed OR bTransferServoAtLabeler)) OR (bTransferServoAtLabeler AND bBottleTransferGripperopen)) THEN
    IF (NOT aActiveOrder.Active)  THEN
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
END_IF;


//======================================================================
// ACTIVE ORDER → WIP[1] AFTER LABELER
//======================================================================

IF bMoveDataToCabinet AND aActiveOrder.Active AND bLabelerDone AND bEscapementDone AND bTransferServoAtLabeler AND bBottleTransferGripperClosed THEN
    aWIP[1] := aActiveOrder;
    aWIP[1].Active := TRUE;
    iBottleStatusAtCabinet := iBottleStatusAtLabeler;
    aActiveOrder.Active := FALSE;
    bMoveDataToCabinet := FALSE;
END_IF


//======================================================================
// FROM WIP[1] → FILLED ORDERS FIFO & ARRAY AT CAPPER
//======================================================================

IF aWIP[1].Active AND bTransferServoAtCapper THEN
    FOR iLane := 1 TO 6 DO
        IF aWIPOrders[iLane].nSys_OrderInfo.ToteID = aWIP[1].nSys_Kepware_Data.FillBtl.ToteID THEN
            // Increment processed bottle count
            aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed :=
                aWIPOrders[iLane].nSys_OrderInfo.BottlesProcessed + 1;

            // Add filled bottle data to Filled FIFO
            stFilledOrder.nSys_FillBtlData := aWIP[1].nSys_Kepware_Data.FillBtl;
            stFilledOrder.Active := TRUE;

            fbFilledOrdersFifo.A_AddTail(
                pWrite  := ADR(stFilledOrder),
                cbWrite := SIZEOF(stFilledOrder),
                bOk     => bOk3
            );
            aWIP[1].Active := FALSE;
        END_IF
    END_FOR

    // Copy from FIFO to Filled Array
    fbFilledOrdersFifo.A_RemoveHead(
        pRead  := ADR(stNextFilled),
        cbRead := SIZEOF(stNextFilled),
        bOk    => bOk3
    );

    IF bOk3 THEN
        nSys_CappedComp_NewDataArrival := 1;
        nSys.CappedComp.LineID := nSys_CappedComp_LineID;
        nSys_CappedComp_Status := 0;
        nSys_CappedComp_Barcode := stNextFilled.nSys_FillBtlData.Barcode;

        aFilledOrders[iFilledCount].nSys_Kepware_Data.FillBtl := stNextFilled.nSys_FillBtlData;
        aFilledOrders[iFilledCount].Active := TRUE;
        iFilledCount := iFilledCount + 1;
        IF iFilledCount > MaxFilled THEN
            iFilledCount := 1;
        END_IF
    END_IF

    aWIP[1].Active := FALSE;
END_IF


//======================================================================
// HMI STATUS
//======================================================================

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

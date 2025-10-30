//===========================================================
// ORDER FIFO → WIP HANDLER (FINAL VERSION)
// Compatible with TwinCAT 3.1+ FB_MemRingBuffer
//===========================================================

VAR
    // FIFO
    fbTODOOrdersFifo : FB_MemRingBuffer;
    arrTODOBuffer    : ARRAY[0..SIZEOF(ST_Order) * 1000] OF BYTE;
    stNewOrder       : ST_Order;
    stNextOrder      : ST_Order;
    NextOrderPreview : ST_Order;
    bOk              : BOOL;
    bInit            : BOOL := TRUE;

    // WIP
    aWIPOrders : ARRAY[1..6] OF ST_Order;
    iLane      : INT;

    // HMI / status
    TotalTODOCount : UDINT;
    TotalActiveWIP : INT;
    TotalFreeLanes : INT;
    LaneStatusText : ARRAY[1..6] OF STRING(40);
END_VAR;


//===========================================================
// 1️⃣ INITIALIZATION
//===========================================================
IF bInit THEN
    fbTODOOrdersFifo(
        pBuffer := ADR(arrTODOBuffer),
        cbBuffer := SIZEOF(arrTODOBuffer)
    );
    fbTODOOrdersFifo.A_Reset(bOk => bOk);
    bInit := FALSE;
END_IF;


//===========================================================
// 2️⃣ ADD NEW ORDER FROM PPS → FIFO
//===========================================================
IF nSys_ToteInfo_NewDataArrival = 1 THEN
    stNewOrder.ToteID          := nSys.ToteInfo.ToteID;
    stNewOrder.NumberOfBottles := nSys.ToteInfo.NumBottles;
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


//===========================================================
// 3️⃣ MOVE NEXT ORDER FROM FIFO → WIP (AUTO MODE)
//===========================================================
IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) THEN
    FOR iLane := 1 TO 6 DO
        IF NOT aWIPOrders[iLane].Active THEN
            fbTODOOrdersFifo.A_RemoveHead(
                pRead  := ADR(stNextOrder),
                cbRead := SIZEOF(stNextOrder),
                bOk    => bOk
            );

            IF bOk THEN
                aWIPOrders[iLane] := stNextOrder;
                aWIPOrders[iLane].Active := TRUE;
                aWIPOrders[iLane].LaneAssigned := iLane;

                aSorterLaneToteID[iLane] := aWIPOrders[iLane].ToteID;
                aSorterLaneAssigned[iLane] := TRUE;
                bSorterLaneOn[iLane] := TRUE;
            END_IF
        END_IF
    END_FOR
END_IF;


//===========================================================
// 4️⃣ RELEASE LANE WHEN ORDER COMPLETE
//===========================================================
FOR iLane := 1 TO 6 DO
    IF aWIPOrders[iLane].Active THEN
        IF aWIPOrders[iLane].NumberOfBottles <= aWIPOrders[iLane].BottlesProcessed THEN
            aWIPOrders[iLane].Active := FALSE;
            aWIPOrders[iLane].LaneAssigned := 0;

            aSorterLaneAssigned[iLane] := FALSE;
            bSorterLaneOn[iLane] := FALSE;
            aSorterLaneToteID[iLane] := 0;
        END_IF
    END_IF
END_FOR;


//===========================================================
// 5️⃣ STATUS / HMI FEEDBACK
//===========================================================

// FIFO stats
TotalTODOCount := fbTODOOrdersFifo.nLoad;

// Peek at next order (no remove)
fbTODOOrdersFifo.A_GetHead(
    pRead  := ADR(NextOrderPreview),
    cbRead := SIZEOF(NextOrderPreview),
    bOk    => bOk
);

// WIP overview
TotalActiveWIP := 0;
TotalFreeLanes := 0;

FOR iLane := 1 TO 6 DO
    IF aWIPOrders[iLane].Active THEN
        TotalActiveWIP := TotalActiveWIP + 1;
        LaneStatusText[iLane] := CONCAT(
            CONCAT('Lane ', INT_TO_STRING(iLane)),
            CONCAT(': Active | ToteID ', DINT_TO_STRING(aWIPOrders[iLane].ToteID))
        );
    ELSE
        TotalFreeLanes := TotalFreeLanes + 1;
        LaneStatusText[iLane] := CONCAT('Lane ', CONCAT(INT_TO_STRING(iLane), ': Free'));
    END_IF
END_FOR;

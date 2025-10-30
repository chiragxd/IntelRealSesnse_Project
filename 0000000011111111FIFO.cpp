Perfect ✅ — below is a ready-to-drop TwinCAT Structured Text implementation using FB_MemRingBuffer to handle order movement cleanly from aTODOOrders (your FIFO queue) to aWIPOrders (your active lanes).

It’s written in your naming style — same TwinCAT convention and logical flow you already use.

🧩 Step 1 — Define Your Order Structure (global)

You already likely have this, but make sure your order structure is defined in a global .TcDUT file:

TYPE ST_Order :
STRUCT
    ToteID          : DINT;
    NumberOfBottles : INT;
    Route           : INT;
    LaneAssigned    : INT;
    Active          : BOOL;
END_STRUCT
END_TYPE

🧠 Step 2 — Declare FIFO and WIP in Your Global Variables

This sets up the memory ring buffer for the incoming orders and defines the active WIP lanes.

VAR_GLOBAL
    // FIFO Buffer for TODO Orders
    fbTODOOrdersFifo : FB_MemRingBuffer;
    arrTODOBuffer    : ARRAY[0..SIZEOF(ST_Order) * 1000] OF BYTE;

    // Active WIP Orders (one per sorter lane)
    aWIPOrders       : ARRAY[1..6] OF ST_Order;

    // Temporary structures
    stNewOrder       : ST_Order;
    stNextOrder      : ST_Order;

    bOk              : BOOL;
END_VAR

⚙️ Step 3 — Initialize FIFO (in your PRG_INIT or startup routine)

Run this once after startup or on reset to prepare the FIFO memory.

fbTODOOrdersFifo(
    pBuffer := ADR(arrTODOBuffer),
    cbBuffer := SIZEOF(arrTODOBuffer)
);

📦 Step 4 — Add New Orders from PPS (in your main program)

This goes wherever you handle new order arrivals from PPS.

// When PPS sends a new ToteInfo
IF nSys_ToteInfo_NewDataArrival = 1 THEN
    stNewOrder.ToteID := nSys.ToteInfo.ToteID;
    stNewOrder.NumberOfBottles := nSys.ToteInfo.NumBottles;
    stNewOrder.Route := nSys.ToteInfo.Route;
    stNewOrder.LaneAssigned := 0;
    stNewOrder.Active := FALSE;

    // Add to FIFO
    fbTODOOrdersFifo.A_Add(in := stNewOrder, bOk => bOk);

    // Reset PPS flag
    nSys_ToteInfo_NewDataArrival := 6;
END_IF

🚀 Step 5 — Move Orders from FIFO → WIP (Auto Mode)

This will continuously check for empty WIP lanes and automatically pull the next order from the FIFO.

IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) THEN
    // Loop through each WIP slot
    FOR iLane := 1 TO 6 DO
        // If lane is free (no active order)
        IF (NOT aWIPOrders[iLane].Active) THEN
            // Try to get the next order from FIFO
            fbTODOOrdersFifo.A_Remove(out := stNextOrder, bOk => bOk);

            IF bOk THEN
                // Assign lane to the new order
                aWIPOrders[iLane] := stNextOrder;
                aWIPOrders[iLane].Active := TRUE;
                aWIPOrders[iLane].LaneAssigned := iLane;

                // (Optional) Drive your sorter variables
                aSorterLaneToteID[iLane] := aWIPOrders[iLane].ToteID;
                aSorterLaneAssigned[iLane] := TRUE;
                bSorterLaneOn[iLane] := TRUE;
            END_IF
        END_IF
    END_FOR
END_IF

🧹 Step 6 — Free Lanes When Order Is Complete

When an order finishes processing in that lane (based on bottles processed or completion condition), clear it safely:

FOR iLane := 1 TO 6 DO
    IF aWIPOrders[iLane].Active THEN
        // Example: order complete when bottles done
        IF aWIPOrders[iLane].NumberOfBottles <= aWIPOrders[iLane].BottlesProcessed THEN
            aWIPOrders[iLane].Active := FALSE;
            aWIPOrders[iLane].LaneAssigned := 0;

            aSorterLaneAssigned[iLane] := FALSE;
            bSorterLaneOn[iLane] := FALSE;
            aSorterLaneToteID[iLane] := 0;
        END_IF
    END_IF
END_FOR

💡 Optional: Reset the FIFO

If you ever need to clear the TODO list (e.g., at shift end):

fbTODOOrdersFifo.A_Reset(in := stNewOrder, bOk => bOk);

✅ What You Get
Behavior	Description
Automatic FIFO queue	New PPS orders are added sequentially.
Automatic lane fill	As soon as a lane is free, next FIFO order is pulled.
Order persistence	Each lane keeps its own aWIPOrders[i] structure — no data loss.
Easy expansion	You can extend to 8 or 10 lanes by changing one number.
Safe and clean	No manual index shifting; uses proven TwinCAT library FB.
🧠 Pro Tip — Debug View

When you go online in TwinCAT:

fbTODOOrdersFifo.nLoad → number of items currently in FIFO

aWIPOrders[i].Active → TRUE = that lane is currently running an order

bSorterLaneOn[i] → directly usable for your HMI lane indicators

🧩 1. Data Type Definition
TYPE ST_Order :
STRUCT
    OrderID     : DINT;
    CustomerID  : DINT;
    Priority    : INT;
    Quantity    : DINT;
    Valid       : BOOL;
END_STRUCT
END_TYPE
🗂 2. Global Variables (GVL_FIFO)
VAR_GLOBAL
    nSys_ToteInfo_NewDataArrival : BOOL;   // PPS new order signal
    PPSOrder : ST_Order;                   // Incoming order from PPS

    MaxTODO : INT := 10;
    MaxWIP  : INT := 4;

    aTODO : ARRAY [0..9] OF ST_Order;      // All waiting orders
    aWIP  : ARRAY [0..3] OF ST_Order;      // Active WIP orders

    ActiveWIP_OrderID : DINT := 0;
    ActiveWIP_Index   : INT := -1;

    DummyCompletedID : DINT := 0;          // For testing completion
END_VAR
⚙️ 3. Add to TODO (FB_AddToTODO)
FUNCTION_BLOCK FB_AddToTODO
VAR_INPUT
    NewOrder : ST_Order;
END_VAR
VAR
    i : INT;
END_VAR

FOR i := 0 TO MaxTODO - 1 DO
    IF NOT aTODO[i].Valid THEN
        aTODO[i] := NewOrder;
        aTODO[i].Valid := TRUE;
        EXIT;
    END_IF
END_FOR
⚙️ 4. Move from TODO → WIP (FB_MoveToWIP)
FUNCTION_BLOCK FB_MoveToWIP
VAR
    i, j : INT;
END_VAR

FOR i := 0 TO MaxWIP - 1 DO
    IF NOT aWIP[i].Valid THEN
        FOR j := 0 TO MaxTODO - 1 DO
            IF aTODO[j].Valid THEN
                aWIP[i] := aTODO[j];
                aWIP[i].Valid := TRUE;
                aTODO[j].Valid := FALSE;
                EXIT;
            END_IF
        END_FOR
    END_IF
END_FOR
⚙️ 5. Complete Order + Shift FIFO (FB_CompleteOrder)
FUNCTION_BLOCK FB_CompleteOrder
VAR_INPUT
    CompletedID : DINT; // ID of completed order
END_VAR
VAR
    i, j : INT;
    FoundIndex : INT := -1;
END_VAR

// Find completed order
FOR i := 0 TO MaxWIP - 1 DO
    IF aWIP[i].Valid AND (aWIP[i].OrderID = CompletedID) THEN
        aWIP[i].Valid := FALSE;
        FoundIndex := i;
        EXIT;
    END_IF
END_FOR

// Shift all remaining orders up (FIFO)
IF FoundIndex >= 0 THEN
    FOR j := FoundIndex TO MaxWIP - 2 DO
        aWIP[j] := aWIP[j + 1];
    END_FOR
    aWIP[MaxWIP - 1].Valid := FALSE;
END_IF
⚙️ 6. Helper — Get Active WIP (Optional)
FUNCTION_BLOCK FB_GetActiveWIP
VAR_OUTPUT
    ActiveCount : INT;
    ActiveOrders : ARRAY [0..MaxWIP-1] OF ST_Order;
END_VAR
VAR
    i, index : INT;
END_VAR

index := 0;
ActiveCount := 0;

FOR i := 0 TO MaxWIP - 1 DO
    IF aWIP[i].Valid THEN
        ActiveOrders[index] := aWIP[i];
        index := index + 1;
        ActiveCount := ActiveCount + 1;
    END_IF
END_FOR
🧠 7. Main Program (PRG_FIFO_Manager)
PROGRAM PRG_FIFO_Manager
VAR
    fbAddToTODO     : FB_AddToTODO;
    fbMoveToWIP     : FB_MoveToWIP;
    fbCompleteOrder : FB_CompleteOrder;
    fbGetActiveWIP  : FB_GetActiveWIP;
    i : INT;
END_VAR

// Step 1: Detect new PPS order
IF nSys_ToteInfo_NewDataArrival THEN
    fbAddToTODO(NewOrder := PPSOrder);
    nSys_ToteInfo_NewDataArrival := FALSE;
END_IF

// Step 2: Move TODO → WIP if space available
fbMoveToWIP();

// Step 3: Complete an order if ID provided
IF DummyCompletedID <> 0 THEN
    fbCompleteOrder(CompletedID := DummyCompletedID);
    fbMoveToWIP(); // refill empty WIP slot
    DummyCompletedID := 0;
END_IF

// Step 4: Update active WIP order (first valid one)
ActiveWIP_OrderID := 0;
ActiveWIP_Index := -1;

FOR i := 0 TO MaxWIP - 1 DO
    IF aWIP[i].Valid THEN
        ActiveWIP_OrderID := aWIP[i].OrderID;
        ActiveWIP_Index := i;
        EXIT;
    END_IF
END_FOR
🧾 8. Example Runtime Behavior
PPS adds orders:
Step	Action	aTODO	aWIP
1	NewDataArrival = TRUE	Order #101	—
2	MoveToWIP	—	101
3	NewDataArrival = TRUE	Order #102	101
4	CompleteOrder(101)	102	(101 removed, 102 shifts up)
✅ FIFO maintained
✅ Always fills empty WIP slot from TODO
✅ Easy to visualize on HMI
🧰 Optional Visualization Tags
You can display or monitor these on your HMI:
aTODO[i].OrderID, aTODO[i].Valid
aWIP[i].OrderID, aWIP[i].Valid
ActiveWIP_OrderID
ActiveWIP_Index
Would you like me to make one compact version (single FB_FIFO_Manager) that wraps all of these blocks into one function block (handles add, move, complete, shift automatically with methods like .Add(), .Complete(), .GetActive())?
That would make your logic even cleaner and production-ready.
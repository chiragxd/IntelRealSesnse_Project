🧩 Step 1: Define the Data Type (ST_Order)

Create this under DUTs → ST_Order.TcDUT or inside a global declaration.

TYPE ST_Order :
STRUCT
    OrderID     : DINT;      // Unique ID of the order
    CustomerID  : DINT;      // Example field from PPS
    Priority    : INT;       // Can be used for scheduling (optional)
    Quantity    : DINT;      // Number of items
    Valid       : BOOL;      // TRUE when this order is valid
END_STRUCT
END_TYPE

🗂 Step 2: Declare Global Variables (GVL_FIFO)

Create a Global Variable List (GVL_FIFO.TcGVL)

VAR_GLOBAL
    nSys_ToteInfo_NewDataArrival : BOOL;   // Signal from PPS/Kepware
    PPSOrder : ST_Order;                   // Current incoming PPS order

    MaxTODO : INT := 10;                   // Maximum number of TODO orders
    MaxWIP  : INT := 4;                    // Maximum number of WIP slots

    aTODO : ARRAY [0..9] OF ST_Order;      // All orders waiting to be processed
    aWIP  : ARRAY [0..3] OF ST_Order;      // Orders currently in progress

    bMoveOrders : BOOL := TRUE;            // Control flag to enable TODO→WIP transfer
END_VAR

⚙️ Step 3: Function Block — FB_AddToTODO

This block will take a new PPS order and push it into the first empty slot in aTODO.

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

⚙️ Step 4: Function Block — FB_MoveToWIP

Moves orders FIFO-style from TODO → WIP when slots are free.

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

⚙️ Step 5: Function Block — FB_CompleteOrder

Marks a WIP order as done and clears its slot.

FUNCTION_BLOCK FB_CompleteOrder
VAR_INPUT
    CompletedID : DINT; // The order ID that is now completed
END_VAR
VAR
    i : INT;
END_VAR

FOR i := 0 TO MaxWIP - 1 DO
    IF aWIP[i].Valid AND (aWIP[i].OrderID = CompletedID) THEN
        aWIP[i].Valid := FALSE; // remove from WIP
        EXIT;
    END_IF
END_FOR

🧠 Step 6: Main Program Logic (PRG_FIFO_Manager)

This program glues it all together.

PROGRAM PRG_FIFO_Manager
VAR
    fbAddToTODO     : FB_AddToTODO;
    fbMoveToWIP     : FB_MoveToWIP;
    fbCompleteOrder : FB_CompleteOrder;

    DummyCompletedID : DINT; // for testing
END_VAR

// Step 1: Detect new order from PPS
IF nSys_ToteInfo_NewDataArrival THEN
    fbAddToTODO(NewOrder := PPSOrder);
    nSys_ToteInfo_NewDataArrival := FALSE; // Reset trigger
END_IF

// Step 2: Move orders from TODO → WIP if possible
IF bMoveOrders THEN
    fbMoveToWIP();
END_IF

// Step 3: Complete an order (simulate by ID)
IF DummyCompletedID <> 0 THEN
    fbCompleteOrder(CompletedID := DummyCompletedID);
    DummyCompletedID := 0; // reset
END_IF

🧾 Step 7: Example Behavior Summary
Stage	Action	Effect
PPS sends NewDataArrival = 1	Adds order to aTODO	Stored in first empty slot
bMoveOrders = TRUE	Moves order to WIP	Transfers FIFO-style
When DummyCompletedID is set	Marks order complete	Clears WIP slot
Empty WIP slots auto-filled	From TODO list	FIFO maintained
🧪 Step 8: (Optional) FIFO Visualization

You can add a visualization table in TwinCAT HMI or Codesys Visu:

Show all aTODO[i].OrderID and Valid status.

Show all aWIP[i].OrderID and Valid status.

Add buttons to simulate:

New Order

Move Orders

Complete Order

Would you like me to add automatic refill logic, so that as soon as an order is completed in WIP, the next TODO order automatically fills it (no need to call FB_MoveToWIP separately)?
That would make it self-managed FIFO behavior — ideal for runtime automation.

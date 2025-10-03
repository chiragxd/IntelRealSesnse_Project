TYPE ST_Order :
STRUCT
    OrderNumber : DWORD;     // 4 bytes
    Barcode     : STRING[6]; // 6 chars
    Size        : WORD;      // 2 bytes
    Quantity    : DWORD;     // 4 bytes
    Network     : BYTE;      // 1 byte (cabinet number)
    CanisterID  : BYTE;      // 1 byte (1..4)
    Valid       : BOOL;
END_STRUCT
END_TYPE


VAR_GLOBAL
    arrTODO : ARRAY[1..5000] OF ST_Order;
    TODO_Count : INT := 0;

    arrWIP  : ARRAY[1..6] OF ST_Order;
    WIP_Count : INT := 0;

    stActiveOrder : ST_Order;
    iWIPActiveIx  : INT := 0;

    // PPS inputs (tags)
    PPS_NewOrder    : BOOL;
    PPS_OrderNumber : DWORD;
    PPS_Barcode     : STRING[6];
    PPS_Size        : WORD;
    PPS_Quantity    : DWORD;
    PPS_Network     : BYTE;
    PPS_CanisterID  : BYTE;

    // Machine signals
    bBottleSorted   : BOOL;
    bSmallBottleSelected : BOOL;
    bLargeBottleSelected : BOOL;
END_VAR

IF PPS_NewOrder AND (TODO_Count < 5000) THEN
    TODO_Count := TODO_Count + 1;

    arrTODO[TODO_Count].OrderNumber := PPS_OrderNumber;
    arrTODO[TODO_Count].Barcode     := PPS_Barcode;
    arrTODO[TODO_Count].Size        := PPS_Size;
    arrTODO[TODO_Count].Quantity    := PPS_Quantity;
    arrTODO[TODO_Count].Network     := PPS_Network;
    arrTODO[TODO_Count].CanisterID  := PPS_CanisterID;
    arrTODO[TODO_Count].Valid       := TRUE;

    PPS_NewOrder := FALSE; // reset handshake
END_IF;

IF (WIP_Count < 6) AND (TODO_Count > 0) THEN
    WIP_Count := WIP_Count + 1;

    arrWIP[WIP_Count] := arrTODO[1];  // oldest order
    arrWIP[WIP_Count].Valid := TRUE;

    // Shift TODO list up
    FOR i := 1 TO TODO_Count - 1 DO
        arrTODO[i] := arrTODO[i+1];
    END_FOR;

    arrTODO[TODO_Count].Valid := FALSE;
    TODO_Count := TODO_Count - 1;
END_IF;

IF (iWIPActiveIx = 0) AND (WIP_Count > 0) THEN
    iWIPActiveIx := 1; // Always first in WIP
    stActiveOrder := arrWIP[iWIPActiveIx];
END_IF;


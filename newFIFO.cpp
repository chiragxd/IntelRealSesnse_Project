TYPE ST_Order :
STRUCT
    OrderNumber : DWORD;
    Barcode     : STRING[6];
    Size        : WORD;
    Quantity    : DWORD;
    Network     : BYTE;
    CanisterID  : BYTE;
    Valid       : BOOL;
END_STRUCT
END_TYPE


VAR_GLOBAL
    arrTODO : ARRAY[1..5000] OF ST_Order;
    iTODO_Head : UINT := 1;
    iTODO_Tail : UINT := 1;
    iTODO_Count : UINT := 0;

    arrWIP : ARRAY[1..6] OF ST_Order;
    iWIP_Count : UINT := 0;
    iWIPActiveIx : UINT := 0;
    stActiveOrder : ST_Order;

    bBottleSorted : BOOL; // set TRUE when one order is completed

    // Inputs from PPS (separate tags)
    PPS_OrderNumber : DWORD;
    PPS_Barcode     : STRING[6];
    PPS_Size        : WORD;
    PPS_Quantity    : DWORD;
    PPS_Network     : BYTE;
    PPS_CanisterID  : BYTE;
    PPS_NewOrder    : BOOL; // PPS sets this TRUE when new order arrives
END_VAR

IF PPS_NewOrder THEN
    arrTODO[iTODO_Head].OrderNumber := PPS_OrderNumber;
    arrTODO[iTODO_Head].Barcode     := PPS_Barcode;
    arrTODO[iTODO_Head].Size        := PPS_Size;
    arrTODO[iTODO_Head].Quantity    := PPS_Quantity;
    arrTODO[iTODO_Head].Network     := PPS_Network;
    arrTODO[iTODO_Head].CanisterID  := PPS_CanisterID;
    arrTODO[iTODO_Head].Valid       := TRUE;

    iTODO_Head := iTODO_Head + 1;
    IF iTODO_Head > 5000 THEN iTODO_Head := 1; END_IF;

    IF iTODO_Count < 5000 THEN
        iTODO_Count := iTODO_Count + 1;
    ELSE
        // buffer full → overwrite oldest
        iTODO_Tail := iTODO_Tail + 1;
        IF iTODO_Tail > 5000 THEN iTODO_Tail := 1; END_IF;
    END_IF;

    PPS_NewOrder := FALSE; // reset handshake
END_IF;

IF (iWIP_Count < 6) AND (iTODO_Count > 0) THEN
    VAR i : INT; END_VAR
    FOR i := 1 TO 6 DO
        IF NOT arrWIP[i].Valid THEN
            arrWIP[i] := arrTODO[iTODO_Tail];
            arrWIP[i].Valid := TRUE;

            arrTODO[iTODO_Tail].Valid := FALSE;
            iTODO_Tail := iTODO_Tail + 1;
            IF iTODO_Tail > 5000 THEN iTODO_Tail := 1; END_IF;
            iTODO_Count := iTODO_Count - 1;

            iWIP_Count := iWIP_Count + 1;
            EXIT;
        END_IF;
    END_FOR;
END_IF;


IF (iWIPActiveIx = 0) AND (iWIP_Count > 0) THEN
    VAR j : INT; END_VAR
    FOR j := 1 TO 6 DO
        IF arrWIP[j].Valid THEN
            iWIPActiveIx := j;
            stActiveOrder := arrWIP[j];
            EXIT;
        END_IF;
    END_FOR;
END_IF;


IF bBottleSorted AND (iWIPActiveIx <> 0) THEN
    arrWIP[iWIPActiveIx].Valid := FALSE;
    iWIP_Count := iWIP_Count - 1;

    iWIPActiveIx := 0;
    stActiveOrder.Valid := FALSE;

    bBottleSorted := FALSE; // reset handshake
END_IF;




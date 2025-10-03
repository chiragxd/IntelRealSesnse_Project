
//1) Define the order type (UDT)

TYPE ST_Order :
STRUCT
    OrderNumber : DWORD;      // 4 bytes
    Barcode     : STRING[6];  // 6 chars
    Size        : WORD;       // 2 bytes (your meaning)
    Quantity    : DWORD;      // 4 bytes
    Network     : BYTE;       // 1 byte (cabinet)
    CanisterID  : BYTE;       // 1 byte (1..4)
    RawData     : STRING(255);// full raw frame (optional)
    Valid       : BOOL;       // slot in use
END_STRUCT
END_TYPE


//2) Global arrays + indices (ring buffer)
VAR_GLOBAL
    // Big TODO ring buffer (FIFO)
    arrTODO      : ARRAY[1..5000] OF ST_Order;
    iTODO_Head   : UINT := 1;  // write position (next free)
    iTODO_Tail   : UINT := 1;  // read position (oldest)
    iTODO_Count  : UINT := 0;  // items in TODO

    // Small WIP pool (up to 6 in process)
    arrWIP       : ARRAY[1..6] OF ST_Order;
    iWIP_Count   : UINT := 0;  // how many valid entries
    iWIPActiveIx : UINT := 0;  // 0=none, else 1..6

    // Current active order (for other logic to read)
    stActiveOrder : ST_Order;

    // External handshake (your sorter/escapement logic should drive this TRUE when an order finished)
    bBottleSorted : BOOL;

    // PPS framing mode (choose TRUE for binary frames, FALSE for ASCII fixed width)
    bPPS_IsBinary : BOOL := TRUE;
END_VAR

//3) Decoders (Binary and ASCII)
//We only read the first 18 bytes/chars; extra data is ignored (but kept in RawData for logs).
FUNCTION DecodePPSOrder_Binary : ST_Order
VAR_INPUT
    sRaw : STRING(255);
END_VAR
VAR
    stOut  : ST_Order;
    byArr  : ARRAY[1..255] OF BYTE;
    i, L   : INT;
END_VAR

L := LEN(sRaw);
FOR i := 1 TO L DO
    byArr[i] := BYTE(sRaw[i]);
END_FOR

// DWORD (big-endian)
stOut.OrderNumber :=
    SHL(DWORD(byArr[1]), 24) OR
    SHL(DWORD(byArr[2]), 16) OR
    SHL(DWORD(byArr[3]), 8)  OR
    DWORD(byArr[4]);

// 6-char barcode
stOut.Barcode := '';
FOR i := 0 TO 5 DO
    stOut.Barcode := CONCAT(stOut.Barcode, CHR(byArr[5+i]));
END_FOR

// WORD size
stOut.Size := WORD(SHL(WORD(byArr[11]), 8) OR WORD(byArr[12]));

// DWORD qty
stOut.Quantity :=
    SHL(DWORD(byArr[13]), 24) OR
    SHL(DWORD(byArr[14]), 16) OR
    SHL(DWORD(byArr[15]), 8)  OR
    DWORD(byArr[16]);

stOut.Network    := byArr[17];
stOut.CanisterID := byArr[18];

stOut.RawData := sRaw;
stOut.Valid   := TRUE;
DecodePPSOrder_Binary := stOut;

//AASCII
FUNCTION DecodePPSOrder_ASCII : ST_Order
VAR_INPUT
    sRaw : STRING(255);
END_VAR
VAR
    stOut : ST_Order;
    tmp   : STRING(16);
END_VAR

tmp := MID(sRaw, 1, 4);  stOut.OrderNumber := STRING_TO_DWORD(tmp);
stOut.Barcode := MID(sRaw, 5, 6);
tmp := MID(sRaw,11, 2);  stOut.Size       := STRING_TO_WORD(tmp);
tmp := MID(sRaw,13, 4);  stOut.Quantity   := STRING_TO_DWORD(tmp);
tmp := MID(sRaw,17, 1);  stOut.Network    := STRING_TO_BYTE(tmp);
tmp := MID(sRaw,18, 1);  stOut.CanisterID := STRING_TO_BYTE(tmp);

stOut.RawData := sRaw;
stOut.Valid   := TRUE;
DecodePPSOrder_ASCII := stOut;

//4) Order manager program (always running)
//Add this PRG_PPS_OrderManager to a cyclic task (e.g., 10–20 ms).
//Replace the serial read section with your serial FB outputs.

PROGRAM PRG_PPS_OrderManager
VAR
    // --- Serial input plumbing ---
    sRxBuf        : STRING(1024); // accumulates bytes
    sFrame        : STRING(255);  // one complete frame
    bFrameReady   : BOOL;

    // Simulated inputs from serial FB (replace with your FB outputs)
    sSerialChunk  : STRING(255);  // new bytes this cycle
    bNewBytes     : BOOL;         // TRUE when sSerialChunk has data

    // Aux
    i, emptyIx : INT;
    rtSorted   : R_TRIG;
END_VAR

// ===== 1) SERIAL RECEIVE → FRAME ASSEMBLY =====
// Replace this block with your actual serial FB:
//   - Append any newly received bytes into sRxBuf each cycle.
//   - Decide framing:
//       Binary: when LEN(sRxBuf) >= 18 → cut first 18 as a frame.
//       ASCII:  wait for CRLF (or use fixed 18 chars if no delimiter).

IF bNewBytes THEN
    sRxBuf := CONCAT(sRxBuf, sSerialChunk);
END_IF

IF bPPS_IsBinary THEN
    IF LEN(sRxBuf) >= 18 THEN
        // take first 18 bytes as one frame
        sFrame := LEFT(sRxBuf, 18);
        sRxBuf := RIGHT(sRxBuf, LEN(sRxBuf)-18);
        bFrameReady := TRUE;
    END_IF
ELSE
    // ASCII: look for CRLF, else fall back to first 18 chars if long enough
    i := FIND(sRxBuf, '\r\n'); // if you store CRLF; otherwise adapt to your delimiter
    IF i > 0 THEN
        sFrame := LEFT(sRxBuf, i-1);
        sRxBuf := RIGHT(sRxBuf, LEN(sRxBuf)-(i+1));
        bFrameReady := TRUE;
    ELSIF LEN(sRxBuf) >= 18 THEN
        sFrame := LEFT(sRxBuf, 18);
        sRxBuf := RIGHT(sRxBuf, LEN(sRxBuf)-18);
        bFrameReady := TRUE;
    END_IF
END_IF


// ===== 2) DECODE + ENQUEUE TO TODO =====
IF bFrameReady THEN
    VAR
        stNew : ST_Order;
    END_VAR

    IF bPPS_IsBinary THEN
        stNew := DecodePPSOrder_Binary(sFrame);
    ELSE
        stNew := DecodePPSOrder_ASCII(sFrame);
    END_IF

    // Enqueue to TODO ring (overwrite oldest if full)
    IF iTODO_Count < 5000 THEN
        arrTODO[iTODO_Head] := stNew; arrTODO[iTODO_Head].Valid := TRUE;
        iTODO_Head := iTODO_Head + 1; IF iTODO_Head > 5000 THEN iTODO_Head := 1; END_IF;
        iTODO_Count := iTODO_Count + 1;
    ELSE
        // Buffer full → overwrite oldest (advance tail)
        arrTODO[iTODO_Head] := stNew; arrTODO[iTODO_Head].Valid := TRUE;
        iTODO_Head := iTODO_Head + 1; IF iTODO_Head > 5000 THEN iTODO_Head := 1; END_IF;
        iTODO_Tail := iTODO_Tail + 1; IF iTODO_Tail > 5000 THEN iTODO_Tail := 1; END_IF;
        // iTODO_Count stays at 5000
    END_IF

    bFrameReady := FALSE;
END_IF


// ===== 3) KEEP WIP FILLED FROM TODO =====
IF (iWIP_Count < 6) AND (iTODO_Count > 0) THEN
    // find first empty slot in WIP
    emptyIx := 0;
    FOR i := 1 TO 6 DO
        IF NOT arrWIP[i].Valid THEN emptyIx := i; EXIT; END_IF;
    END_FOR

    IF emptyIx <> 0 THEN
        arrWIP[emptyIx] := arrTODO[iTODO_Tail];
        arrWIP[emptyIx].Valid := TRUE;

        // advance TODO tail
        arrTODO[iTODO_Tail].Valid := FALSE;
        iTODO_Tail := iTODO_Tail + 1; IF iTODO_Tail > 5000 THEN iTODO_Tail := 1; END_IF;
        iTODO_Count := iTODO_Count - 1;

        iWIP_Count := iWIP_Count + 1;
    END_IF
END_IF


// ===== 4) PICK AN ACTIVE WIP ORDER (if none) =====
IF (iWIPActiveIx = 0) AND (iWIP_Count > 0) THEN
    // choose lowest-index valid slot
    FOR i := 1 TO 6 DO
        IF arrWIP[i].Valid THEN
            iWIPActiveIx := i;
            stActiveOrder := arrWIP[i];
            EXIT;
        END_IF;
    END_FOR
END_IF

// (Optional) map to your existing bottle selection here:
// Example only — adjust per your rules:
//  - Small vs Large decision could depend on Size or CanisterID.
(*
bSmallBottleSelected := (iWIPActiveIx <> 0) AND (stActiveOrder.CanisterID IN [1,2]);
bLargeBottleSelected := (iWIPActiveIx <> 0) AND (stActiveOrder.CanisterID IN [3,4]);
*)


// ===== 5) WHEN ORDER COMPLETE → CLEAR FROM WIP =====
rtSorted(CLK := bBottleSorted);
IF rtSorted.Q AND (iWIPActiveIx <> 0) THEN
    arrWIP[iWIPActiveIx].Valid := FALSE;
    iWIP_Count := iWIP_Count - 1;
    iWIPActiveIx := 0;               // release active slot
    stActiveOrder.Valid := FALSE;    // no active order
END_IF





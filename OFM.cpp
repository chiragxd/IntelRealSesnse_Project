(*
    PRG_OFM — OptiFill Manager (TwinCAT ST)
    Implements the core of _04_OFM ladder using your named tags.

    Handshake convention (mirrors ladder):
      - On receive paths, we treat NewDataArrival <> 1 as "new/needs processing" and ACK by setting it to 6 (per pdf rungs).
      - You can change the exact compare if your upstream publishes a specific "1=new, 6=idle" scheme.

    Key capacities (mirror ladder intent):
      - Active Patient Orders (max 6)
      - Manual Order FIFOs (ToteID / Route) length 100
      - Bottle Order FIFO length 5 (one slot per cabinet bank in your ladder)
*)

PROGRAM PRG_OFM
VAR
    (* ===== Kepware/System Tags (declare as VAR_EXTERNAL if you already define them elsewhere) ===== *)
    nSys_NewDataArrival         : INT;    (* N27[000] *)   (* not used directly here *) //:contentReference[oaicite:7]{index=7}
    nSys_Status                 : INT;    (* N27[001] *)   //:contentReference[oaicite:8]{index=8}

    (* Patient Order / Tote Info *)
    nSys_ToteInfo_NewDataArrival : INT;   (* N27[010] *)   //:contentReference[oaicite:9]{index=9}
    nSys_ToteInfo_ToteID         : INT;   (* N27[011] *)   //:contentReference[oaicite:10]{index=10}
    nSys_ToteInfo_NumberOfBottles: INT;   (* N27[012] *)   //:contentReference[oaicite:11]{index=11}
    nSys_ToteInfo_Route          : INT;   (* N27[013] *)   //【13†file*/

    (* Bottle Order / FillBtl *)
    nSys_FillBtl_NewDataArrival  : INT;   (* N27[020] *)   //:contentReference[oaicite:12]{index=12}
    nSys_FillBtl_Cabinet         : INT;   (* N27[021] *)   //:contentReference[oaicite:13]{index=13}
    nSys_FillBtl_Can1            : INT;   (* N27[022] *)   //:contentReference[oaicite:14]{index=14}
    nSys_FillBtl_Can2            : INT;   (* N27[023] *)   //:contentReference[oaicite:15]{index=15}
    nSys_FillBtl_Can3            : INT;   (* N27[024] *)   //:contentReference[oaicite:16]{index=16}
    nSys_FillBtl_Can4            : INT;   (* N27[025] *)   //:contentReference[oaicite:17]{index=17}
    nSys_FillBtl_BottleSize      : INT;   (* N27[026] *)   //:contentReference[oaicite:18]{index=18}
    nSys_FillBtl_Quantity        : INT;   (* N27[027] *)   //:contentReference[oaicite:19]{index=19}
    nSys_FillBtl_ToteID          : INT;   (* N27[028] *)   //【13†file*/
    nSys_FillBtl_Barcode1        : INT;   (* N27[029] *)   //【13†file*/
    nSys_FillBtl_Barcode2        : INT;   (* N27[030] *)   //【13†file*/
    nSys_FillBtl_Barcode3        : INT;   (* N27[031] *)   //【13†file*/
    nSys_FillBtl_Can1Hi          : INT;   (* N27[034] *)   //【13†file*/
    nSys_FillBtl_Can1Lo          : INT;   (* N27[035] *)   //【13†file*/
    nSys_FillBtl_Can2Hi          : INT;   (* N27[036] *)   //【13†file*/
    nSys_FillBtl_Can2Lo          : INT;   (* N27[037] *)   //【13†file*/
    nSys_FillBtl_Can3Hi          : INT;   (* N27[038] *)   //【13†file*/
    nSys_FillBtl_Can3Lo          : INT;   (* N27[039] *)   //【13†file*/
    nSys_FillBtl_Can4Hi          : INT;   (* N27[040] *)   //【13†file*/
    nSys_FillBtl_Can4Lo          : INT;   (* N27[041] *)   //【13†file*/

    (* Bottle Status *)
    nSys_BtlStat_NewDataArrival  : INT;   (* N27[050] *)   //【13†file*/
    nSys_BtlStat_ToteID          : INT;   (* N27[051] *)   //【13†file*/
    nSys_BtlStat_Status          : INT;   (* N27[052] *)   //【13†file*/

    (* Tote Eject *)
    nSys_ToteEject_NewDataArrival: INT;   (* N27[060] *)   //【13†file*/
    nSys_ToteEject_ToteID        : INT;   (* N27[061] *)   //【13†file*/
    nSys_ToteEject_Route         : INT;   (* N27[062] *)   //【13†file*/

    (* Fill Complete / FBC out (placeholders; this program primarily receives & queues) *)
    nFBC_NewDataArrival          : INT;   (* N27[110] *)   //【13†file*/
    nFBC_LineId                  : INT;   (* N27[111] *)   //【13†file*/
    nFBC_Cabinet                 : INT;   (* N27[112] *)   //【13†file*/
    nFBC_Quantity                : INT;   (* N27[117] *)   //【13†file*/
    nFBC_BottleStat              : INT;   (* N27[129] *)   //【13†file*/
    nFBC_CanStat                 : INT;   (* N27[130] *)   //【13†file*/
    nFBC_StrayDet                : INT;   (* N27[131] *)   //【13†file*/

    (* Capper Complete *)
    nSys_CappedComp_NewDataArrival : INT; (* N27[140] *)   //【13†file*/
    nSys_CappedComp_LineID         : INT; (* N27[141] *)   //【13†file*/
    nSys_CappedComp_Barcode1       : INT; (* N27[142] *)   //【13†file*/
    nSys_CappedComp_Status         : INT; (* N27[147] *)   //【13†file*/

    (* Tote Info Complete *)
    nSys_ToteInfoComp_NewDataArrival : INT; (* N27[150] *) //【13†file*/
    nSys_ToteInfoComp_LineID         : INT; (* N27[151] *) //【13†file*/
    nSys_ToteInfoComp_ToteNumber     : INT; (* N27[152] *) //【13†file*/
    nSys_ToteInfoComp_ToteID         : INT; (* N27[153] *) //【13†file*/

    (* Diagnostics *)
    nSys_Diag_NewDataArrival     : INT;   (* N27[160] *)   //【13†file*/
    nSys_Diag_LineID             : INT;   (* N27[161] *)   //【13†file*/
    nSys_Diag_MessageID          : INT;   (* N27[162] *)   //【13†file*/
    nSys_Diag_DataWord           : ARRAY[0..17] OF INT; // maps N27[163]..[180] //【13†file*/

    (* ===== Internal working data ===== *)
    bInit                        : BOOL := TRUE;

    (* Active Patient Orders table (max 6 like ladder N13[10],20,30,40,50,60) *)
    ActiveTotes                  : ARRAY[1..6] OF INT;  (* Pseudo/real Tote IDs *)
    ActiveCount                  : INT;

    (* FIFO: Manual Order (Patient) queues; ladder used R46[0]/[1], N17[...] *)
    MANUAL_FIFO_SIZE             : INT := 100;
    Man_Tote_FIFO                : ARRAY[0..99] OF INT;
    Man_Route_FIFO               : ARRAY[0..99] OF INT;
    ManHead                      : INT := 0;
    ManTail                      : INT := 0;
    ManCount                     : INT := 0;

    (* FIFO: Bottle Orders; ladder used R46[2], N15.../N25...; keep simple combined slot *)
    BTL_FIFO_SIZE                : INT := 20;
    Btl_Tote_FIFO                : ARRAY[0..19] OF INT;
    Btl_Cabinet_FIFO             : ARRAY[0..19] OF INT;
    Btl_Qty_FIFO                 : ARRAY[0..19] OF INT;
    Btl_BC1_FIFO                 : ARRAY[0..19] OF INT;
    Btl_BC2_FIFO                 : ARRAY[0..19] OF INT;
    Btl_BC3_FIFO                 : ARRAY[0..19] OF INT;
    BtlHead                      : INT := 0;
    BtlTail                      : INT := 0;
    BtlCount                     : INT := 0;

    (* Error/Diag flags (friendly names replacing B43[]/B33[] from ladder) *)
    err_PatientLimitExceeded     : BOOL := FALSE;  (* ladder B43[1].0 *)  //:contentReference[oaicite:20]{index=20}
    err_DuplicatePatientOrder    : BOOL := FALSE;  (* ladder B43[1].1 *)  //:contentReference[oaicite:21]{index=21}
    err_ManualPickFifoFull       : BOOL := FALSE;  (* ladder B43[1].2 *)  //:contentReference[oaicite:22]{index=22}
    err_InvalidFillerCabinet     : BOOL := FALSE;  (* ladder B43[2].0 *)  //【7†file*/
    err_SorterCapacityExceeded   : BOOL := FALSE;  (* ladder B43[1.4] *)  //【7†file*/

    (* Simple sorter capacity model (from ladder 6 lanes * 5 bottles = 30 max) *)
    SorterAssignedLanes          : INT := 0;       (* computed from external bits in ladder; simplified here *) //:contentReference[oaicite:23]{index=23}
    SorterRemainingLanes         : INT := 6;       //:contentReference[oaicite:24]{index=24}
    SorterRemainingCapacity      : INT := 30;      // 6 lanes * 5 each (ladder rungs 9..17 math) //【7†file*/

    i                            : INT;
END_VAR

(* === Utility functions === *)
LOCAL FUNCTION IsDuplicateToteID(toteID : INT) : BOOL
VAR
    k : INT;
END_VAR
IsDuplicateToteID := FALSE;
FOR k := 1 TO 6 DO
    IF ActiveTotes[k] = toteID THEN
        IsDuplicateToteID := TRUE;
        RETURN;
    END_IF
END_FOR

LOCAL FUNCTION AddActiveTote(toteID : INT) : BOOL
VAR
    k : INT;
END_VAR
AddActiveTote := FALSE;
(* if already present, do nothing *)
IF IsDuplicateToteID(toteID) THEN
    AddActiveTote := TRUE;
    RETURN;
END_IF
FOR k := 1 TO 6 DO
    IF ActiveTotes[k] = 0 THEN
        ActiveTotes[k] := toteID;
        ActiveCount := ActiveCount + 1;
        AddActiveTote := TRUE;
        RETURN;
    END_IF
END_FOR

LOCAL FUNCTION RemoveActiveTote(toteID : INT) : BOOL
VAR
    k : INT;
END_VAR
RemoveActiveTote := FALSE;
FOR k := 1 TO 6 DO
    IF ActiveTotes[k] = toteID THEN
        ActiveTotes[k] := 0;
        IF ActiveCount > 0 THEN ActiveCount := ActiveCount - 1; END_IF
        RemoveActiveTote := TRUE;
        RETURN;
    END_IF
END_FOR

LOCAL FUNCTION ManFIFO_Push(toteID : INT; route : INT) : BOOL
ManFIFO_Push := FALSE;
IF ManCount >= MANUAL_FIFO_SIZE THEN
    err_ManualPickFifoFull := TRUE;  (* ladder sets B43[1].2 *) //:contentReference[oaicite:25]{index=25}
    RETURN;
END_IF
Man_Tote_FIFO[ManHead]  := toteID;
Man_Route_FIFO[ManHead] := route;
ManHead := (ManHead + 1) MOD MANUAL_FIFO_SIZE;
ManCount := ManCount + 1;
ManFIFO_Push := TRUE;

LOCAL FUNCTION BtlFIFO_Push(cab : INT; toteID : INT; qty : INT; bc1 : INT; bc2 : INT; bc3 : INT) : BOOL
BtlFIFO_Push := FALSE;
IF BtlCount >= BTL_FIFO_SIZE THEN
    (* you can add a separate error here if needed *)
    RETURN;
END_IF
Btl_Cabinet_FIFO[BtlHead] := cab;
Btl_Tote_FIFO[BtlHead]    := toteID;
Btl_Qty_FIFO[BtlHead]     := qty;
Btl_BC1_FIFO[BtlHead]     := bc1;
Btl_BC2_FIFO[BtlHead]     := bc2;
Btl_BC3_FIFO[BtlHead]     := bc3;
BtlHead := (BtlHead + 1) MOD BTL_FIFO_SIZE;
BtlCount := BtlCount + 1;
BtlFIFO_Push := TRUE;

(* === MAIN CYCLIC LOGIC === *)

(* One-time init like ladder rungs 0..3: clear tables & reset flags *)
IF bInit THEN
    FOR i:=1 TO 6 DO ActiveTotes[i] := 0; END_FOR;
    ActiveCount := 0;

    ManHead := 0; ManTail := 0; ManCount := 0;
    BtlHead := 0; BtlTail := 0; BtlCount := 0;

    err_PatientLimitExceeded   := FALSE;
    err_DuplicatePatientOrder  := FALSE;
    err_ManualPickFifoFull     := FALSE;
    err_InvalidFillerCabinet   := FALSE;
    err_SorterCapacityExceeded := FALSE;

    (* mirror ladder’s “set handshake idle = 6” on system channels *)
    nSys_ToteInfo_NewDataArrival   := 6;  //:contentReference[oaicite:26]{index=26}
    nSys_FillBtl_NewDataArrival    := 6;  //:contentReference[oaicite:27]{index=27}
    nSys_BtlStat_NewDataArrival    := 6;  //:contentReference[oaicite:28]{index=28}
    nSys_ToteEject_NewDataArrival  := 6;  //:contentReference[oaicite:29]{index=29}
    nFBC_NewDataArrival            := 6;  //:contentReference[oaicite:30]{index=30}
    nSys_CappedComp_NewDataArrival := 6;  //:contentReference[oaicite:31]{index=31}
    nSys_ToteInfoComp_NewDataArrival := 6; //:contentReference[oaicite:32]{index=32}
    nSys_Diag_NewDataArrival       := 6;  //【7†file*/

    bInit := FALSE;
END_IF

(* =========================
   1) PATIENT ORDER / TOTE INFO
   Ladder refs: rungs 4..18
   ========================= *)
IF nSys_ToteInfo_NewDataArrival <> 1 THEN
    (* Treat as new data present; ladder ACKs by writing 6 back *)
    err_DuplicatePatientOrder  := FALSE;
    err_PatientLimitExceeded   := FALSE;

    (* Capacity check (max 6 active) and duplicate check *)
    IF IsDuplicateToteID(nSys_ToteInfo_ToteID) THEN
        err_DuplicatePatientOrder := TRUE;   (* ladder B43[1].1 *) //:contentReference[oaicite:33]{index=33}
    ELSIF ActiveCount >= 6 THEN
        err_PatientLimitExceeded := TRUE;    (* ladder B43[1].0 *) //:contentReference[oaicite:34]{index=34}
    ELSE
        (* Sorter capacity check (ladder rungs 16..17 do 6 - assigned, *5) *)
        SorterRemainingLanes    := 6 - SorterAssignedLanes;
        SorterRemainingCapacity := SorterRemainingLanes * 5; //:contentReference[oaicite:35]{index=35}
        IF nSys_ToteInfo_NumberOfBottles > SorterRemainingCapacity THEN
            err_SorterCapacityExceeded := TRUE; (* ladder B43[1].4 *) //【7†file*/
        ELSE
            (* Queue into manual order FIFO and mark tote active *)
            IF ManFIFO_Push(nSys_ToteInfo_ToteID, nSys_ToteInfo_Route) THEN
                AddActiveTote(nSys_ToteInfo_ToteID);
            END_IF
        END_IF
    END_IF

    (* ACK like ladder: MOVE 6 to handshake *)
    nSys_ToteInfo_NewDataArrival := 6;  //:contentReference[oaicite:36]{index=36}
END_IF

(* =========================
   2) BOTTLE ORDER / FILLBTL
   Ladder refs: rungs 19..29
   ========================= *)
IF nSys_FillBtl_NewDataArrival <> 1 THEN
    err_InvalidFillerCabinet := FALSE;

    IF (nSys_FillBtl_Cabinet < 1) OR (nSys_FillBtl_Cabinet > 5) THEN
        err_InvalidFillerCabinet := TRUE;    (* ladder B43[2].0 *) //【7†file*/
    ELSE
        (* Push a combined bottle-order record into FIFO *)
        BtlFIFO_Push(
            cab := nSys_FillBtl_Cabinet,
            toteID := nSys_FillBtl_ToteID,
            qty := nSys_FillBtl_Quantity,
            bc1 := nSys_FillBtl_Barcode1,
            bc2 := nSys_FillBtl_Barcode2,
            bc3 := nSys_FillBtl_Barcode3
        );
    END_IF

    (* ACK like ladder: MOVE 6 to handshake *)
    nSys_FillBtl_NewDataArrival := 6;  //:contentReference[oaicite:37]{index=37}
END_IF

(* =========================
   3) BOTTLE STATUS
   Ladder refs: rungs 30..34
   ========================= *)
IF nSys_BtlStat_NewDataArrival <> 1 THEN
    (* Example: decide diagnostic based on Status code;
       your ladder branches to NoRead/OutOfSync/OK messages, then clears. *)
    CASE nSys_BtlStat_Status OF
        0: (* OK – bottle matched/loaded; update as needed *);
        1: (* No-Read *);
        2: (* Out-of-Sync *);
        ELSE
           (* Unknown status -> diagnostic message path *)
    END_CASE

    (* ACK *)
    nSys_BtlStat_NewDataArrival := 6;  //:contentReference[oaicite:38]{index=38}
END_IF

(* =========================
   4) TOTE EJECT
   Ladder refs: rungs 35..39
   ========================= *)
IF nSys_ToteEject_NewDataArrival <> 1 THEN
    (* Only eject if tote is in our active table *)
    IF RemoveActiveTote(nSys_ToteEject_ToteID) THEN
        (* In ladder: loads two eject FIFOs with ToteID and Route; here you can
           queue into your eject handling (omitted) or reuse ManFIFO_Push as a placeholder. *)
        ManFIFO_Push(nSys_ToteEject_ToteID, nSys_ToteEject_Route);
    ELSE
        (* Not found: do nothing (ladder skips FIFO load) *)
    END_IF

    (* ACK *)
    nSys_ToteEject_NewDataArrival := 6;  //:contentReference[oaicite:39]{index=39}
END_IF

(* =========================
   5) DIAGNOSTICS (example)
   Ladder refs: rungs 51..62 (Load/Unload diag FIFOs)
   ========================= *)
IF err_PatientLimitExceeded THEN
    nSys_Diag_LineID    := 1;     (* example line id *)
    nSys_Diag_MessageID := 100;   (* “PATIENT ORDER LIMIT EXCEEDED” from ladder r56 *) //:contentReference[oaicite:40]{index=40}
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6; (* load+ack pattern *)
    err_PatientLimitExceeded := FALSE;
END_IF

IF err_DuplicatePatientOrder THEN
    nSys_Diag_LineID    := 1;
    nSys_Diag_MessageID := 101;   (* “DUPLICATE PATIENT ORDER” from ladder r57 *) //【7†file*/
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6;
    err_DuplicatePatientOrder := FALSE;
END_IF

IF err_SorterCapacityExceeded THEN
    nSys_Diag_LineID    := 1;
    nSys_Diag_MessageID := 102;   (* “SORTER CAPACITY EXCEEDED” from ladder r58 *) //【7†file*/
    nSys_Diag_DataWord[0] := SorterRemainingCapacity;
    nSys_Diag_NewDataArrival := 6;
    err_SorterCapacityExceeded := FALSE;
END_IF

IF err_InvalidFillerCabinet THEN
    nSys_Diag_LineID    := 1;
    nSys_Diag_MessageID := 200;   (* custom: invalid cabinet *)
    nSys_Diag_DataWord[0] := nSys_FillBtl_Cabinet;
    nSys_Diag_NewDataArrival := 6;
    err_InvalidFillerCabinet := FALSE;
END_IF

IF err_ManualPickFifoFull THEN
    nSys_Diag_LineID    := 1;
    nSys_Diag_MessageID := 104;   (* “MANUAL PICK FIFO FULL” from ladder r60 *) //【7†file*/
    nSys_Diag_DataWord[0] := ManCount;
    nSys_Diag_NewDataArrival := 6;
    err_ManualPickFifoFull := FALSE;
END_IF
END_PROGRAM

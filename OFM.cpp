PROGRAM PRG_OFM
(*
    Implements your "FIFO UNLOAD LOGIC" steps in ST:

    Steps:
      1) nSys_NewDataArrival (N27[000]) gate
      2) ToteInfo (N27[010])  → Manual order FIFO / lane assignment
      3) FillBtl  (N27[020])  → Bottle FIFO + sorter move trigger
      4) BtlStat  (N27[050])  → barcode status handshake
      5) ToteEject(N27[060])  → Eject FIFO (routing) handshake
      6) FBC     (N27[110])   → filler handshake complete
      7) Capped  (N27[140])   → capped complete handshake
      8) ToteInfoComp (N27[150]) completion word
      9) Patient order diag message when induction complete

    Notes:
      • This version processes when NewDataArrival <> 1 and ACKs with 6 (per your screenshots).
      • Where your ladder used COP/FLL/MOVE, ST uses direct assigns/loops.
      • Manual FIFO and Bottle FIFO provided; lane assignment uses 4 bottles per lane.
*)

VAR
    // ======== System master new-data gate (Step 1) ========
    nSys_NewDataArrival : INT;         // N27[000]

    // ======== Tote Info (Step 2) ========
    nSys_ToteInfo_NewDataArrival : INT;    // N27[010]
    nSys_ToteInfo_ToteID         : INT;    // N27[011]
    nSys_ToteInfo_NumberOfBottles: INT;    // N27[012]
    nSys_ToteInfo_Route          : INT;    // N27[013]

    // ======== Bottle Order / FillBtl (Step 3) ========
    nSys_FillBtl_NewDataArrival  : INT;    // N27[020]
    nSys_FillBtl_Cabinet         : INT;    // N27[021]
    nSys_FillBtl_ToteID          : INT;    // N27[022]
    nSys_FillBtl_Quantity        : INT;    // N27[023]
    nSys_FillBtl_Barcode1        : INT;    // N27[024]
    nSys_FillBtl_Barcode2        : INT;    // N27[025]
    nSys_FillBtl_Barcode3        : INT;    // N27[026]

    // ======== Bottle Status (Step 4) ========
    nSys_BtlStat_NewDataArrival  : INT;    // N27[050]
    nSys_BtlStat_ToteID          : INT;
    nSys_BtlStat_Status          : INT;    // 0 OK, 1 NR, 2 OOS

    // ======== Tote Eject (Step 5) ========
    nSys_ToteEject_NewDataArrival: INT;    // N27[060]
    nSys_ToteEject_ToteID        : INT;
    nSys_ToteEject_Route         : INT;    // “new routing code” target

    // ======== Other handshakes (Steps 6–8) ========
    nFBC_NewDataArrival          : INT;    // N27[110]
    nSys_CappedComp_NewDataArrival : INT;  // N27[140]
    nSys_ToteInfoComp_NewDataArrival : INT;// N27[150]

    // ======== Diagnostics (Step 9) ========
    ENABLE_PATIENT_ORDER_DIAG_MSGS : BOOL := TRUE;
    PATIENT_ORDER_INDUCTION_COMPLETE : BOOL := FALSE;
    // Diag FIFO / packet
    nSys_Diag_NewDataArrival     : INT;
    nSys_Diag_LineID             : INT;
    nSys_Diag_MessageID          : INT;
    nSys_Diag_DataWord           : ARRAY[0..17] OF INT;

    // ======== Internal working ========
    i, k, Lane, BottleReq, Remaining : INT;
    bInit        : BOOL := TRUE;

    // ======== Manual Order FIFO (like R46[1].EN / N15 etc.) ========
    MANUAL_FIFO_SIZE : INT := 100;
    Man_Tote_FIFO    : ARRAY[0..99] OF INT;
    Man_Route_FIFO   : ARRAY[0..99] OF INT;
    ManHead, ManTail, ManCount : INT := 0;
    bManOrderToteFifoEN : BOOL := FALSE;   // unlatch per step 2
    bManOrderRouteFifoEN: BOOL := FALSE;   // unlatch per step 2

    // ======== Bottle FIFO ========
    BTL_FIFO_SIZE    : INT := 20;
    Btl_Tote_FIFO    : ARRAY[0..19] OF INT;
    Btl_Cabinet_FIFO : ARRAY[0..19] OF INT;
    Btl_Qty_FIFO     : ARRAY[0..19] OF INT;
    Btl_BC1_FIFO     : ARRAY[0..19] OF INT;
    Btl_BC2_FIFO     : ARRAY[0..19] OF INT;
    Btl_BC3_FIFO     : ARRAY[0..19] OF INT;
    BtlHead, BtlTail, BtlCount : INT := 0;
    bBottleOrderFifoEN : BOOL := FALSE;   // unlatch per step 3

    // ======== Reject FIFO (order reject load enable step 5) ========
    bOrderRejectFifoLoadEN : BOOL := FALSE;

    // ======== Sorter / Lane Assignment ========
    LANE_CAPACITY : INT := 4;
    LaneTote        : ARRAY[1..6] OF INT;
    LaneBottleCount : ARRAY[1..6] OF INT;
    LaneAssigned    : ARRAY[1..6] OF BOOL;

    // Track active totes (max 6)
    ActiveTotes : ARRAY[1..6] OF INT := [0,0,0,0,0,0];
    ActiveCount : INT := 0;

    // ======== Sorter interface ========
    g_bSorterReady        : BOOL;
    g_iTargetLane         : INT;
    g_bMoveReq            : BOOL;
    g_bBottleDroppedEvent : BOOL;
    g_iBottleDroppedLane  : INT;
    g_iBottleDroppedTote  : INT;
    g_bLaneClearedEvent   : BOOL;
    g_iLaneCleared        : INT;

    // ======== Flags / errors ========
    err_PatientLimitExceeded   : BOOL := FALSE;
    err_DuplicatePatientOrder  : BOOL := FALSE;
    err_ManualPickFifoFull     : BOOL := FALSE;
    err_InvalidFillerCabinet   : BOOL := FALSE;
    err_SorterCapacityExceeded : BOOL := FALSE;

    // ======== Misc ========
    freeLanes, usedLanes : INT;
    bBottleFillCompleteMsgSent : BOOL := FALSE;   // rung 43 emulation
END_VAR

// =============== INIT =================================================
IF bInit THEN
    FOR i := 1 TO 6 DO
        LaneTote[i] := 0; LaneBottleCount[i] := 0; LaneAssigned[i] := FALSE;
        ActiveTotes[i] := 0;
    END_FOR
    ActiveCount := 0;

    ManHead := 0; ManTail := 0; ManCount := 0;
    BtlHead := 0; BtlTail := 0; BtlCount := 0;

    // default ACK (6) for all channels
    nSys_NewDataArrival := 6;
    nSys_ToteInfo_NewDataArrival := 6;
    nSys_FillBtl_NewDataArrival  := 6;
    nSys_BtlStat_NewDataArrival  := 6;
    nSys_ToteEject_NewDataArrival:= 6;
    nFBC_NewDataArrival          := 6;
    nSys_CappedComp_NewDataArrival := 6;
    nSys_ToteInfoComp_NewDataArrival := 6;
    nSys_Diag_NewDataArrival     := 6;

    bInit := FALSE;
END_IF

// =====================================================================
// STEP 1: nSys_NewDataArrival gate (N27[000])
// Ladder: IF N27[000] <> 1 THEN 2;
// In ST we simply proceed (this gate can be used to pause processing)
// =====================================================================
IF nSys_NewDataArrival <> 1 THEN
    // allow steps 2.. onward to run
END_IF

// =====================================================================
// STEP 2: ToteInfo (N27[010]) – Receive Patient Order
// Ladder: if <>1 then UNLATCH R46[1].ENs and process order
// =====================================================================
IF nSys_ToteInfo_NewDataArrival <> 1 THEN
    // unlatch manual FIFO enables (ladder style)
    bManOrderToteFifoEN  := FALSE;
    bManOrderRouteFifoEN := FALSE;

    // Duplicate & 6-patient limit checks
    err_DuplicatePatientOrder := FALSE;
    FOR k := 1 TO 6 DO
        IF ActiveTotes[k] = nSys_ToteInfo_ToteID THEN
            err_DuplicatePatientOrder := TRUE;
            EXIT;
        END_IF
    END_FOR

    IF err_DuplicatePatientOrder THEN
        // diag will be sent later
    ELSIF ActiveCount >= 6 THEN
        err_PatientLimitExceeded := TRUE;
    ELSE
        // Check remaining lane capacity (free lanes * 4)
        freeLanes := 0;
        FOR Lane := 1 TO 6 DO
            IF NOT LaneAssigned[Lane] THEN freeLanes := freeLanes + 1; END_IF
        END_FOR
        IF nSys_ToteInfo_NumberOfBottles > (freeLanes * LANE_CAPACITY) THEN
            err_SorterCapacityExceeded := TRUE;
        ELSE
            // push to Manual FIFO (trace)
            IF ManCount < MANUAL_FIFO_SIZE THEN
                Man_Tote_FIFO[ManHead]  := nSys_ToteInfo_ToteID;
                Man_Route_FIFO[ManHead] := nSys_ToteInfo_Route;
                ManHead := (ManHead + 1) MOD MANUAL_FIFO_SIZE;
                ManCount := ManCount + 1;

                // add to ActiveTotes
                FOR k := 1 TO 6 DO
                    IF ActiveTotes[k] = 0 THEN
                        ActiveTotes[k] := nSys_ToteInfo_ToteID;
                        ActiveCount := ActiveCount + 1;
                        EXIT;
                    END_IF
                END_FOR

                // assign lanes (4 per lane)
                BottleReq := nSys_ToteInfo_NumberOfBottles;
                FOR Lane := 1 TO 6 DO
                    IF (BottleReq <= 0) THEN EXIT; END_IF
                    IF NOT LaneAssigned[Lane] THEN
                        LaneTote[Lane] := nSys_ToteInfo_ToteID;
                        IF BottleReq <= LANE_CAPACITY THEN
                            LaneBottleCount[Lane] := BottleReq; BottleReq := 0;
                        ELSE
                            LaneBottleCount[Lane] := LANE_CAPACITY; BottleReq := BottleReq - LANE_CAPACITY;
                        END_IF
                        LaneAssigned[Lane] := TRUE;
                    END_IF
                END_FOR

                // mark induction complete (for diag step)
                PATIENT_ORDER_INDUCTION_COMPLETE := (BottleReq = 0);
            ELSE
                err_ManualPickFifoFull := TRUE;   // FIFO full
            END_IF
        END_IF
    END_IF

    // clear PPS/Kepware ToteInfo and ACK with 6
    nSys_ToteInfo_ToteID := 0;
    nSys_ToteInfo_NumberOfBottles := 0;
    nSys_ToteInfo_Route := 0;
    nSys_ToteInfo_NewDataArrival := 6;
END_IF

// =====================================================================
// STEP 3: FillBtl (N27[020]) – Cabinet 1..5 → Bottle FIFO and sorter trigger
// Ladder rung 21, 28 behavior combined
// =====================================================================
IF nSys_FillBtl_NewDataArrival <> 1 THEN
    // unlatch bottle FIFO enable (ladder: UNLATCH BOTTLE ORDER FIFO ENABLE)
    bBottleOrderFifoEN := FALSE;

    // trigger sorter move to first lane for this tote that still needs bottles
    IF g_bSorterReady THEN
        FOR Lane := 1 TO 6 DO
            IF LaneAssigned[Lane]
               AND (LaneTote[Lane] = nSys_FillBtl_ToteID)
               AND (LaneBottleCount[Lane] > 0) THEN
                g_iTargetLane := Lane;
                g_bMoveReq := TRUE;
                EXIT;
            END_IF
        END_FOR
    END_IF

    // Cabinet validity + Bottle FIFO trace
    IF (nSys_FillBtl_Cabinet >= 1) AND (nSys_FillBtl_Cabinet <= 5) THEN
        IF BtlCount < BTL_FIFO_SIZE THEN
            Btl_Cabinet_FIFO[BtlHead] := nSys_FillBtl_Cabinet;
            Btl_Tote_FIFO[BtlHead]    := nSys_FillBtl_ToteID;
            Btl_Qty_FIFO[BtlHead]     := nSys_FillBtl_Quantity;
            Btl_BC1_FIFO[BtlHead]     := nSys_FillBtl_Barcode1;
            Btl_BC2_FIFO[BtlHead]     := nSys_FillBtl_Barcode2;
            Btl_BC3_FIFO[BtlHead]     := nSys_FillBtl_Barcode3;
            BtlHead := (BtlHead + 1) MOD BTL_FIFO_SIZE;
            BtlCount := BtlCount + 1;
        END_IF
    ELSE
        err_InvalidFillerCabinet := TRUE;
    END_IF

    // clear PPS/Kepware FillBtl & ACK with 6
    nSys_FillBtl_Cabinet := 0;
    nSys_FillBtl_ToteID := 0;
    nSys_FillBtl_Quantity := 0;
    nSys_FillBtl_Barcode1 := 0; nSys_FillBtl_Barcode2 := 0; nSys_FillBtl_Barcode3 := 0;
    nSys_FillBtl_NewDataArrival := 6;
END_IF

// =====================================================================
// STEP 4: Bottle Status (N27[050]) – barcode result handshakes
// =====================================================================
IF nSys_BtlStat_NewDataArrival <> 1 THEN
    // handle 0 OK, 1 No-Read, 2 Out-of-Sync (extend as needed)
    // ... (place custom actions if you need)
    nSys_BtlStat_ToteID := 0;
    nSys_BtlStat_Status := 0;
    nSys_BtlStat_NewDataArrival := 6;
END_IF

// =====================================================================
// STEP 5: Tote Eject (N27[060]) – Reject FIFO EN unlatch + Ejection FIFO2
// =====================================================================
IF nSys_ToteEject_NewDataArrival <> 1 THEN
    // unlatch ORDER REJECT FIFO LOAD ENABLE
    bOrderRejectFifoLoadEN := FALSE;

    // ORDER EJECTION FIFO 2 (NEW ROUTING CODE) – emulate as direct command:
    // (If you have a dedicated eject FIFO, enqueue here; else leave as a direct command)

    // ACK handshake
    nSys_ToteEject_ToteID := 0;
    nSys_ToteEject_Route := 0;
    nSys_ToteEject_NewDataArrival := 6;
END_IF

// =====================================================================
// STEP 6: FBC Fill Complete handshake (N27[110])
// Ladder: if BOTTLE_FILL_COMPLETE_MSG_SENT then MOVE 1 to N27[110]; CLR B43[5]
// =====================================================================
IF bBottleFillCompleteMsgSent THEN
    nFBC_NewDataArrival := 1;    // host sees this, then will send back 6 typically
    // clear any local coil here if you have one
    bBottleFillCompleteMsgSent := FALSE;
END_IF
// If host responded, it will set nFBC_NewDataArrival back to 6 externally.

// =====================================================================
// STEP 7: Capped Complete handshake (N27[140])
// (Your ladder gates it; here we just leave the var to be handled externally)
// =====================================================================
// if you need to assert it here, add logic similar to step 6.

// =====================================================================
// STEP 8: ToteInfoComp (N27[150]) gate – nothing to do except observe
// =====================================================================
// if nSys_ToteInfoComp_NewDataArrival <> 6 then ... your actions ...

// =====================================================================
// STEP 9: Patient Order DIAG messages after induction complete
// Ladder copies multiple words; here we package a diag and push
// =====================================================================
IF ENABLE_PATIENT_ORDER_DIAG_MSGS AND PATIENT_ORDER_INDUCTION_COMPLETE THEN
    nSys_Diag_LineID := 1;
    nSys_Diag_MessageID := 900;           // your custom “Induction Complete”
    nSys_Diag_DataWord[0] := 0;           // fill as needed
    nSys_Diag_DataWord[1] := 0;
    // zero remaining
    FOR i := 2 TO 17 DO nSys_Diag_DataWord[i] := 0; END_FOR
    nSys_Diag_NewDataArrival := 6;        // send packet
    // reset flag so we don’t spam
    PATIENT_ORDER_INDUCTION_COMPLETE := FALSE;
END_IF

// =====================================================================
// SORTER FEEDBACK: bottle dropped → decrement lane counter
// =====================================================================
IF g_bBottleDroppedEvent THEN
    IF (g_iBottleDroppedLane >= 1) AND (g_iBottleDroppedLane <= 6) THEN
        IF LaneAssigned[g_iBottleDroppedLane]
           AND (LaneTote[g_iBottleDroppedLane] = g_iBottleDroppedTote)
           AND (LaneBottleCount[g_iBottleDroppedLane] > 0) THEN
            LaneBottleCount[g_iBottleDroppedLane] := LaneBottleCount[g_iBottleDroppedLane] - 1;
        END_IF
    END_IF
    g_bBottleDroppedEvent := FALSE;
END_IF

// Lane cleared (gate cycle complete) → free lane
IF g_bLaneClearedEvent THEN
    IF (g_iLaneCleared >= 1) AND (g_iLaneCleared <= 6) THEN
        LaneTote[g_iLaneCleared] := 0;
        LaneBottleCount[g_iLaneCleared] := 0;
        LaneAssigned[g_iLaneCleared] := FALSE;
    END_IF
    g_bLaneClearedEvent := FALSE;
END_IF

// =====================================================================
// ORDER PROGRESS: auto-detect tote completion and issue eject message
// =====================================================================
FOR k := 1 TO 6 DO
    IF ActiveTotes[k] <> 0 THEN
        Remaining := 0;
        FOR Lane := 1 TO 6 DO
            IF LaneAssigned[Lane] AND (LaneTote[Lane] = ActiveTotes[k]) THEN
                Remaining := Remaining + LaneBottleCount[Lane];
            END_IF
        END_FOR

        IF Remaining = 0 THEN
            // Send eject to host (step 5 handshake will ACK)
            nSys_ToteEject_ToteID := ActiveTotes[k];
            nSys_ToteEject_Route  := 0; // set a stored per-tote route if you have it
            nSys_ToteEject_NewDataArrival := 1;

            // remove from active list
            ActiveTotes[k] := 0;
            IF ActiveCount > 0 THEN ActiveCount := ActiveCount - 1; END_IF
        END_IF
    END_IF
END_FOR

// =====================================================================
// ERROR REPORTING (mirrors your ladder’s diag rungs)
// =====================================================================
IF err_PatientLimitExceeded THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 100; // Patient limit exceeded
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6; err_PatientLimitExceeded := FALSE;
END_IF;

IF err_DuplicatePatientOrder THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 101; // Duplicate patient order
    nSys_Diag_DataWord[0] := nSys_ToteInfo_ToteID;
    nSys_Diag_NewDataArrival := 6; err_DuplicatePatientOrder := FALSE;
END_IF;

IF err_SorterCapacityExceeded THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 102; // Sorter capacity exceeded
    nSys_Diag_DataWord[0] := 0;
    nSys_Diag_NewDataArrival := 6; err_SorterCapacityExceeded := FALSE;
END_IF;

IF err_InvalidFillerCabinet THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 200; // Invalid cabinet
    nSys_Diag_DataWord[0] := nSys_FillBtl_Cabinet;
    nSys_Diag_NewDataArrival := 6; err_InvalidFillerCabinet := FALSE;
END_IF;

IF err_ManualPickFifoFull THEN
    nSys_Diag_LineID := 1; nSys_Diag_MessageID := 104; // Manual FIFO full
    nSys_Diag_DataWord[0] := ManCount;
    nSys_Diag_NewDataArrival := 6; err_ManualPickFifoFull := FALSE;
END_IF;

// =====================================================================
// AUTO-RESET WHEN IDLE (optional – keeps memory clean)
// =====================================================================
IF ActiveCount = 0 THEN
    usedLanes := 0;
    FOR Lane := 1 TO 6 DO IF LaneAssigned[Lane] THEN usedLanes := usedLanes + 1; END_IF END_FOR;
    IF usedLanes = 0 THEN
        // clear FIFOs and arrays
        FOR i := 1 TO 6 DO
            LaneTote[i] := 0; LaneBottleCount[i] := 0; LaneAssigned[i] := FALSE;
            ActiveTotes[i] := 0;
        END_FOR
        ActiveCount := 0;

        ManHead := 0; ManTail := 0; ManCount := 0;
        BtlHead := 0; BtlTail := 0; BtlCount := 0;
    END_IF
END_IF

// =====================================================================
// SPECIAL HANDSHAKES FROM YOUR NOTES
// • Send 3 when barcode NR/No Response/OOS  → set nSys_BtlStat_NewDataArrival := 3
// • Send 2 when Manual Order FIFO full while new ToteInfo present
// =====================================================================
// Barcode failure example trigger (tie to your reader logic):
// IF BarcodeNoRead OR BarcodeNoResponse OR OutOfSync THEN
//     nSys_BtlStat_NewDataArrival := 3;
// END_IF;

// Manual FIFO full while ToteInfo words nonzero:
IF (nSys_ToteInfo_ToteID <> 0) AND (nSys_ToteInfo_NumberOfBottles <> 0) AND (ManCount >= MANUAL_FIFO_SIZE) THEN
    nSys_ToteInfo_NewDataArrival := 2; // host sees "2 = FIFO full"
//  MANUAL PICK FIFO FULL bit already set above via err_ManualPickFifoFull
END_IF

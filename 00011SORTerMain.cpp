
(*
===============================================================================
  TYPE DEFINITIONS
===============================================================================
*)
TYPE ST_OrderDetails :
STRUCT
    BottleBarcode : STRING(30);
    Quantity      : INT;
    CabinetNo     : INT;
    Status        : INT; // 0=pending, 1=done
END_STRUCT
END_TYPE

TYPE ST_AddedOrder :
STRUCT
    LaneNumber   : INT;
    ToteID       : DINT;
    OrderDetails : ARRAY[1..5] OF ST_OrderDetails;
    BottleCount  : INT;   // how many bottles currently in lane
    IsFull       : BOOL;  // TRUE when 5 bottles reached
END_STRUCT
END_TYPE


(*
===============================================================================
  VARIABLES
===============================================================================
*)
VAR
    // --- Original sorter variables (unchanged) ---
    bOptifillMachineInAutoMode : BOOL;
    bOptiFillMachineInManualMode : BOOL;
    bCycleStart : BOOL;
    bTransferServoAtCapper : BOOL;
    bPrevTransferAtCapper : BOOL;
    bSorterDone : BOOL;
    bSorterPENDWEND : BOOL;
    bSorterOpMode : BOOL;
    bSorterJISE : BOOL;
    bSorterCSTRPWRT : BOOL;
    bPositionCmdBit1 : BOOL;
    bPositionCmdBit2 : BOOL;
    bPositionCmdBit3 : BOOL;
    iSorterStep : INT := 0;
    iSorterCurrentLane : INT;
    iPM : INT;
    iTargetLane : INT;
    iFilledCount : INT;
    iBottleStatusAtCapper : INT;
    aWIPOrders : ARRAY[1..6] OF ST_AddedOrder; // placeholder if used in your project
    aFilledOrders : ARRAY[1..1000] OF ST_AddedOrder;

    // --- Added Lane/Order structures ---
    aAddedOrders : ARRAY[1..6] OF ST_AddedOrder;
    aCompletedOrders : ARRAY[1..1000] OF ST_AddedOrder;
    iCompletedCount  : INT := 1;

    // FIFO for completed orders
    fbCompletedOrdersFifo : FB_MemRingBuffer;
    arrCompletedOrdersBuffer : ARRAY[0..SIZEOF(ST_AddedOrder) * 1000] OF BYTE;
    stCompletedOrder : ST_AddedOrder;
    bOkCompleted : BOOL;

    // Added variables
    bSorterUnloadGateOpen : BOOL;
    bSorterUnloadRequest : ARRAY[1..6] OF BOOL;
    iSorterUnloadStep : INT := 0;
    bTotePresent : BOOL;
    bLiftDown : BOOL;
    bLiftGripperOpen : BOOL;
    thisTote : DINT;
    thisLane : INT;
    bottlesDone, totalBottles : INT;
    iLane, iBottle : INT;
    thisIsPass : BOOL;
    tgtLane : INT;

    // Sorter lane status
    aSorterLaneAssigned : ARRAY[1..6] OF BOOL;
    bSorterLaneOn : ARRAY[1..6] OF BOOL;
    aSorterLaneToteID : ARRAY[1..6] OF DINT;

    bInit : BOOL := TRUE;
END_VAR


(*
===============================================================================
  INITIALIZATION
===============================================================================
*)
IF bInit THEN
    fbCompletedOrdersFifo(
        pBuffer := ADR(arrCompletedOrdersBuffer),
        cbBuffer := SIZEOF(arrCompletedOrdersBuffer)
    );
    fbCompletedOrdersFifo.A_Reset(bOk => bOkCompleted);
    bInit := FALSE;
END_IF


(*
===============================================================================
  AUTO MODE SORTER LOGIC  (Original logic preserved)
===============================================================================
*)

IF (bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode) THEN
 
    // 1. On bottle arrival at capper, decide target lane
    IF bTransferServoAtCapper THEN
        thisIsPass := (iBottleStatusAtCapper = 1);
        thisTote := aFilledOrders[iFilledCount-1].ToteID; // as per your structure

        IF thisIsPass THEN
            tgtLane := 0;
            // Lookup directly from aWIP array using ToteID
            FOR iLane := 1 TO 6 DO
                IF (aWIPOrders[iLane].ToteID = thisTote) THEN
                    // Determine which lane to use based on bottles processed
                    bottlesDone := aWIPOrders[iLane].BottleCount;
                    IF bottlesDone < 5 THEN
                        tgtLane := aWIPOrders[iLane].LaneNumber;
                    ELSE
                        tgtLane := 7;
                    END_IF;
                    EXIT;
                END_IF;
            END_FOR;

            IF tgtLane <> 0 THEN
                iTargetLane := tgtLane;
            ELSE
                iTargetLane := 7; // Fallback
            END_IF;
        ELSE
            iTargetLane := 7; // Rejected bottle
        END_IF;
    END_IF;
    bPrevTransferAtCapper := bTransferServoAtCapper;
 
    // 2. Move to Target Lane using Step Logic
    CASE iSorterStep OF
        0:
            IF ((iTargetLane >= 1 AND iTargetLane <= 6) OR (iTargetLane = 7)) AND NOT bSorterDone THEN
                IF (iSorterCurrentLane <> iTargetLane) OR (iPM <> iTargetLane) THEN
                    bSorterOpMode := FALSE;
                    bSorterJISE := FALSE;
 
                    bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
                    bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
                    bPositionCmdBit3 := (iTargetLane AND 4) <> 0;
 
                    iSorterStep := 10;
                END_IF;
            END_IF;
 
        10:
            bSorterCSTRPWRT := TRUE;
            iSorterStep := 20;
 
        20:
            tCstrPulse(IN := TRUE, PT := T#10MS);
            IF tCstrPulse.Q THEN
                bSorterCSTRPWRT := FALSE;
                tCstrPulse(IN := FALSE);
            END_IF;
 
            IF bSorterPENDWEND AND (iPM = iTargetLane) THEN
                iSorterCurrentLane := iPM;
                bSorterDone:= TRUE;
                iSorterStep := 0;
            END_IF;
    END_CASE;
END_IF;


(*
===============================================================================
  ADDED SECTION: TRACKING AND UNLOADING WHEN LANE FULL OR ORDER COMPLETE
===============================================================================
*)

FOR iLane := 1 TO 6 DO
    IF aAddedOrders[iLane].ToteID <> 0 THEN
        // If 5 bottles or full order processed, prepare unload
        IF aAddedOrders[iLane].IsFull OR
           (aWIPOrders[iLane].BottleCount >= aWIPOrders[iLane].BottleCount) THEN

            // Check all mechanical interlocks
            IF bTotePresent AND bLiftDown AND bLiftGripperOpen AND (NOT bTransferServoAtCapper) THEN
                iTargetLane := iLane;

                CASE iSorterUnloadStep OF
                    0:
                        bSorterUnloadGateOpen := FALSE;
                        bSorterDone := FALSE;
                        iSorterUnloadStep := 10;

                    10: // Move sorter to the lane
                        bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
                        bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
                        bPositionCmdBit3 := (iTargetLane AND 4) <> 0;
                        IF bSorterPENDWEND AND (iPM = iTargetLane) THEN
                            iSorterUnloadStep := 20;
                        END_IF;

                    20: // Gate open and transfer data to CompletedOrders FIFO
                        bSorterUnloadGateOpen := TRUE;

                        stCompletedOrder := aAddedOrders[iLane];

                        fbCompletedOrdersFifo.A_AddTail(
                            pWrite  := ADR(stCompletedOrder),
                            cbWrite := SIZEOF(stCompletedOrder),
                            bOk     => bOkCompleted
                        );

                        // Push to array for HMI/history
                        aCompletedOrders[iCompletedCount] := stCompletedOrder;
                        iCompletedCount := iCompletedCount + 1;
                        IF iCompletedCount > 1000 THEN
                            iCompletedCount := 1;
                        END_IF;

                        // Clear the lane data
                        aAddedOrders[iLane].IsFull := FALSE;
                        aAddedOrders[iLane].BottleCount := 0;
                        aAddedOrders[iLane].ToteID := 0;
                        FOR iBottle := 1 TO 5 DO
                            aAddedOrders[iLane].OrderDetails[iBottle].Status := 0;
                            aAddedOrders[iLane].OrderDetails[iBottle].BottleBarcode := '';
                            aAddedOrders[iLane].OrderDetails[iBottle].Quantity := 0;
                            aAddedOrders[iLane].OrderDetails[iBottle].CabinetNo := 0;
                        END_FOR

                        // Mark sorter lane free again
                        aSorterLaneAssigned[iLane] := FALSE;
                        bSorterLaneOn[iLane] := FALSE;
                        aSorterLaneToteID[iLane] := 0;

                        iSorterUnloadStep := 30;

                    30: // Close gate and finalize unload
                        bSorterUnloadGateOpen := FALSE;
                        bSorterDone := TRUE;
                        iSorterUnloadStep := 0;
                END_CASE
            END_IF
        END_IF
    END_IF
END_FOR

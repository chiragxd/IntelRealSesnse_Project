TYPE WIP_Order :
STRUCT
    Active       : BOOL;   // Whether this WIP slot is in use
    ToteID       : INT;    // The order’s tote ID
    BottleCount  : INT;    // Total number of bottles required for the order
    laneAssigned : INT;    // The assigned lane number (1–6)
END_STRUCT
END_TYPE

VAR
    aWIP : ARRAY[1..6] OF WIP_Order;


iCurrentOrder       : INT;    // Index to loop over aWIP
iBottleCount        : INT;    // Temporary holder of aWIP[i].BottleCount
iToteID             : INT;    // Temporary holder of aWIP[i].ToteID
laneCount           : INT;    // How many lanes this order needs (1 or 2)
laneFoundCount      : INT;    // Count of lanes already assigned to a tote
laneAssignedCount   : INT;    // Number of lanes assigned in current pass
laneAssigned        : BOOL;   // Flag: is lane(s) already assigned for this order
tgtLane             : INT;    // Resolved target lane for current bottle

aSorterLaneAssigned : ARRAY[1..6] OF BOOL;  // Tracks which lanes are already assigned
aSorterLaneToteID   : ARRAY[1..6] OF INT;   // ToteID assigned to each lane
aSorterLaneBottleCount : ARRAY[1..6] OF INT; // Bottles currently in each lane
iLaneTargetQty      : ARRAY[1..6] OF INT;   // Bottle targets per lane

END_VAR

// Enhanced Auto Mode Logic using aWIP.laneAssigned for direct lane lookup

IF bOptifillMachineInAutoMode AND bCycleStart AND NOT bOptiFillMachineInManualMode THEN

    // 1. Assign lanes for all active WIP orders
    FOR iCurrentOrder := 1 TO 6 DO
        IF aWIP[iCurrentOrder].Active THEN
            iToteID := aWIP[iCurrentOrder].ToteID;
            iBottleCount := aWIP[iCurrentOrder].BottleCount;

            // Decide how many lanes are needed
            IF iBottleCount > LANE_CAPACITY THEN
                laneCount := 2; // split across two lanes
            ELSE
                laneCount := 1;
            END_IF;

            laneAssigned := FALSE;

            // Check if already has lane assigned
            laneFoundCount := 0;
            FOR seekLane := LANE_MIN TO LANE_MAX DO
                IF aSorterLaneToteID[seekLane] = iToteID THEN
                    laneFoundCount := laneFoundCount + 1;
                END_IF;
            END_FOR;

            IF laneFoundCount >= laneCount THEN
                laneAssigned := TRUE;
            END_IF;

            // If not assigned yet, assign lanes and store in aWIP[i].laneAssigned
            IF NOT laneAssigned THEN
                laneAssignedCount := 0;
                FOR seekLane := LANE_MIN TO LANE_MAX DO
                    IF NOT aSorterLaneAssigned[seekLane] THEN
                        aSorterLaneToteID[seekLane] := iToteID;
                        aSorterLaneAssigned[seekLane] := TRUE;

                        IF iBottleCount > LANE_CAPACITY THEN
                            iLaneTargetQty[seekLane] := LANE_CAPACITY;
                            iBottleCount := iBottleCount - LANE_CAPACITY;
                        ELSE
                            iLaneTargetQty[seekLane] := iBottleCount;
                        END_IF;

                        // First assigned lane gets stored in aWIP[i].laneAssigned
                        IF laneAssignedCount = 0 THEN
                            aWIP[iCurrentOrder].laneAssigned := seekLane;
                        END_IF;

                        laneAssignedCount := laneAssignedCount + 1;
                    END_IF;

                    IF laneAssignedCount >= laneCount THEN
                        EXIT;
                    END_IF;
                END_FOR;
            END_IF;

        END_IF;
    END_FOR;

    // 2. On bottle arrival at capper, decide target lane
    IF bTransferServoAtCapper AND (NOT bPrevTransferAtCapper) THEN
        thisIsPass := (iBottleStatus = 1);
        thisToteID := iToteID;

        IF thisIsPass THEN
            tgtLane := 0;
            // Lookup directly from aWIP array using ToteID
            FOR iCurrentOrder := 1 TO 6 DO
                IF aWIP[iCurrentOrder].Active AND (aWIP[iCurrentOrder].ToteID = thisToteID) THEN
                    tgtLane := aWIP[iCurrentOrder].laneAssigned;
                    EXIT;
                END_IF;
            END_FOR;

            IF tgtLane <> 0 THEN
                iTargetLane := tgtLane;
            ELSE
                iTargetLane := REJECT_LANE; // Fallback
            END_IF;
        ELSE
            iTargetLane := REJECT_LANE; // Rejected bottle
        END_IF;
    END_IF;
    bPrevTransferAtCapper := bTransferServoAtCapper;

    // 3. Move to Target Lane using Step Logic
    CASE iSorterStep OF
        0:
            IF (iTargetLane >= LANE_MIN AND iTargetLane <= LANE_MAX) OR (iTargetLane = REJECT_LANE) THEN
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
            tCstrPulse(IN := TRUE, PT := T#10MS);
            iSorterStep := 20;

        20:
            IF tCstrPulse.Q THEN
                bSorterCSTRPWRT := FALSE;
                tCstrPulse(IN := FALSE);
            END_IF;

            IF bSorterPENDWEND AND (iPM = iTargetLane) THEN
                iSorterCurrentLane := iPM;
                iSorterStep := 0;
            END_IF;
    END_CASE;
END_IF;

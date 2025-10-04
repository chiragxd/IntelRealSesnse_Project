PROGRAM PRG_Sorter
VAR
    iSorterStep : INT := 0;
    iCmdPos     : INT := 0;
    iPM         : INT := 0;
    iCurrentLane: INT := -1;

    // teaching
    aTeachPos   : ARRAY[1..7] OF INT; 
    iTeachIndex : INT := 0;

    // timers
    tPCSettle   : TON;
    tCSTRPulse  : TON;
    tTimeout    : TON;
    tGate       : TON;
END_VAR


// -------------------------
// Decode feedback bits (PM)
// -------------------------
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// -------------------------
// AUTO MODE (step machine)
// -------------------------
CASE iSorterStep OF
    0: // Idle
        IF bOptifillMachineInAutoMode AND bSorterStartCmd THEN
            bSorterOpMode := FALSE;
            bSorterJISE   := FALSE;

            iCmdPos := iTargetLane;
            bPositionCmdBit1 := (iCmdPos AND 1) <> 0;
            bPositionCmdBit2 := (iCmdPos AND 2) <> 0;
            bPositionCmdBit3 := (iCmdPos AND 4) <> 0;

            tPCSettle(IN := TRUE, PT := T#10ms);
            iSorterStep := 5;
        END_IF;

    5: // pulse Start
        IF tPCSettle.Q THEN
            tPCSettle(IN := FALSE);
            bSorterCSTRPWRT := TRUE;
            tCSTRPulse(IN := TRUE, PT := T#20ms);
            tTimeout(IN := TRUE, PT := T#10s);
            iSorterStep := 10;
        END_IF;

    10: // wait move complete
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            tCSTRPulse(IN := FALSE);
        END_IF;

        IF bSorterPENDWEND THEN
            IF iPM = iCmdPos THEN
                iCurrentLane := iCmdPos;
                tTimeout(IN := FALSE);
                bExtendSorterTray := TRUE;
                tGate(IN := TRUE, PT := T#2000ms);
                iSorterStep := 20;
            ELSE
                iSorterStep := 900; // mismatch
            END_IF;
        ELSIF tTimeout.Q THEN
            iSorterStep := 900; // timeout
        END_IF;

    20: // retract tray
        IF tGate.Q THEN
            bExtendSorterTray := FALSE;
            bRetractSorterTray := TRUE;
            tGate(IN := TRUE, PT := T#2000ms);
            iSorterStep := 30;
        END_IF;

    30: // done
        IF tGate.Q THEN
            bRetractSorterTray := FALSE;
            tGate(IN := FALSE);
            bBottleSorted := TRUE;
            iSorterStep := 0;
        END_IF;

    900: // fault
        bSorterCSTRPWRT := FALSE;
        bExtendSorterTray := FALSE;
        bRetractSorterTray := FALSE;
        tPCSettle(IN := FALSE); tCSTRPulse(IN := FALSE);
        tTimeout(IN := FALSE); tGate(IN := FALSE);
        iSorterStep := 0;
END_CASE;


// -------------------------
// MANUAL JOG MODE (direct)
// -------------------------
IF bOptifillMachineInManualMode THEN
    bSorterOpMode := TRUE;
    bSorterJISE   := TRUE;

    // Tray jog
    bExtendSorterTray  := bManExtendTray;
    bRetractSorterTray := bManRetractTray;

    // Lane jog
    IF bManMoveSorter THEN
        IF bManualIndexToLane1 THEN iCmdPos := 1; END_IF;
        IF bManualIndexToLane2 THEN iCmdPos := 2; END_IF;
        IF bManualIndexToLane3 THEN iCmdPos := 3; END_IF;
        IF bManualIndexToLane4 THEN iCmdPos := 4; END_IF;
        IF bManualIndexToLane5 THEN iCmdPos := 5; END_IF;
        IF bManualIndexToLane6 THEN iCmdPos := 6; END_IF;
        IF bManualIndexToLane7 THEN iCmdPos := 7; END_IF;

        bPositionCmdBit1 := (iCmdPos AND 1) <> 0;
        bPositionCmdBit2 := (iCmdPos AND 2) <> 0;
        bPositionCmdBit3 := (iCmdPos AND 4) <> 0;

        bSorterCSTRPWRT := TRUE;
    ELSE
        bSorterCSTRPWRT := FALSE;
    END_IF;
END_IF;


// -------------------------
// TEACH MODE (direct)
// -------------------------
IF bTeachMode THEN
    bSorterOpMode := TRUE;
    bSorterJISE   := TRUE;

    // jog like manual
    bExtendSorterTray  := bManExtendTray;
    bRetractSorterTray := bManRetractTray;

    // lane jog
    IF bTeachMoveSorter THEN
        bPositionCmdBit1 := (iTeachIndex AND 1) <> 0;
        bPositionCmdBit2 := (iTeachIndex AND 2) <> 0;
        bPositionCmdBit3 := (iTeachIndex AND 4) <> 0;

        bSorterCSTRPWRT := TRUE;
    ELSE
        bSorterCSTRPWRT := FALSE;
    END_IF;

    // save current position
    IF bTeachSave THEN
        aTeachPos[iTeachIndex] := iPM;
        bTeachSave := FALSE;
    END_IF;
END_IF;

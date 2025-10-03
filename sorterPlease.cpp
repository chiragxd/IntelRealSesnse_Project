PROGRAM PRG_Sorter
VAR
    // Mode
    bSorterAutoMode   : BOOL;   // from HMI / GVL
    bSorterManualMode : BOOL;   // from HMI / GVL

    // Manual commands
    bManExtendTray    : BOOL;   // HMI manual extend
    bManRetractTray   : BOOL;   // HMI manual retract
    bManMoveToLane    : BOOL;   // HMI request move
    iManTargetLane    : INT;    // HMI lane selection

    // Auto commands
    iTargetLane       : INT := 0; // comes from PPS / WIP
    iCurrentLane      : INT := 0;
    bStartAutoMove    : BOOL;     // handshake to start auto sort
    bBottleSorted     : BOOL;     // signals one bottle sorted

    // State machine
    iSorterStep       : INT := 0;

    // Timers
    tPulse            : TON;
    tTimeout          : TON;
    tGateDelay        : TON;
END_VAR


// ==================================================
// MANUAL MODE
// ==================================================
IF bSorterManualMode THEN
    // Operator direct control
    bExtendSorterTray := bManExtendTray;
    bRetractSorterTray := bManRetractTray;

    IF bManMoveToLane THEN
        bPositionCmdBit1 := (iManTargetLane AND 1) <> 0;
        bPositionCmdBit2 := (iManTargetLane AND 2) <> 0;
        bPositionCmdBit3 := (iManTargetLane AND 4) <> 0;

        // Pulse CSTR for move
        bSorterCSTRPWRT := TRUE;
    ELSE
        bSorterCSTRPWRT := FALSE;
    END_IF;

    // Reset auto state machine while in manual
    iSorterStep := 0;
    tPulse(IN := FALSE);
    tTimeout(IN := FALSE);
    tGateDelay(IN := FALSE);

ELSE
// ==================================================
// AUTO MODE
// ==================================================
IF bSorterAutoMode THEN
    // Encode lane command
    bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
    bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
    bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

    CASE iSorterStep OF

        0: // Idle
            IF bStartAutoMove AND (iTargetLane <> iCurrentLane)
               AND bSorterServoDriveReady AND NOT bSorterServoMoving
               AND NOT bSorterServoError THEN

                // Normal op mode
                bSorterOpMode := FALSE;  
                bSorterJISE   := FALSE;

                // Pulse CSTR
                bSorterCSTRPWRT := TRUE;
                tPulse(IN := TRUE, PT := T#100ms);
                tTimeout(IN := TRUE, PT := T#10s);

                iSorterStep := 10;
            END_IF;

        10: // Wait for in-position
            IF tPulse.Q THEN
                bSorterCSTRPWRT := FALSE; // drop pulse
                tPulse(IN := FALSE);
            END_IF;

            IF bSorterPENDWEND THEN
                iCurrentLane := iTargetLane;
                tTimeout(IN := FALSE);
                iSorterStep := 20;
            ELSIF tTimeout.Q THEN
                iSorterStep := 900; // fault
            END_IF;

        20: // Extend tray
            bExtendSorterTray := TRUE;
            bRetractSorterTray := FALSE;
            tGateDelay(IN := TRUE, PT := T#2000ms);
            iSorterStep := 30;

        30: // Retract tray
            IF tGateDelay.Q THEN
                bExtendSorterTray := FALSE;
                bRetractSorterTray := TRUE;
                tGateDelay(IN := TRUE, PT := T#2000ms);
                iSorterStep := 40;
            END_IF;

        40: // Confirm retract complete
            IF tGateDelay.Q THEN
                bRetractSorterTray := FALSE;
                tGateDelay(IN := FALSE);

                // Done for this order
                bBottleSorted := TRUE;
                iSorterStep := 0;
            END_IF;

        900: // Fault state
            bSorterCSTRPWRT := FALSE;
            bExtendSorterTray := FALSE;
            bRetractSorterTray := FALSE;
            tPulse(IN := FALSE);
            tTimeout(IN := FALSE);
            tGateDelay(IN := FALSE);
            // Needs operator reset
    END_CASE;
END_IF
END_IF

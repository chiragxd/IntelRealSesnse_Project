PROGRAM PRG_Sorter
VAR
    iSorterStep : INT := 0;
    iTargetLane : INT := 0;          // Auto target (PPS writes this)
    iTargetLanePrev : INT := -1;
    iCurrentLane : INT := -1;

    // timers
    tPCSettle   : TON;   // >=6ms after setting PC bits
    tCSTRPulse  : TON;   // Start pulse
    tInPosTO    : TON;   // move timeout
    tExtend     : TON;   // tray extend
    tRetract    : TON;   // tray retract
END_VAR


// Manual lane selection 
IF bOptifillMachineInManualMode AND NOT bOptifillMachineInAutoMode THEN
    IF bManualIndexToLane1 THEN iTargetLane := 1; END_IF;
    IF bManualIndexToLane2 THEN iTargetLane := 2; END_IF;
    IF bManualIndexToLane3 THEN iTargetLane := 3; END_IF;
    IF bManualIndexToLane4 THEN iTargetLane := 4; END_IF;
    IF bManualIndexToLane5 THEN iTargetLane := 5; END_IF;
    IF bManualIndexToLane6 THEN iTargetLane := 6; END_IF;
    IF bManualIndexToLane7 THEN iTargetLane := 7; END_IF;
END_IF;


// Small helpers (inline)
IF TRUE THEN
    // write command bits from lane#
    bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
    bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
    bPositionCmdBit3 := (iTargetLane AND 4) <> 0;
END_IF;

VAR
    iPM : INT;
END_VAR
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// State machine
CASE iSorterStep OF

    0: // Idle -> issue move when lane changes
        bSorterOpMode := FALSE;      // Normal mode (teach off)
        bSorterJISE   := FALSE;      // Jog-to-index off
        bSorterCSTRPWRT := FALSE;
        bExtendSorterTray := FALSE;
        bRetractSorterTray := FALSE;

        IF (bOptifillMachineInAutoMode OR bOptifillMachineInManualMode) AND
           (iTargetLane <> iTargetLanePrev) AND
           bSorterServoDriveReady AND NOT bSorterServoError AND NOT bSorterServoMoving THEN

            tPCSettle(IN := TRUE,  PT := T#10ms);    // >=6ms before Start
            iSorterStep := 5;
        END_IF;


    5: // wait PC bits settle, then Start pulse
        IF tPCSettle.Q THEN
            tPCSettle(IN := FALSE);
            bSorterCSTRPWRT := TRUE;                 // CSTR ON
            tCSTRPulse(IN := TRUE, PT := T#20ms);    // >=6ms
            tInPosTO(IN := TRUE,  PT := T#10s);
            iSorterStep := 10;
        END_IF;


    10: // finish Start pulse, wait PEND/WEND
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;               // CSTR OFF (must be low for PEND) 
            tCSTRPulse(IN := FALSE);
        END_IF;

        IF bSorterPENDWEND THEN                      // In position (normal) / Write complete (teach)
            IF (iPM = iTargetLane) OR (iPM = 0) THEN // some tables only assert PEND; accept either exact PM match or PEND-only
                iCurrentLane := iTargetLane;
                tInPosTO(IN := FALSE);
                bExtendSorterTray := TRUE;
                tExtend(IN := TRUE, PT := T#2000ms);
                iSorterStep := 20;
            ELSE
                iSorterStep := 900;                  // wrong lane
            END_IF

        ELSIF tInPosTO.Q THEN
            iSorterStep := 900;                      // timeout
        END_IF;


    20: // extend done -> retract
        IF tExtend.Q THEN
            bExtendSorterTray := FALSE;
            tExtend(IN := FALSE);
            bRetractSorterTray := TRUE;
            tRetract(IN := TRUE, PT := T#2s);
            iSorterStep := 30;
        END_IF;


    30: // retract done -> complete
        IF tRetract.Q THEN
            bRetractSorterTray := FALSE;
            tRetract(IN := FALSE);
            iTargetLanePrev := iTargetLane;
            iSorterStep := 0;
        END_IF;


    900: // fault clear outputs and return to idle step
        bSorterCSTRPWRT := FALSE;
        bExtendSorterTray := FALSE;
        bRetractSorterTray := FALSE;
        tPCSettle(IN := FALSE); tCSTRPulse(IN := FALSE); tInPosTO(IN := FALSE);
        iSorterStep := 0;
END_CASE;

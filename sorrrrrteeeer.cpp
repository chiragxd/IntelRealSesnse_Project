// ======================================
// Sorter Logic (Auto + Manual)
// Using original tags
// ======================================

CASE iSorterStep OF

    // -------------------------
    0: // IDLE
    // -------------------------
        // -------- Manual mode --------
        IF bOptifillMachineInManualMode THEN
            // Operator tray control
            bExtendSorterTray  := bManExtendTray;   // from HMI/GVL
            bRetractSorterTray := bManRetractTray;  // from HMI/GVL

            // Operator lane move request
            IF bManMoveSorter THEN
                bPositionCmdBit1 := (iManTargetLane AND 1) <> 0;
                bPositionCmdBit2 := (iManTargetLane AND 2) <> 0;
                bPositionCmdBit3 := (iManTargetLane AND 4) <> 0;

                bSorterCSTRPWRT := TRUE;   // pulse start
                tPulse(IN := TRUE, PT := T#100ms);
                iSorterStep := 1;          // wait to drop pulse
            END_IF;

        // -------- Auto mode --------
        ELSIF bOptifillMachineInAutoMode AND bSorterStartCmd THEN
            // Encode lane bits
            bPositionCmdBit1 := (iTargetLane AND 1) <> 0;
            bPositionCmdBit2 := (iTargetLane AND 2) <> 0;
            bPositionCmdBit3 := (iTargetLane AND 4) <> 0;

            // Command move
            bSorterOpMode := FALSE;   // normal
            bSorterJISE   := FALSE;
            bSorterCSTRPWRT := TRUE;

            tPulse(IN := TRUE, PT := T#100ms);
            tTimeout(IN := TRUE, PT := T#10s);

            iSorterStep := 10;
        END_IF;


    // -------------------------
    1: // Drop pulse (manual)
    // -------------------------
        IF tPulse.Q THEN
            bSorterCSTRPWRT := FALSE;
            tPulse(IN := FALSE);
            iSorterStep := 0;
        END_IF;


    // -------------------------
    10: // Wait for in-position (auto)
    // -------------------------
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


    // -------------------------
    20: // Extend tray (auto)
    // -------------------------
        bExtendSorterTray := TRUE;
        bRetractSorterTray := FALSE;
        tGateDelay(IN := TRUE, PT := T#2000ms);
        iSorterStep := 30;


    // -------------------------
    30: // Retract tray (auto)
    // -------------------------
        IF tGateDelay.Q THEN
            bExtendSorterTray := FALSE;
            bRetractSorterTray := TRUE;
            tGateDelay(IN := TRUE, PT := T#2000ms);
            iSorterStep := 40;
        END_IF;


    // -------------------------
    40: // Confirm retract complete (auto)
    // -------------------------
        IF tGateDelay.Q THEN
            bRetractSorterTray := FALSE;
            tGateDelay(IN := FALSE);

            // Finished sorting this bottle
            bBottleSorted := TRUE;

            iSorterStep := 0; // back to idle
        END_IF;


    // -------------------------
    900: // FAULT
    // -------------------------
        bSorterCSTRPWRT := FALSE;
        bExtendSorterTray := FALSE;
        bRetractSorterTray := FALSE;

        tPulse(IN := FALSE);
        tTimeout(IN := FALSE);
        tGateDelay(IN := FALSE);

        // Wait for reset command from HMI
END_CASE;

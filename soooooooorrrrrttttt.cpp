PROGRAM PRG_SorterMoveToPosition
VAR
    iStep       : INT := 0;
    iCmdPos     : INT := 0;
    iPM         : INT := 0;

    // timers
    tPCSettle   : TON;
    tCSTRPulse  : TON;
    tTimeout    : TON;
END_VAR


// ---- Decode PM feedback bits (position met) ----
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// ---- Step Machine ----
CASE iStep OF

    0: // Idle → request move
        // Example: iCmdPos comes from order or manual select
        IF bStartMoveCmd THEN
            // Normal mode
            bSorterOpMode := FALSE;
            bSorterJISE   := FALSE;

            // Set PC bits
            bPositionCmdBit1 := (iCmdPos AND 1) <> 0;
            bPositionCmdBit2 := (iCmdPos AND 2) <> 0;
            bPositionCmdBit3 := (iCmdPos AND 4) <> 0;

            // wait >=6ms
            tPCSettle(IN := TRUE, PT := T#10ms);
            iStep := 5;
        END_IF;


    5: // Pulse CSTR
        IF tPCSettle.Q THEN
            tPCSettle(IN := FALSE);
            bSorterCSTRPWRT := TRUE;              // CSTR HIGH
            tCSTRPulse(IN := TRUE, PT := T#20ms); // keep HIGH ≥6ms
            tTimeout(IN := TRUE, PT := T#10s);    // movement timeout
            iStep := 10;
        END_IF;


    10: // Finish pulse, keep CSTR LOW
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE;  // back to LOW
            tCSTRPulse(IN := FALSE);
        END_IF;

        // check in-position
        IF bSorterPENDWEND THEN
            IF iPM = iCmdPos THEN
                tTimeout(IN := FALSE);
                iStep := 100; // success
            ELSE
                iStep := 900; // wrong lane
            END_IF;
        ELSIF tTimeout.Q THEN
            iStep := 900; // fault
        END_IF;


    100: // Done
        bMoveComplete := TRUE;
        iStep := 0;


    900: // Fault
        bSorterCSTRPWRT := FALSE;
        tPCSettle(IN := FALSE); tCSTRPulse(IN := FALSE); tTimeout(IN := FALSE);
        bMoveFault := TRUE;
        iStep := 0;

END_CASE;

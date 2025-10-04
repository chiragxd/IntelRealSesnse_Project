PROGRAM PRG_SorterMoveToPosition
VAR
    iStep       : INT := 0;
    iCmdPos     : INT := 0;   // desired lane (1..7)
    iPM         : INT := 0;   // feedback lane from PM bits

    bMoveRequest   : BOOL;    // trigger to start move
    bMoveComplete  : BOOL;    // done flag
    bMoveFault     : BOOL;    // fault flag

    // timers
    tPCSettle   : TON;
    tCSTRPulse  : TON;
    tTimeout    : TON;
END_VAR


// ---- Decode PM feedback bits (Position Met) ----
iPM := 0;
IF bSorterPositionMetBit1 THEN iPM := iPM + 1; END_IF;
IF bSorterPositionMetBit2 THEN iPM := iPM + 2; END_IF;
IF bSorterPositionMetBit3 THEN iPM := iPM + 4; END_IF;


// ---- State Machine ----
CASE iStep OF

    0: // IDLE
        bMoveComplete := FALSE;
        bMoveFault    := FALSE;

        IF bMoveRequest THEN
            // 1. Normal mode (MODE = LOW)
            bSorterOpMode := FALSE;
            bSorterJISE   := FALSE;

            // if not homed, go home first
            IF NOT bSorterServoHomed THEN
                // could issue home command here
                iStep := 1; 
            ELSE
                iStep := 5; 
            END_IF;
        END_IF;


    1: // HOMING (placeholder, depends on your drive setup)
        // Assume homing handled separately
        IF bSorterServoHomed THEN
            iStep := 5;
        END_IF;


    5: // 2. Set PC bits
        bPositionCmdBit1 := (iCmdPos AND 1) <> 0;
        bPositionCmdBit2 := (iCmdPos AND 2) <> 0;
        bPositionCmdBit3 := (iCmdPos AND 4) <> 0;

        // wait at least 6ms
        tPCSettle(IN := TRUE, PT := T#10ms);
        iStep := 10;


    10: // 3. Start pulse
        IF tPCSettle.Q THEN
            tPCSettle(IN := FALSE);

            bSorterCSTRPWRT := TRUE;               // CSTR HIGH
            tCSTRPulse(IN := TRUE, PT := T#20ms);  // hold ≥6ms
            tTimeout(IN := TRUE, PT := T#10s);     // move timeout
            iStep := 20;
        END_IF;


    20: // Finish pulse
        IF tCSTRPulse.Q THEN
            bSorterCSTRPWRT := FALSE; // CSTR LOW again
            tCSTRPulse(IN := FALSE);
        END_IF;

        // 4. Wait for PEND/WEND HIGH (in position)
        IF bSorterPENDWEND THEN
            // check feedback lane matches commanded
            IF iPM = iCmdPos THEN
                tTimeout(IN := FALSE);
                bMoveComplete := TRUE;
                iStep := 0; // done
            ELSE
                bMoveFault := TRUE; // mismatch
                iStep := 0;
            END_IF;

        ELSIF tTimeout.Q THEN
            bMoveFault := TRUE; // timeout
            iStep := 0;
        END_IF;

END_CASE;

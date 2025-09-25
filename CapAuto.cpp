//======================================================================
// CAPPER AUTO SEQUENCE (Structured Text)
//======================================================================

// ---------------------------
// 1. CAP TRACK / HOPPER CONTROL
// ---------------------------
// If high not triggered for X seconds -> run hopper until high level seen
IF NOT CapTrackHighLevel THEN
    tCapHopperDelay(IN := TRUE, PT := T#5s);
    IF tCapHopperDelay.Q THEN
        CapHopperMotorOutput := TRUE;
    END_IF;
ELSE
    CapHopperMotorOutput := FALSE;
    tCapHopperDelay(IN := FALSE);
END_IF;


// ---------------------------
// 2. IDLE POSITION
// ---------------------------
// Stuffer down, chuck up, chuck at cap side, lift gripper open
IF CapperStep = 0 THEN
    CapStufferRaiseOutput := FALSE;
    ChuckRaiseOutput := TRUE;
    ChuckRetractToCapOutput := TRUE;
    LiftGripperOpenOutput := TRUE;

    IF TransferComplete THEN
        CapperStep := 10;
    END_IF;
END_IF;


// ---------------------------
// 3. MAIN SEQUENCE STATE MACHINE
// ---------------------------
CASE CapperStep OF
    10: // Bottle arrived -> close lift gripper
        LiftGripperCloseOutput := TRUE;
        IF BottleAtLift THEN
            CapperStep := 20;
        END_IF;

    20: // When lift gripper closed, open transfer gripper
        TransferGripperOpen := TRUE;
        IF TransferGripperOpen THEN
            CapperStep := 30;
        END_IF;

    30: // Bottle status decision
        CASE BottleStatus OF
            1: // Pass -> go cap
                CapperStep := 35;
            2: // Reject EmptyBottle -> complete, release
                CapperStep := 200;
            3: // Reject FullBottle -> still cap
                CapperStep := 35;
            ELSE
                // Wait here until status is known
                CapperStep := 30;
        END_CASE;

    35: // Raise stuffer if needed
        IF CapTrackLowLevel AND ChuckHigh AND ChuckAtCapSide THEN
            CapStufferRaiseOutput := TRUE;
            tStufferRaiseDelay(IN := TRUE, PT := T#2s);
            IF tStufferRaiseDelay.Q THEN
                CapStufferRaiseOutput := FALSE;
                CapperStep := 40;
            END_IF;
        END_IF;

    40: // Chuck moves to lift
        ChuckExtendToLiftOutput := TRUE;
        IF ChuckAtLiftSide THEN
            CapperStep := 50;
        END_IF;

    50: // Chuck lowers
        ChuckLowerOutput := TRUE;
        tChuckLowerDelay(IN := TRUE, PT := T#1s);
        IF ChuckDown AND tChuckLowerDelay.Q THEN
            CapperStep := 60;
        END_IF;

    60: // Chuck raises
        ChuckRaiseOutput := TRUE;
        tChuckRaiseDelay(IN := TRUE, PT := T#1s);
        IF ChuckHigh AND tChuckRaiseDelay.Q THEN
            CapperStep := 70;
        END_IF;

    70: // Chuck returns to cap side
        ChuckRetractToCapOutput := TRUE;
        IF ChuckAtCapSide THEN
            CapperStep := 80;
        END_IF;

    80: // Cap check
        tCapCheckDelay(IN := TRUE, PT := T#500MS);
        IF tCapCheckDelay.Q THEN
            IF NOT CapOnBottle THEN
                CapperError := TRUE; // show on HMI
                CapperStep := 999;
            ELSE
                CapperStep := 100;
            END_IF;
        END_IF;

    100: // Sequence complete
        LiftGripperOpenOutput := TRUE;
        IF LiftGripperOpenOutput THEN
            CapperStep := 0;
        END_IF;

    200: // RejectEmptyBottle -> subsystem complete
        LiftGripperOpenOutput := TRUE;
        CapperStep := 0;

    999: // Fault state
        IF ResetRequest THEN
            CapperError := FALSE;
            CapperStep := 0;
        END_IF;
END_CASE;

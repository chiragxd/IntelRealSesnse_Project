//======================================================================
// CAPPER ROUTINE (Manual + Auto)
//======================================================================

IF AutoMode AND NOT ManualMode THEN
    // ---------------------------
    // Hopper Motor Control
    // ---------------------------
    IF NOT CapTrackHighLevel THEN
        tCapHopperDelay(IN := TRUE, PT := T#5s);
        IF tCapHopperDelay.Q THEN
            CapHopperMotorOutput := TRUE;      // O[0].7
        END_IF;
    ELSE
        CapHopperMotorOutput := FALSE;
        tCapHopperDelay(IN := FALSE);
    END_IF;

    // ---------------------------
    // Auto Sequence
    // ---------------------------
    CASE CapperStep OF
        0: // Idle
            LiftGripperOpenOutput := TRUE;     // O[0].0
            LiftGripperCloseOutput := FALSE;
            ChuckRetractToCapOutput := TRUE;   // O[0].3
            ChuckExtendToLiftOutput := FALSE;
            ChuckRaiseOutput := TRUE;          // O[0].5
            ChuckLowerOutput := FALSE;
            CapStufferRaiseOutput := FALSE;
            IF TransferComplete THEN
                CapperStep := 10;
            END_IF;

        10: // Close Lift Gripper
            LiftGripperCloseOutput := TRUE;    // O[0].1
            IF BottleAtLift THEN
                CapperStep := 20;
            END_IF;

        20: // Open Transfer Gripper
            TransferGripperOpen := TRUE;       // O[1].0
            IF TransferGripperOpen THEN
                CapperStep := 30;
            END_IF;

        30: // Bottle Status
            CASE BottleStatus OF
                1: CapperStep := 35;           // Passed
                2: CapperStep := 200;          // Reject Empty
                3: CapperStep := 35;           // Reject Full
                ELSE
                    CapperStep := 30;
            END_CASE;

        35: // Raise Cap Stuffer
            IF CapTrackLowLevel AND ChuckHigh AND ChuckAtCapSide THEN
                CapStufferRaiseOutput := TRUE; // O[0].6
                tStufferRaiseDelay(IN := TRUE, PT := T#2s);
                IF tStufferRaiseDelay.Q THEN
                    CapStufferRaiseOutput := FALSE;
                    CapperStep := 40;
                END_IF;
            ELSE
                CapperStep := 40;
            END_IF;

        40: // Chuck to Lift
            ChuckExtendToLiftOutput := TRUE;   // O[0].2
            IF ChuckAtLiftSide THEN
                CapperStep := 50;
            END_IF;

        50: // Chuck Lower
            ChuckLowerOutput := TRUE;          // O[0].4
            tChuckLowerDelay(IN := TRUE, PT := T#1s);
            IF ChuckDown AND tChuckLowerDelay.Q THEN
                CapperStep := 60;
            END_IF;

        60: // Chuck Raise
            ChuckRaiseOutput := TRUE;          // O[0].5
            tChuckRaiseDelay(IN := TRUE, PT := T#1s);
            IF ChuckHigh AND tChuckRaiseDelay.Q THEN
                CapperStep := 70;
            END_IF;

        70: // Chuck Retract to Cap
            ChuckRetractToCapOutput := TRUE;   // O[0].3
            IF ChuckAtCapSide THEN
                CapperStep := 80;
            END_IF;

        80: // Cap Check
            tCapCheckDelay(IN := TRUE, PT := T#500ms);
            IF tCapCheckDelay.Q THEN
                IF NOT CapOnBottle THEN
                    CapperError := TRUE;       // B133[11].0
                    CapperStep := 999;
                ELSE
                    CapperStep := 100;
                END_IF;
            END_IF;

        100: // Release Bottle
            LiftGripperOpenOutput := TRUE;     // O[0].0
            IF LiftGripperOpenOutput THEN
                CapperStep := 0;
            END_IF;

        200: // Reject Empty
            LiftGripperOpenOutput := TRUE;     // O[0].0
            CapperStep := 0;

        999: // Error
            IF ResetRequest THEN               // B33[5].1
                CapperError := FALSE;
                CapperStep := 0;
            END_IF;
    END_CASE;

END_IF;


//======================================================================
// MANUAL CONTROL
//======================================================================
IF ManualMode AND NOT AutoMode THEN

    // Lift Gripper
    IF LiftGripperOpenCmd THEN
        LiftGripperOpenOutput := TRUE;        // O[0].0
        LiftGripperCloseOutput := FALSE;      // O[0].1
    END_IF;

    IF LiftGripperCloseCmd THEN
        LiftGripperCloseOutput := TRUE;       // O[0].1
        LiftGripperOpenOutput := FALSE;       // O[0].0
    END_IF;

    // Capper Chuck
    IF ChuckExtendCmd THEN
        ChuckExtendToLiftOutput := TRUE;      // O[0].2
        ChuckRetractToCapOutput := FALSE;     // O[0].3
    END_IF;

    IF ChuckRetractCmd THEN
        ChuckRetractToCapOutput := TRUE;      // O[0].3
        ChuckExtendToLiftOutput := FALSE;     // O[0].2
    END_IF;

    IF ChuckLowerCmd THEN
        ChuckLowerOutput := TRUE;             // O[0].4
        ChuckRaiseOutput := FALSE;            // O[0].5
    END_IF;

    IF ChuckRaiseCmd THEN
        ChuckRaiseOutput := TRUE;             // O[0].5
        ChuckLowerOutput := FALSE;            // O[0].4
    END_IF;

    // Cap Stuffer
    IF StufferRaiseCmd THEN
        CapStufferRaiseOutput := TRUE;        // O[0].6
    ELSE
        CapStufferRaiseOutput := FALSE;
    END_IF;

    // Hopper Motor
    IF HopperMotorCmd THEN
        CapHopperMotorOutput := TRUE;         // O[0].7
    ELSE
        CapHopperMotorOutput := FALSE;
    END_IF;

    // Transfer Gripper (shared)
    IF TransferGripperOpenCmd THEN
        TransferGripperOpen := TRUE;          // O[1].0
    ELSE
        TransferGripperOpen := FALSE;
    END_IF;

END_IF;

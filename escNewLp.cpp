CASE nEscState OF
    0: // Idle
        // Brake engaged, motors off
        bReleaseBottleEscapementMotorBrake := FALSE;
        bStartEscapementMotorSmallBottleCCW := FALSE;
        bStartEscapementMotorLargeBottleCW := FALSE;
        bStartSmallBottleUnscramblerMotor := FALSE;
        bStartLargeBottleUnscramblerMotor := FALSE;

        // If a queued request exists → process it first
        IF bNextSmallBottleRequest THEN
            bSmallBottleSelected := TRUE;
            bNextSmallBottleRequest := FALSE;
            nEscState := 10;

        ELSIF bNextLargeBottleRequest THEN
            bLargeBottleSelected := TRUE;
            bNextLargeBottleRequest := FALSE;
            nEscState := 10;

        // Otherwise if PPS sends a new request
        ELSIF (bSmallBottleSelected OR bLargeBottleSelected) AND NOT bBottlePresentAtLabeler THEN
            nEscState := 10;
        END_IF;

    10: // Rotate to pickup (Descrambler ON)
        bReleaseBottleEscapementMotorBrake := TRUE;

        IF bSmallBottleSelected THEN
            bStartEscapementMotorSmallBottleCCW := TRUE;
            bStartSmallBottleUnscramblerMotor := TRUE;
        ELSIF bLargeBottleSelected THEN
            bStartEscapementMotorLargeBottleCW := TRUE;
            bStartLargeBottleUnscramblerMotor := TRUE;
        END_IF;

        // If a *new PPS order* arrives while busy → queue it
        IF (bNewSmallBottleOrder AND NOT bSmallBottleSelected) THEN
            bNextSmallBottleRequest := TRUE;
        ELSIF (bNewLargeBottleOrder AND NOT bLargeBottleSelected) THEN
            bNextLargeBottleRequest := TRUE;
        END_IF;

        // Stop at pickup
        IF bBottleEscapementDiskInPosition THEN
            bStartEscapementMotorSmallBottleCCW := FALSE;
            bStartEscapementMotorLargeBottleCW := FALSE;
            bReleaseBottleEscapementMotorBrake := FALSE; // brake ON

            tEscStep(IN := TRUE, PT := T#2S); // settle bottle into slot
            nEscState := 15;
        END_IF;

    15: // Wait dwell at pickup
        IF tEscStep.Q THEN
            tEscStep(IN := FALSE);

            // Reverse toward labeler
            bReleaseBottleEscapementMotorBrake := TRUE;
            IF bSmallBottleSelected THEN
                bStartEscapementMotorLargeBottleCW := TRUE;
            ELSIF bLargeBottleSelected THEN
                bStartEscapementMotorSmallBottleCCW := TRUE;
            END_IF;

            nEscState := 20;
        END_IF;

    20: // Rotate to labeler
        IF bBottleEscapementDiskAtLabeler THEN
            bStartEscapementMotorSmallBottleCCW := FALSE;
            bStartEscapementMotorLargeBottleCW := FALSE;
            bReleaseBottleEscapementMotorBrake := FALSE;
            bStartSmallBottleUnscramblerMotor := FALSE;
            bStartLargeBottleUnscramblerMotor := FALSE;

            IF bBottlePresentAtLabeler THEN
                // Finished current job
                bSmallBottleSelected := FALSE;
                bLargeBottleSelected := FALSE;
                nEscState := 0;  // go idle → next queued job will run
            END_IF;
        END_IF;
END_CASE;

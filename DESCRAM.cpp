// -------------------------------
// Manual Mode
// -------------------------------
IF bOptifillMachineInManualMode AND NOT bOptifillMachineInAutoMode THEN

    // Hoppers
    bStartSmallBottleHopperMotor := bStartSmallBottleHopperMotorHMIPB;
    bStartLargeBottleHopperMotor := bStartLargeBottleHopperMotorHMIPB;

    // Unscramblers Forward
    bStartSmallBottleUnscramblerMotor := bStartSmallBottleUnscramblerMotorHMIPB;
    bStartLargeBottleUnscramblerMotor := bStartLargeBottleUnscramblerMotorHMIPB;

    // Unscramblers Reverse
    bReverseSmallBottleUnscramblerMotor := bStartSmallBottleDescramblerMotorHMIPB;
    bReverseLargeBottleUnscramblerMotor := bStartLargeBottleDescramblerMotorHMIPB;

END_IF;

// -------------------------------
// Auto Mode
// -------------------------------
IF bOptifillMachineInAutoMode AND NOT bOptifillMachineInManualMode THEN

    // ---------------- Small Hopper ----------------
    IF (NOT bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        bStartSmallBottleHopperMotor := TRUE;
    ELSIF (NOT bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        tSmallHopperNotFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tSmallHopperNotFullDelayTmr.Q THEN
            bStartSmallBottleHopperMotor := TRUE;
        END_IF;
    ELSIF (bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        tSmallHopperFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tSmallHopperFullDelayTmr.Q THEN
            bStartSmallBottleHopperMotor := FALSE;
        END_IF;
    ELSIF (bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        bStartSmallBottleHopperMotor := FALSE;
    END_IF;

    // ---------------- Large Hopper ----------------
    IF (NOT bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        bStartLargeBottleHopperMotor := TRUE;
    ELSIF (NOT bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        tLargeHopperNotFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tLargeHopperNotFullDelayTmr.Q THEN
            bStartLargeBottleHopperMotor := TRUE;
        END_IF;
    ELSIF (bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        tLargeHopperFullDelayTmr(IN := TRUE, PT := T#5S);
        IF tLargeHopperFullDelayTmr.Q THEN
            bStartLargeBottleHopperMotor := FALSE;
        END_IF;
    ELSIF (bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        bStartLargeBottleHopperMotor := FALSE;
    END_IF;

    // ---------------- Small Descrambler ----------------
    IF (NOT bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) OR
       (NOT bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        bStartSmallBottleUnscramblerMotor := TRUE;
        bReverseSmallBottleUnscramblerMotor := FALSE;

    ELSIF (bSmallBottlePresentAtHopper AND bSmallBottlePresentAtEscapement) THEN
        bStartSmallBottleUnscramblerMotor := FALSE;
        bReverseSmallBottleUnscramblerMotor := FALSE;

    ELSIF (bSmallBottlePresentAtHopper AND NOT bSmallBottlePresentAtEscapement) THEN
        IF (bSmallDescrJam AND NOT prevSmallJam) THEN
            nSmallDescrJamCount := nSmallDescrJamCount + 1;
            IF nSmallDescrJamCount >= nMaxDescrJamRetries THEN
                bSmallDescrFault := TRUE;
            ELSE
                tSmallDescrRevPulseTmr(IN := TRUE, PT := T#5S);
                tSmallDescrFwdPulseTmr(IN := TRUE, PT := T#10S);
            END_IF;
        END_IF;

        IF tSmallDescrRevPulseTmr.IN AND NOT tSmallDescrRevPulseTmr.Q THEN
            bStartSmallBottleUnscramblerMotor := FALSE;
            bReverseSmallBottleUnscramblerMotor := TRUE;
        ELSIF tSmallDescrFwdPulseTmr.IN AND NOT tSmallDescrFwdPulseTmr.Q THEN
            bReverseSmallBottleUnscramblerMotor := FALSE;
            bStartSmallBottleUnscramblerMotor := TRUE;
        ELSE
            tSmallDescrRevPulseTmr(IN := FALSE);
            tSmallDescrFwdPulseTmr(IN := FALSE);
        END_IF;
    END_IF;

    // ---------------- Large Descrambler ----------------
    IF (NOT bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) OR
       (NOT bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        bStartLargeBottleUnscramblerMotor := TRUE;
        bReverseLargeBottleUnscramblerMotor := FALSE;

    ELSIF (bLargeBottlePresentAtHopper AND bLargeBottlePresentAtEscapement) THEN
        bStartLargeBottleUnscramblerMotor := FALSE;
        bReverseLargeBottleUnscramblerMotor := FALSE;

    ELSIF (bLargeBottlePresentAtHopper AND NOT bLargeBottlePresentAtEscapement) THEN
        IF (bLargeDescrJam AND NOT prevLargeJam) THEN
            nLargeDescrJamCount := nLargeDescrJamCount + 1;
            IF nLargeDescrJamCount >= nMaxDescrJamRetries THEN
                bLargeDescrFault := TRUE;
            ELSE
                tLargeDescrRevPulseTmr(IN := TRUE, PT := T#5S);
                tLargeDescrFwdPulseTmr(IN := TRUE, PT := T#10S);
            END_IF;
        END_IF;

        IF tLargeDescrRevPulseTmr.IN AND NOT tLargeDescrRevPulseTmr.Q THEN
            bStartLargeBottleUnscramblerMotor := FALSE;
            bReverseLargeBottleUnscramblerMotor := TRUE;
        ELSIF tLargeDescrFwdPulseTmr.IN AND NOT tLargeDescrFwdPulseTmr.Q THEN
            bReverseLargeBottleUnscramblerMotor := FALSE;
            bStartLargeBottleUnscramblerMotor := TRUE;
        ELSE
            tLargeDescrRevPulseTmr(IN := FALSE);
            tLargeDescrFwdPulseTmr(IN := FALSE);
        END_IF;
    END_IF;

END_IF;

// -------------------------------
// Previous jam flags
// -------------------------------
prevSmallJam := bSmallDescrJam;
prevLargeJam := bLargeDescrJam;

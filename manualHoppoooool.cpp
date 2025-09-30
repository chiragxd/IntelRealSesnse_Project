PROGRAM PRG_Hopper_Manual
VAR
    bManual     : BOOL;
    bPermissive : BOOL;
END_VAR

// mode + safeties
bManual     := bOptifillMachineInManualMode OR bManualMode;
bPermissive := bMCR_PowerOn AND bAirSupplyOK AND bSorterDoorClosed AND bCapperDoorClosed;

IF bManual AND bPermissive THEN
    // ---- SMALL HOPPER ----
    IF bHMI_SmallHopperStart THEN bStartSmallHopperMotor := TRUE; END_IF;
    IF bHMI_SmallHopperStop  THEN bStartSmallHopperMotor := FALSE; END_IF;
    bReverseSmallHopperMotor := bHMI_SmallHopperReverse;

    // ---- LARGE HOPPER ----
    IF bHMI_LargeHopperStart THEN bStartLargeHopperMotor := TRUE; END_IF;
    IF bHMI_LargeHopperStop  THEN bStartLargeHopperMotor := FALSE; END_IF;
    bReverseLargeHopperMotor := bHMI_LargeHopperReverse;
ELSE
    // force safe when not manual or permissives lost
    bStartSmallHopperMotor   := FALSE;
    bReverseSmallHopperMotor := FALSE;
    bStartLargeHopperMotor   := FALSE;
    bReverseLargeHopperMotor := FALSE;
END_IF
PROGRAM PRG_Descrambler_Manual
VAR
    bManual     : BOOL;
    bPermissive : BOOL;
END_VAR

// mode + safeties
bManual     := bOptifillMachineInManualMode OR bManualMode;
bPermissive := bMCR_PowerOn AND bAirSupplyOK AND bSorterDoorClosed AND bCapperDoorClosed;

IF bManual AND bPermissive THEN
    // ---- SMALL DESCRAMBLER ----
    IF bHMI_SmallDescrReverse THEN
        bReverseSmallDescrambler := TRUE;   bStartSmallDescrambler := FALSE;   // REV overrides FWD
    ELSIF bHMI_SmallDescrStart THEN
        bStartSmallDescrambler   := TRUE;   bReverseSmallDescrambler := FALSE;
    ELSIF bHMI_SmallDescrStop THEN
        bStartSmallDescrambler   := FALSE;  bReverseSmallDescrambler := FALSE;
    END_IF;

    // ---- LARGE DESCRAMBLER ----
    IF bHMI_LargeDescrReverse THEN
        bReverseLargeDescrambler := TRUE;   bStartLargeDescrambler := FALSE;
    ELSIF bHMI_LargeDescrStart THEN
        bStartLargeDescrambler   := TRUE;   bReverseLargeDescrambler := FALSE;
    ELSIF bHMI_LargeDescrStop THEN
        bStartLargeDescrambler   := FALSE;  bReverseLargeDescrambler := FALSE;
    END_IF;
ELSE
    // force safe when not manual or permissives lost
    bStartSmallDescrambler    := FALSE;
    bReverseSmallDescrambler  := FALSE;
    bStartLargeDescrambler    := FALSE;
    bReverseLargeDescrambler  := FALSE;
END_IF

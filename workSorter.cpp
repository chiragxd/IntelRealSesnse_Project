(*
Bottle SORTER – Auto + Manual
- Uses existing CurrentGVL variables (no new globals).
- Auto: picks a lane (simple rule: PASS -> round-robin lanes 1..6, REJECT -> lane 7),
        indexes servo to lane, handshakes with Lift routine.
- Manual: Home / index-to-lane / jog / open-close gate.
*)

VAR
    // ===== Internal State / Helpers =====
    iSorterStep         : INT := 0;           // state machine
    iLaneCmd            : INT := 0;           // 0..7 commanded lane (3-bit)
    iLaneCmdPrev        : INT := -1;          // detect command change
    iNextGoodLane       : INT := 1;           // simple round-robin 1..6
    bCmdBusy            : BOOL;               // start pulse stretching
    tCmdPulse           : TON;                // CSTR/PWRT pulse
    tInPosTmo           : TON;                // timeout reaching lane
    tGatePulse          : TON;                // optional pulse for gate
    bFaultSorter        : BOOL;               // local fault flag

    // convenience aliases (readability)
    bAuto    : BOOL := bOptifillMachineInAutoMode;      // B33[3].4
    bManual  : BOOL := bOptifillMachineInManualMode;    // B33[3].2
END_VAR


// ---------------------
// Manual controls (HMI)
// ---------------------
IF bManual THEN
    // Servo HOME
    bHomeBottleSorter := bManualHomeSorter OR bSorterManualHomeServo;

    // Manual lane index (N11[180].0..6 set one at a time)
    IF    bManualIndexToLane1 THEN iLaneCmd := 1
    ELSIF bManualIndexToLane2 THEN iLaneCmd := 2
    ELSIF bManualIndexToLane3 THEN iLaneCmd := 3
    ELSIF bManualIndexToLane4 THEN iLaneCmd := 4
    ELSIF bManualIndexToLane5 THEN iLaneCmd := 5
    ELSIF bManualIndexToLane6 THEN iLaneCmd := 6
    ELSIF bManualIndexToLane7 THEN iLaneCmd := 7
    END_IF;

    // Open/Close gate (manual jog style)
    bOpenSorterGate  := bManualOpenSorterGate;
    bCloseSorterGate := bManualCloseSorterGate;

    // Manual jog (optional): set JISE=0 (index mode) for clean behavior when indexing
    bSorterServoJISE := FALSE;
    bSorterServoOperationMode := FALSE;         // Normal (0)
END_IF;


// ---------------------------
// Encode lane -> 3 command bits
// ---------------------------
bSorterServoPositionCmdBit1 := (iLaneCmd AND 1) <> 0;   // LSB
bSorterServoPositionCmdBit2 := (iLaneCmd AND 2) <> 0;
bSorterServoPositionCmdBit3 := (iLaneCmd AND 4) <> 0;

// Pulse “Start / Position Write” (use existing helper bit)
IF iLaneCmd <> iLaneCmdPrev THEN
    // only when drive is OK & not moving & doors/safety closed
    IF bSorterServoReady AND NOT bSorterServoAlarm AND NOT bSorterServoMoving
       AND bSorterDoorClosed AND bCapperDoorClosed AND bControlPowerAndAirOn THEN
        bSorterServoOperationMode := FALSE;     // Normal mode
        bSorterServoJISE := FALSE;              // Jog-to-index (not inching)
        bSorterServoPositionCmdPulse := TRUE;   // pulse start
        tCmdPulse(IN := TRUE, PT := T#150ms);
        bCmdBusy := TRUE;
    END_IF
END_IF;

IF bCmdBusy THEN
    IF tCmdPulse.Q THEN
        bSorterServoPositionCmdPulse := FALSE;
        tCmdPulse(IN := FALSE);
        bCmdBusy := FALSE;
    END_IF;
END_IF;

iLaneCmdPrev := iLaneCmd;


// ------------------------
// AUTO – lane select + move
// ------------------------
IF bAuto AND bAutoSequenceEnable THEN

    CASE iSorterStep OF

        0:  // Idle – wait for bottle at lift and capper complete cue from capper logic
            bFaultSorter := FALSE;
            tInPosTmo(IN := FALSE);
            // very light debounce/guard: only proceed if servo OK & safeties OK
            IF bBottleAtLift AND bSorterServoReady AND NOT bSorterServoAlarm
               AND bSorterDoorClosed AND bCapperDoorClosed AND bControlPowerAndAirOn THEN

                // Pick lane:
                //   - Reject (empty/full) -> lane 7
                //   - Pass -> next good lane 1..6 (round-robin)
                IF (iBottleStatus = 2) OR (iBottleStatus = 3) THEN
                    iLaneCmd := 7;  // reject lane
                ELSE
                    iLaneCmd := iNextGoodLane;
                    iNextGoodLane := iNextGoodLane + 1;
                    IF iNextGoodLane > 6 THEN iNextGoodLane := 1 END_IF;
                END_IF;

                // Start timeout waiting for in-position
                tInPosTmo(IN := TRUE, PT := T#6s);
                iSorterStep := 10;
            END_IF;

        10: // Wait sorter lane reach position (servo + prox)
            // Both PLC position-ready and local lane sensor
            IF bSorterServoInPosition AND bBottleSorterLaneInPosition THEN
                tInPosTmo(IN := FALSE);
                iSorterStep := 20;
            ELSIF tInPosTmo.Q THEN
                bFaultSorter := TRUE;           // could augment with HMI message
                iSorterStep := 900;             // fault/end
            END_IF;

        20: // Lane is ready – hand off to Lift sequence
            // Nothing else to do here: Lift PRG will raise/transfer/drop.
            iSorterStep := 0;

        900: // Fault: stop auto until reset
            // Hold position; operator can jog manually
            ; // no-op

    END_CASE
ELSE
    // Auto disabled -> ensure timers off
    tInPosTmo(IN := FALSE);
END_IF;

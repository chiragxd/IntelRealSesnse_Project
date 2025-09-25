// ---------------------------
// Additional helper declarations (place inside the same PROGRAM's VAR section)
// ---------------------------
VAR
    // Edge detectors (make sure TwinCAT library with R_TRIG is available)
    rTrigReset         : R_TRIG;   // for ResetRequest rising edge
    rTrigHomeReq       : R_TRIG;   // for explicit Home requests (if used)
    rTrigFaultReset    : R_TRIG;   // for FaultResetRequest rising edge
    rTrigMotionReq     : R_TRIG;   // pulses when a motion command becomes active
    motionCmdActive    : BOOL;     // helper bool = (MotionSeqRegister <> 0)
    moveExecutePulse   : BOOL;     // one-shot to feed fbMoveAbs.Execute
    fbResetExecute     : BOOL;     // one-shot for fbReset execute
    fbHomeExecute      : BOOL;     // one-shot for fbHome execute
    fbMoveExecute      : BOOL;     // passed to fbMoveAbs.Execute
    CabinetIndex       : INT := 0; // chosen cabinet (1..5) from GoToCabinetAuto bits

    // small watchdog timers for solenoids (prevents indefinitely holding outputs)
    tGripperCloseWatch : TON;
    tGripperOpenWatch  : TON;

    // small safety latching so ServoPowerEnable doesn't flap
    ServoPowerLatched  : BOOL := FALSE;

END_VAR


// ---------------------------
// Derive CabinetIndex from ladder-style bits GoToCabinetAuto[1..5]
// (Equivalent to ladder rungs that select N137 := 2..6)
// ---------------------------
CabinetIndex := 0;
FOR i := 1 TO 5 DO
    IF GoToCabinetAuto[i] THEN
        CabinetIndex := i; // last set wins (ladder would effectively pick the active one)
    END_IF;
END_FOR


// ---------------------------
// GLOBAL ABORT / PERMISSIVES (ladder top-level inhibit)
// If any of these active, stop motion & clear state like ladder CLR.
// ---------------------------
IF bMachineResetEnable OR FaultResetRequest OR ServoErrorInput OR NOT bAirSupplyOK THEN
    // Stop any active move FBs by removing Execute
    fbMoveExecute := FALSE;
    fbMoveAbs(Axis := AxisRef, Execute := FALSE);
    // Clear motion registers and flags (ladder CLR)
    MotionSeqRegister := 0;
    MotionGoingTo := 0;
    MotionAtRegister := 0;
    MotionSeqActive := FALSE;
    MotionSeqInProgress := FALSE;
    MotionSeqComplete := FALSE;
    MotionDataTransferred := FALSE;

    // Gripper sequence clears
    CloseGripperCmd := FALSE;
    OpenGripperCmd := FALSE;
    CloseGripperActive := FALSE;
    OpenGripperActive := FALSE;
    CloseGripperComplete := FALSE;
    OpenGripperComplete := FALSE;

    // Reset move execute pulse / rtrigs
    motionCmdActive := FALSE;
    rTrigMotionReq(CLK := motionCmdActive);
    rTrigReset(CLK := ResetRequest);
    rTrigFaultReset(CLK := FaultResetRequest);

    // go to safe idle step
    Step := 0;
END_IF


// ---------------------------
// 1) AUTO FEED ENABLE / SERVO START (ladder ~rungs 0..4)
// - MotionSeqActive = B133[1].0
// - tFeedEnableDelay = feed enable TON
// ---------------------------
IF bOptifillMachineInAutoMode AND NOT bOptifillMachineInManualMode THEN
    MotionSeqActive := TRUE; // ladder bit set
ELSE
    MotionSeqActive := FALSE;
END_IF;

// feed enable TON
tFeedEnableDelay(IN := MotionSeqActive, PT := T#250MS);

IF tFeedEnableDelay.Q THEN
    // latch servo power so it doesn't flap; clear on explicit reset/fault (handled above)
    ServoPowerLatched := TRUE;
    ServoPowerEnable := TRUE; // actual output to MC_Power
    MotionSeqInProgress := TRUE; // B133[1].2
ELSE
    // don't disable immediately if latched, let latching handle it
    IF NOT ServoPowerLatched THEN
        ServoPowerEnable := FALSE;
        MotionSeqInProgress := FALSE;
    END_IF;
END_IF;

// Apply MC_Power (call FB every scan with desired enable)
fbPower(Axis := AxisRef, Enable := ServoPowerEnable);
ServoReady := fbPower.Status;
ServoFault := fbPower.Error;


// ---------------------------
// 2) RESET / HOMING (edge triggered)
// - Use R_TRIG so Execute is pulsed only once on rising edges
// - Ladder had timers allowing reset to clear — we use ResetRequest and FaultResetRequest
// ---------------------------
// ResetRequest rising edge => execute fbReset once
rTrigReset(CLK := ResetRequest);
IF rTrigReset.Q THEN
    fbResetExecute := TRUE;
ELSE
    fbResetExecute := FALSE;
END_IF;

IF fbResetExecute THEN
    fbReset(Axis := AxisRef, Execute := TRUE);
ELSE
    fbReset(Axis := AxisRef, Execute := FALSE);
END_IF;

// After reset FB indicates Done we issue a home (also pulse)
IF fbReset.Done THEN
    // pulse home execute once
    rTrigHomeReq(CLK := TRUE); // we will use immediately and then reset (simple pulse)
    // simpler approach: set fbHomeExecute true for a single scan
    fbHomeExecute := TRUE;
END_IF;

// Execute home as one-shot
IF fbHomeExecute THEN
    fbHome(Axis := AxisRef, Execute := TRUE);
    // clear the one-shot next scan
    fbHomeExecute := FALSE;
ELSE
    fbHome(Axis := AxisRef, Execute := FALSE);
END_IF;

// Monitor HOME Done
IF fbHome.Done THEN
    ServoHomed := TRUE;
    MotionSeqComplete := TRUE; // B133[1].15 indicates reset/home complete
    MotionAtRegister := 15;    // optional HOME index
END_IF;


// ---------------------------
// 3) GRIPPER Sequences with Watchdogs (edge-safe)
// - Ensure air supply allowed before energizing solenoids
// - Use TON to avoid holding solenoid forever if no sensor feedback
// ---------------------------
// Close request
IF CloseGripperCmd AND NOT CloseGripperActive AND NOT CloseGripperComplete THEN
    IF bAirSupplyOK THEN
        CloseGripperActive := TRUE;
        GripperCloseOutput := TRUE;
        GripperOpenOutput := FALSE;
        tGripperCloseWatch(IN := TRUE, PT := T#2S); // 2s watchdog on close
    END_IF;
END_IF;

// Complete close when feedback true or watchdog expired (safety)
IF CloseGripperActive THEN
    IF bBottleTransferGripperClosed OR GripperIsClosed THEN
        CloseGripperActive := FALSE;
        CloseGripperComplete := TRUE;
        GripperCloseOutput := FALSE;
        tGripperCloseWatch(IN := FALSE);
    ELSIF tGripperCloseWatch.Q THEN
        // watchdog expired — mark fault (do not leave solenoid stuck)
        CloseGripperActive := FALSE;
        GripperCloseOutput := FALSE;
        // set an internal error/flag for operator to act
        ServoFault := TRUE;
        tGripperCloseWatch(IN := FALSE);
    END_IF;
END_IF;

// Open request
IF OpenGripperCmd AND NOT OpenGripperActive AND NOT OpenGripperComplete THEN
    IF bAirSupplyOK THEN
        OpenGripperActive := TRUE;
        GripperOpenOutput := TRUE;
        GripperCloseOutput := FALSE;
        tGripperOpenWatch(IN := TRUE, PT := T#2S);
    END_IF;
END_IF;

IF OpenGripperActive THEN
    IF bBottleTransferGripperOpen OR GripperIsOpen THEN
        OpenGripperActive := FALSE;
        OpenGripperComplete := TRUE;
        GripperOpenOutput := FALSE;
        tGripperOpenWatch(IN := FALSE);
    ELSIF tGripperOpenWatch.Q THEN
        // watchdog expired: mark fault and clear output
        OpenGripperActive := FALSE;
        GripperOpenOutput := FALSE;
        ServoFault := TRUE;
        tGripperOpenWatch(IN := FALSE);
    END_IF;
END_IF;


// ---------------------------
// 4) AUTO STATE MACHINE (decides MotionSeqRegister & gripper requests)
// - Exactly follows ladder step mapping and uses CabinetIndex computed earlier
// - Keeps same Step numbering as your routine
// ---------------------------
CASE Step OF

    // Step 0: Idle -> go to Labeler when cycle start and permissives ok
    0:
        IF bOptifillMachineInAutoMode AND bCycleStart AND ServoReadyInput AND ServoHomed AND bAirSupplyOK THEN
            IF GripperIsOpen THEN
                MotionSeqRegister := 1; // labeler
                TargetPosition := PosLabeler;
                TargetName := 'Labeler';
                MotionDataTransferred := FALSE;
                Step := 10;
            ELSE
                OpenGripperCmd := TRUE; // open first, ladder did this on idle
            END_IF;
        END_IF;

    // Step 10: At Labeler -> ensure gripper open before checking bottle
    10:
        IF MotionAtRegister = 1 THEN
            IF NOT GripperIsOpen THEN
                OpenGripperCmd := TRUE;
            ELSE
                Step := 20;
            END_IF;
        END_IF;

    // Step 20: Decide routing
    20:
        IF NOT GripperIsClosed THEN
            CloseGripperCmd := TRUE; // must close to pick bottle
        ELSE
            CASE BottleStatus OF
                1: // Passed -> route to selected cabinet
                    IF (CabinetIndex >= 1) AND (CabinetIndex <= 5) THEN
                        MotionSeqRegister := 1 + CabinetIndex; // maps: 2..6
                        TargetPosition := PosCabinet[CabinetIndex];
                        TargetName := CONCAT('Cabinet ', INT_TO_STRING(CabinetIndex));
                        Step := 30;
                    ELSE
                        // no cabinet selected - stay wait
                        Step := 10;
                    END_IF;

                2, 3: // Reject -> Capper
                    MotionSeqRegister := 14;
                    TargetPosition := PosCapper;
                    TargetName := 'Capper';
                    Step := 100;

                ELSE
                    Step := 10; // keep waiting
            END_CASE;
        END_IF;

    // Step 30: At Cabinet -> wait for fill complete handshake
    30:
        IF (MotionAtRegister >= 2) AND (MotionAtRegister <= 6) THEN
            IF FillComplete THEN
                MotionSeqRegister := 13; // go to camera
                TargetPosition := PosCamera;
                TargetName := 'Camera';
                Step := 40;
            END_IF;
        END_IF;

    // Step 40: At Camera -> wait for PPS or timeout
    40:
        IF MotionAtRegister = 13 THEN
            IF CameraTriggerOK THEN
                Step := 50;
            ELSE
                tCamDelay(IN := TRUE, PT := T#500MS);
                IF tCamDelay.Q THEN
                    Step := 50;
                    tCamDelay(IN := FALSE);
                END_IF;
            END_IF;
        END_IF;

    // Step 50: Command go to Capper
    50:
        MotionSeqRegister := 14;
        TargetPosition := PosCapper;
        TargetName := 'Capper';
        Step := 60;

    // Step 60: Wait at Capper -> handshake and open gripper
    60:
        IF MotionAtRegister = 14 THEN
            IF HandoffComplete THEN
                OpenGripperCmd := TRUE;
                Step := 70;
            END_IF;
        END_IF;

    // Step 70: Wait for gripper open done, then return to labeler
    70:
        IF OpenGripperComplete THEN
            MotionSeqRegister := 1;
            TargetPosition := PosLabeler;
            TargetName := 'Labeler';
            Step := 0;
            BottleStatus := 0;
            OpenGripperComplete := FALSE; // emulate ladder CLR
        END_IF;

    // Step 100: Reject direct capper path
    100:
        IF MotionAtRegister = 14 THEN
            IF HandoffComplete THEN
                OpenGripperCmd := TRUE;
                IF OpenGripperComplete THEN
                    MotionSeqRegister := 1;
                    TargetPosition := PosLabeler;
                    TargetName := 'Labeler';
                    Step := 0;
                    BottleStatus := 0;
                    OpenGripperComplete := FALSE;
                END_IF;
            END_IF;
        END_IF;

    ELSE
        Step := 0;
END_CASE;


// ---------------------------
// 5) MOTION EXECUTION LAYER (edge-safe, only issues Execute once per command)
// - Issues fbMoveAbs.Execute as a one-shot when MotionSeqRegister transitions to non-zero
// - Uses R_TRIG on motionCmdActive to create one-shot
// ---------------------------
motionCmdActive := (MotionSeqRegister <> 0);
rTrigMotionReq(CLK := motionCmdActive);
moveExecutePulse := rTrigMotionReq.Q; // one-shot true when motion command first becomes active

// Provide fbMoveExecute based on the one-shot and permissives
IF moveExecutePulse AND ServoReadyInput AND ServoHomed AND (NOT ServoErrorInput) AND bAirSupplyOK THEN
    fbMoveExecute := TRUE; // one-shot execute to kick move FB
ELSE
    fbMoveExecute := FALSE;
END_IF;

// Execute appropriate motion command (move only when fbMoveExecute pulses)
IF fbMoveExecute THEN
    CASE MotionSeqRegister OF
        1:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosLabeler, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        2:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[1], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        3:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[2], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        4:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[3], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        5:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[4], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        6:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[5], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        13: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCamera, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        14: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCapper, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        15: fbHome(Axis := AxisRef, Execute := TRUE); // optional: homing mapping
        ELSE
            // unknown command: do nothing
    END_CASE;

    // reflect the going-to register like ladder N137[1]
    MotionGoingTo := MotionSeqRegister;
    MotionDataTransferred := TRUE;
END_IF;

// Note: fbMoveAbs will set Done when the axis reaches the commanded pos.
// Handle completion (update N137[2] and clear command)
IF fbMoveAbs.Done THEN
    // Put the 'At' register equal to what we were going to
    MotionAtRegister := MotionGoingTo; // N137[2] := N137[1]
    // Clear command register (ladder CLR)
    MotionSeqRegister := 0;
    MotionDataTransferred := FALSE;
    // clear fbMoveAbs execute explicitly (safety)
    fbMoveAbs(Axis := AxisRef, Execute := FALSE);
END_IF;

// Handle motion error
IF fbMoveAbs.Error THEN
    ServoFault := TRUE;
    // Stop move execute
    fbMoveAbs(Axis := AxisRef, Execute := FALSE);
    MotionSeqRegister := 0;
    MotionSeqComplete := FALSE;
    // set safe step
    Step := 0;
END_IF;


// ---------------------------
// 6) Clear/Pulse bits that ladder cleared (emulated CLR)
// ---------------------------
IF MotionSeqComplete THEN
    MotionSeqComplete := FALSE; // one-scan pulse
END_IF;

IF OpenGripperComplete THEN
    OpenGripperComplete := FALSE;
END_IF;

IF CloseGripperComplete THEN
    CloseGripperComplete := FALSE;
END_IF;


// ---------------------------
// 7) Fault reset via HMI (if operator presses)
// - Use R_TRIG to call fbReset as one-shot on rising edge of FaultResetRequest
// ---------------------------
rTrigFaultReset(CLK := FaultResetRequest);
IF rTrigFaultReset.Q THEN
    fbReset(Axis := AxisRef, Execute := TRUE);
ELSE
    fbReset(Axis := AxisRef, Execute := FALSE);
END_IF;

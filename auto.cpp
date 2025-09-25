// ---------------------------
// 1. FEED ENABLE / SERVO START
// ---------------------------
IF AutoMode AND NOT ManualMode THEN
    MotionSeqActive := TRUE;         // B133[1].0
ELSE
    MotionSeqActive := FALSE;
END_IF;

tFeedEnableDelay(IN := MotionSeqActive, PT := T#250MS);
IF tFeedEnableDelay.Q THEN
    ServoPowerEnable := TRUE;        // Equivalent to feed enable
    MotionSeqInProgress := TRUE;     // B133[1].2
END_IF;

// Servo Power Management
fbPower(Axis := AxisRef, Enable := ServoPowerEnable);
ServoReady := fbPower.Status;
ServoFault := fbPower.Error;


// ---------------------------
// 2. RESET / HOMING SEQUENCE
// ---------------------------
IF ResetRequest THEN
    fbReset(Axis := AxisRef, Execute := TRUE);
    ServoResetOutput := TRUE;
END_IF;

IF fbReset.Done THEN
    ServoFault := FALSE;
    ServoResetOutput := FALSE;

    // Start homing
    fbHome(Axis := AxisRef, Execute := TRUE);
END_IF;

IF fbHome.Done THEN
    ServoHomed := TRUE;
    MotionSeqComplete := TRUE;       // B133[1].15
END_IF;


// ---------------------------
// 3. GRIPPER SEQUENCES (AUTO)
// ---------------------------
// Close Gripper
IF CloseGripperCmd THEN
    CloseGripperActive := TRUE;
    GripperCloseOutput := TRUE;
    GripperOpenOutput := FALSE;
END_IF;

IF CloseGripperActive AND GripperIsClosed THEN
    CloseGripperComplete := TRUE;
    CloseGripperActive := FALSE;
END_IF;

// Open Gripper
IF OpenGripperCmd THEN
    OpenGripperActive := TRUE;
    GripperOpenOutput := TRUE;
    GripperCloseOutput := FALSE;
END_IF;

IF OpenGripperActive AND GripperIsOpen THEN
    OpenGripperComplete := TRUE;
    OpenGripperActive := FALSE;
END_IF;


// ---------------------------
// 4. AUTO SEQUENCE STATE MACHINE
// ---------------------------
CASE Step OF
    0: // Idle / start at Labeler
        IF CycleStart THEN
            MotionSeqRegister := 1;              // Labeler
            TargetPosition := PosLabeler;
            TargetName := 'Labeler';
            Step := 10;
        END_IF;

    10: // Wait at Labeler
        IF MotionAtRegister = 1 THEN
            // Gripper must be open at idle
            OpenGripperCmd := TRUE;
            Step := 20;
        END_IF;

    20: // Decide routing based on bottle status
        CASE BottleStatus OF
            1: // Passed -> go to Cabinet (example uses 1st)
                MotionSeqRegister := 2;          // Cabinet 1
                TargetPosition := PosCabinet[1];
                TargetName := 'Cabinet 1';
                Step := 30;

            2: // RejectEmptyBottle -> Capper
                MotionSeqRegister := 14;
                TargetPosition := PosCapper;
                TargetName := 'Capper';
                Step := 100;

            3: // RejectFullBottle -> Capper
                MotionSeqRegister := 14;
                TargetPosition := PosCapper;
                TargetName := 'Capper';
                Step := 100;

            ELSE
                // Stay at labeler until status available
                Step := 10;
        END_CASE;

    30: // At Cabinet -> wait for fill complete
        IF MotionAtRegister >= 2 AND MotionAtRegister <= 6 THEN
            IF FillComplete THEN
                MotionSeqRegister := 13;         // Camera
                TargetPosition := PosCamera;
                TargetName := 'Camera';
                Step := 40;
            END_IF;
        END_IF;

    40: // At Camera -> wait for PPS or timer
        IF MotionAtRegister = 13 THEN
            IF CameraTriggerOK THEN
                Step := 50;
            ELSE
                tCamDelay(IN := TRUE, PT := T#500MS);
                IF tCamDelay.Q THEN
                    Step := 50;
                END_IF;
            END_IF;
        END_IF;

    50: // Go to Capper
        MotionSeqRegister := 14;
        TargetPosition := PosCapper;
        TargetName := 'Capper';
        Step := 60;

    60: // Wait at Capper -> wait for handoff complete
        IF MotionAtRegister = 14 THEN
            IF HandoffComplete THEN
                OpenGripperCmd := TRUE;          // release bottle
                Step := 70;
            END_IF;
        END_IF;

    70: // Return to Labeler after capper
        IF OpenGripperComplete THEN
            MotionSeqRegister := 1;
            TargetPosition := PosLabeler;
            TargetName := 'Labeler';
            Step := 0;                           // Cycle complete
        END_IF;

    100: // Direct Capper path (Reject cases)
        IF MotionAtRegister = 14 THEN
            IF HandoffComplete THEN
                OpenGripperCmd := TRUE;
                MotionSeqRegister := 1;          // Back to Labeler
                Step := 0;
            END_IF;
        END_IF;

    ELSE
        Step := 0;
END_CASE;


// ---------------------------
// 5. MOTION EXECUTION (fbMoveAbs)
// ---------------------------
IF MotionSeqRegister <> 0 AND ServoReady THEN
    CASE MotionSeqRegister OF
        1: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosLabeler, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        2: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[1], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        3: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[2], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        4: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[3], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        5: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[4], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        6: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[5], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        13: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCamera, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
        14: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCapper, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
    END_CASE;

    // Update registers
    MotionGoingTo := MotionSeqRegister;
END_IF;

IF fbMoveAbs.Done THEN
    MotionAtRegister := MotionSeqRegister;
    MotionSeqRegister := 0;           // Clear command
END_IF;

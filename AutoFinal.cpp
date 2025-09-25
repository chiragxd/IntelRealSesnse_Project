// ======================================================================
// AUTO SEQUENCE (Structured Text) - uses your declared variables
// ----------------------------------------------------------------------
// Notes:
// - MotionSeqRegister  -> N137[0] (command register)
// - MotionGoingTo      -> N137[1] ("Going To" register shown on HMI)
// - MotionAtRegister   -> N137[2] ("At" register shown on HMI)
// - MotionSeqActive    -> B133[1].0
// - MotionSeqInProgress-> B133[1].2
// - MotionSeqComplete  -> B133[1].15
// - MotionDataTransferred -> B133[1].3
// - Close/Open gripper flags -> B133[2]/B133[3] family
// - This block intentionally keeps state machine (Step) separate from motion execution
// ======================================================================


// ---------------------------
// GLOBAL ABORT / PERMISSIVES
// (Like ladder: inhibit motion when faults, reset request, or no air)
// ---------------------------
IF ResetRequest OR FaultResetRequest OR ServoErrorInput OR NOT AirSupplyOK THEN
    // Abort motion and clear active requests (ladder CLR behavior)
    fbMoveAbs.Execute := FALSE;             // stop any active move command
    MotionSeqRegister := 0;                 // clear command register (N137[0])
    MotionGoingTo := 0;                     // clear "going to" register (N137[1])
    MotionAtRegister := 0;                  // clear "at" register (N137[2])
    MotionSeqActive := FALSE;               // B133[1].0
    MotionSeqInProgress := FALSE;           // B133[1].2
    MotionSeqComplete := FALSE;             // B133[1].15
    MotionDataTransferred := FALSE;         // B133[1].3
    // Reset gripper sequence requests too
    CloseGripperCmd := FALSE;
    OpenGripperCmd := FALSE;
    CloseGripperActive := FALSE;
    OpenGripperActive := FALSE;
    CloseGripperComplete := FALSE;
    OpenGripperComplete := FALSE;
    // return to idle step
    Step := 0;
END_IF;


// ---------------------------
// 1) FEED ENABLE / SERVO START (ladder ~rungs 0..4)
// - Sets MotionSeqActive (B133[1].0) when AutoMode request is present
// - Starts tFeedEnableDelay and enables servo when timer done
// ---------------------------
IF AutoMode AND NOT ManualMode THEN
    MotionSeqActive := TRUE;   // B133[1].0 (motion sequence active)
ELSE
    MotionSeqActive := FALSE;
END_IF;

// start feed enable timer (ladder used TON T134[1] / 250ms)
tFeedEnableDelay(IN := MotionSeqActive, PT := T#250MS);
IF tFeedEnableDelay.Q THEN
    ServoPowerEnable := TRUE;          // equivalent to the ladder feed enable bit
    MotionSeqInProgress := TRUE;       // B133[1].2
ELSE
    // until timer done we don't enable feed
    ServoPowerEnable := FALSE;
    MotionSeqInProgress := FALSE;
END_IF;

// Drive power / status mirror (keep these reads in place)
fbPower(Axis := AxisRef, Enable := ServoPowerEnable);
ServoReady := fbPower.Status;          // map to drive ready (MC.DN)
ServoFault := fbPower.Error;           // map to drive error (MC.ER)


// ---------------------------
// 2) RESET & HOMING SEQUENCE (ladder ~rungs 21..36)
// - Ladder clears flags, opens gripper, clears servo faults, issues home
// ---------------------------
IF ResetRequest THEN
    // Start reset logic: clear MC faults (MC_Reset) and prepare home request
    fbReset(Axis := AxisRef, Execute := TRUE);
END_IF;

// After fbReset.Done, clear local fault flag and request home
IF fbReset.Done THEN
    ServoFault := FALSE;
    // small delay or immediate home as ladder had timers (we'll kick home)
    fbHome(Axis := AxisRef, Execute := TRUE);
END_IF;

// Monitor home completion
IF fbHome.Done THEN
    ServoHomed := TRUE;
    MotionSeqComplete := TRUE;   // B133[1].15, indicate reset/home complete
    // update "at" register to home index if ladder expected a special code (optional)
    MotionAtRegister := 15;      // using 15 for HOME as in ladder mapping
END_IF;


// ---------------------------
// 3) GRIPPER SEQUENCES (auto open/close) (ladder ~rungs 7..16, 12..15)
// - Issue Open/Close commands (solenoid outputs) and wait for feedback sensors
// ---------------------------

// Close gripper sequence request (CloseGripperCmd is request bit e.g. B33[11].0)
IF CloseGripperCmd AND NOT CloseGripperActive AND NOT CloseGripperComplete THEN
    // start close sequence
    CloseGripperActive := TRUE;           // B133[2].0
    GripperCloseOutput := TRUE;           // physical output to close solenoid
    GripperOpenOutput := FALSE;
    // Start close timer if ladder used a short TON for solenoid response
    tGripperCloseDelay(IN := TRUE, PT := T#200MS);
END_IF;

// Complete close when feedback indicates closed
IF CloseGripperActive THEN
    IF bGripperClosedFeedback OR GripperIsClosed THEN
        CloseGripperActive := FALSE;
        CloseGripperComplete := TRUE;     // B133[2].15
        // clear solenoid (optionally hold if needed)
        GripperCloseOutput := FALSE;
        tGripperCloseDelay(IN := FALSE);
    END_IF;
END_IF;

// Reset close-complete after consumer reads it (ladder often CLR's these)
IF CloseGripperComplete THEN
    // keep flag true long enough for logic; clear when next action desires it
    // (Here we leave it set until explicitly cleared by higher logic or next cycle)
END_IF;


// Open gripper sequence request
IF OpenGripperCmd AND NOT OpenGripperActive AND NOT OpenGripperComplete THEN
    OpenGripperActive := TRUE;            // B133[3].0
    GripperOpenOutput := TRUE;            // physical output to open solenoid
    GripperCloseOutput := FALSE;
    tGripperOpenDelay(IN := TRUE, PT := T#200MS);
END_IF;

IF OpenGripperActive THEN
    IF bGripperOpenFeedback OR GripperIsOpen THEN
        OpenGripperActive := FALSE;
        OpenGripperComplete := TRUE;      // B133[3].15
        GripperOpenOutput := FALSE;
        tGripperOpenDelay(IN := FALSE);
    END_IF;
END_IF;


// ---------------------------
// 4) AUTO STATE MACHINE (mirrors ladder flow)
//    This block only decides what next position to request and sets
//    MotionSeqRegister (N137[0]), TargetPosition, and gripper requests.
//    It does NOT run the low-level fbMoveAbs directly here (motion execution layer does).
// ---------------------------
CASE Step OF

    // -----------------------
    // Step 0 - Idle / go to Labeler on cycle start
    // Ladder intent: Default idle pos = Labeler. (PDF rungs ~42..50)
    // -----------------------
    0:
        IF AutoMode AND CycleStart AND ServoReady AND ServoHomed AND AirSupplyOK THEN
            // require gripper open before moving to Labeler (ladder enforces this)
            IF GripperIsOpen THEN
                MotionSeqRegister := 1;            // N137[0] = 1 (Labeler)
                TargetPosition := PosLabeler;
                TargetName := 'Labeler';
                MotionDataTransferred := FALSE;    // prepare new transfer
                Step := 10;
            ELSE
                // request gripper open first (ladder had open-on-reset)
                OpenGripperCmd := TRUE;
            END_IF;
        END_IF;

    // -----------------------
    // Step 10 - Wait At Labeler
    // Ladder: Wait until MotionAtRegister == 1 (N137[2] == 1)
    // -----------------------
    10:
        IF MotionAtRegister = 1 THEN
            // ensure gripper is open (Labeler idle requirement)
            IF NOT GripperIsOpen THEN
                OpenGripperCmd := TRUE;
            ELSE
                // ready to inspect bottle status / route
                Step := 20;
            END_IF;
        END_IF;

    // -----------------------
    // Step 20 - Decide routing based on BottleStatus
    // Ladder: If Passed -> goto cabinet; Reject -> goto capper
    // -----------------------
    20:
        // Before leaving Labeler the gripper must be closed (ladder logic)
        IF NOT GripperIsClosed THEN
            CloseGripperCmd := TRUE;
            // remain in step until closed
        ELSE
            CASE BottleStatus OF
                1:  // Passed -> pick a cabinet (implement simplest: cabinet 1)
                    MotionSeqRegister := 2;            // 2 => Cabinet 1 (N137 mapping)
                    TargetPosition := PosCabinet[1];
                    TargetName := 'Cabinet 1';
                    Step := 30;

                2:  // RejectEmptyBottle -> Capper
                    MotionSeqRegister := 14;           // 14 => Capper
                    TargetPosition := PosCapper;
                    TargetName := 'Capper';
                    Step := 100;

                3:  // RejectFullBottle -> Capper
                    MotionSeqRegister := 14;
                    TargetPosition := PosCapper;
                    TargetName := 'Capper';
                    Step := 100;

                ELSE
                    // No bottle status yet: stay in Labeler wait
                    Step := 10;
            END_CASE;
        END_IF;

    // -----------------------
    // Step 30 - Arrived at Cabinet -> wait for filler handshake
    // Ladder: wait FillComplete from filler (PDF had this)
    // -----------------------
    30:
        // Wait until motion layer sets MotionAtRegister to cabinet index (2..6)
        IF (MotionAtRegister >= 2) AND (MotionAtRegister <= 6) THEN
            // after arrive, wait FillComplete (filler signals)
            IF FillComplete THEN
                MotionSeqRegister := 13;           // Camera
                TargetPosition := PosCamera;
                TargetName := 'Camera';
                Step := 40;
            END_IF;
        END_IF;

    // -----------------------
    // Step 40 - Arrived at Camera -> wait for PPS or timer
    // Ladder: uses PPS or fallback timer
    // -----------------------
    40:
        IF MotionAtRegister = 13 THEN
            IF CameraTriggerOK THEN
                // camera OK, proceed
                Step := 50;
            ELSE
                // fallback timer as ladder used a TON
                tCamDelay(IN := TRUE, PT := T#500MS);
                IF tCamDelay.Q THEN
                    Step := 50;
                    tCamDelay(IN := FALSE);
                END_IF;
            END_IF;
        END_IF;

    // -----------------------
    // Step 50 - Command move to Capper
    // -----------------------
    50:
        MotionSeqRegister := 14;   // capper
        TargetPosition := PosCapper;
        TargetName := 'Capper';
        Step := 60;

    // -----------------------
    // Step 60 - Wait at Capper -> handshake for handoff complete
    // -----------------------
    60:
        IF MotionAtRegister = 14 THEN
            // wait for capper to confirm handoff
            IF HandoffComplete THEN
                // release bottle
                OpenGripperCmd := TRUE;
                Step := 70;
            END_IF;
        END_IF;

    // -----------------------
    // Step 70 - Wait for gripper open complete, then go back to labeler
    // -----------------------
    70:
        IF OpenGripperComplete THEN
            MotionSeqRegister := 1;     // go back to Labeler
            TargetPosition := PosLabeler;
            TargetName := 'Labeler';
            Step := 0;                  // cycle done: back to Idle
            // Clear bottle status after handing off (optional)
            BottleStatus := 0;
            OpenGripperComplete := FALSE; // emulate ladder CLR
        END_IF;

    // -----------------------
    // Step 100 - Direct capper path (reject cases)
    // -----------------------
    100:
        IF MotionAtRegister = 14 THEN
            IF HandoffComplete THEN
                OpenGripperCmd := TRUE;
                IF OpenGripperComplete THEN
                    MotionSeqRegister := 1; // return to Labeler
                    TargetPosition := PosLabeler;
                    TargetName := 'Labeler';
                    Step := 0;
                    BottleStatus := 0;
                    OpenGripperComplete := FALSE;
                END_IF;
            END_IF;
        END_IF;

    ELSE
        Step := 0; // safety fallback
END_CASE;


// ---------------------------
// 5) MOTION EXECUTION LAYER (keeps ladder separation: rungs ~86..104 etc.)
// - This block observes MotionSeqRegister (N137[0]) and calls fbMoveAbs
// - Enforces safety checks before issuing move command (ServoReady, homed, no fault, air OK)
// - Updates MotionGoingTo (N137[1]) and MotionAtRegister (N137[2]) when move finishes
// ---------------------------
IF (MotionSeqRegister <> 0) THEN
    // check all motion permissives (ladder rungs check MC.DN, MSO, etc.)
    IF ServoReadyInput AND ServoHomed AND (NOT ServoErrorInput) AND AirSupplyOK THEN
        // if we are not currently commanding a move (prevent re-trigger each scan)
        IF NOT fbMoveAbs.Execute THEN
            CASE MotionSeqRegister OF
                1:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosLabeler, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                2:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[1], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                3:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[2], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                4:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[3], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                5:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[4], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                6:  fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCabinet[5], Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                13: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCamera, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                14: fbMoveAbs(Axis := AxisRef, Execute := TRUE, Position := PosCapper, Velocity := 100.0, Acceleration := 300.0, Deceleration := 300.0);
                15: fbHome(Axis := AxisRef, Execute := TRUE); // optional mapping for HOME
                ELSE
                    // unknown command: do nothing
            END_CASE;
            // update "going to" register for HMI (N137[1])
            MotionGoingTo := MotionSeqRegister;
            MotionDataTransferred := TRUE;    // emulate ladder data transferred flag
        END_IF;
    ELSE
        // Permissives not satisfied: do not issue move
        // Keep MotionSeqRegister until permissives restored or reset
    END_IF;
END_IF;

// When move completes: update At register and clear command register (mimic ladder CLR)
IF fbMoveAbs.Done THEN
    // Move finished successfully
    MotionAtRegister := MotionGoingTo;  // N137[2] := N137[1]
    // Clear the active command (ladder rungs CLR N137[0])
    MotionSeqRegister := 0;
    MotionDataTransferred := FALSE;     // CLR data transferred (ladder)
    fbMoveAbs.Execute := FALSE;         // reset execute for next command
END_IF;

// Handle move error similar to ladder fault behavior
IF fbMoveAbs.Error THEN
    // Mark servo fault and abort sequence
    ServoFault := TRUE;
    fbMoveAbs.Execute := FALSE;
    MotionSeqRegister := 0;
    // Optionally set a fault register bit (MotionSeqComplete / error)
    MotionSeqComplete := FALSE;
    // return to safe state
    Step := 0;
END_IF;


// ---------------------------
// 6) Clear one-shot / pulse flags the way ladder would CLR them
// ---------------------------
// Emulate ladder CLR for MotionSeqComplete (B133[1].15) — pulse true when fbHome.Done or reset sequence completes
IF MotionSeqComplete THEN
    // leave it true only for one scan (or until HMI reads it as desired)
    MotionSeqComplete := FALSE;
END_IF;

// Emulate clearing open/close complete when consumed or next action begins
// (If you need the HMI to see the bit longer, remove auto-clear)
IF OpenGripperComplete THEN
    // Optionally, keep until consumer clears it. For now clear after one scan.
    OpenGripperComplete := FALSE;
END_IF;
IF CloseGripperComplete THEN
    CloseGripperComplete := FALSE;
END_IF;

// ---------------------------------------------------------------------
// End of Auto Sequence block
// ---------------------------------------------------------------------

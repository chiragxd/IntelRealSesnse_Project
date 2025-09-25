//Enhanced Bottle Transfer Control System - TwinCAT 3.1 Structured Text
//Based on ladder logic analysis and adding missing functionality

//Cabinet Index Selection (Enhanced to prevent multiple selections)
CabinetIndex := 0;
FOR i := 1 TO 5 DO
    IF GoToCabinetAuto[i] AND CabinetIndex = 0 THEN  // Only select first active request
        CabinetIndex := i;
    END_IF
END_FOR

//=============================================================================
// AUTO SEQUENCE CONTROL (Missing from original - based on ladder rungs 1-6)
//=============================================================================

// Auto Mode Motion Sequence Active Control
IF bOptifillMachineInAutoMode AND NOT bOptifillMachineInManualMode AND NOT ServoFault THEN
    // Check if any motion request is active
    IF GoToLabelerAuto OR 
       GoToCabinetAuto[1] OR GoToCabinetAuto[2] OR GoToCabinetAuto[3] OR 
       GoToCabinetAuto[4] OR GoToCabinetAuto[5] OR 
       GoToCameraAuto OR GoToCapperFromLabeler OR 
       GoToCapperFromCab OR GoToCapperFromCamera THEN
        MotionSeqActive := TRUE;   // B133[1].0
    END_IF
ELSE
    MotionSeqActive := FALSE;
    // Clear motion sequence complete when not active
    IF MotionSeqComplete THEN
        MotionSeqComplete := FALSE;
    END_IF
END_IF

// Feed Enable Delay Timer (Rung 2-3)
tFeedEnableDelay(IN := MotionSeqActive, PT := T#250MS);
IF tFeedEnableDelay.Q THEN
    ServoPowerEnable := TRUE;          
    MotionSeqInProgress := TRUE;       // B133[1].2
ELSE
    ServoPowerEnable := FALSE;
    MotionSeqInProgress := FALSE;
END_IF

//=============================================================================
// SERVO DRIVE POWER AND STATUS MANAGEMENT
//=============================================================================

// Drive Power Control
fbPower(Axis := Axis, Enable := ServoPowerEnable OR (bOptifillMachineInManualMode AND ServoOn));
ServoReady := fbPower.Status;          // MC.DN equivalent
ServoFault := fbPower.Error;           // MC.ER equivalent

// Servo Drive Status Monitoring
IF NOT ServoReady AND ServoPowerEnable THEN
    // Servo enabled but not ready condition
    IF NOT (bOptifillMachineInAutoMode AND NOT bCycleStartRequestHmiOneShot) AND
       NOT (bOptifillMachineInManualMode AND NOT bResetFaultRequestHmiOneShot) THEN
        // Set servo enabled but not ready flag for diagnostics
        // This would trigger appropriate alarms in the HMI
    END_IF
END_IF

//=============================================================================
// ENHANCED RESET AND FAULT HANDLING
//=============================================================================

// Comprehensive Global Abort/Reset (Based on ladder rungs 21-36)
IF bMachineResetEnable OR FaultResetRequest OR ServoErrorInput OR 
   NOT bAirSupplyOK OR NOT bControlPowerOn THEN
    
    // Abort all motion immediately
    fbMoveAbs.Execute := FALSE;
    fbHome.Execute := FALSE;
    fbJogFwd.Execute := FALSE;             
    fbJogRev.Execute := FALSE;
    
    // Clear motion registers
    MotionSeqRegister := 0;                 
    MotionGoingTo := 0;                     
    MotionAtRegister := 0;                  
    MotionSeqActive := FALSE;               // B133[1].0
    MotionSeqInProgress := FALSE;           // B133[1].2
    MotionSeqComplete := FALSE;             // B133[1].15
    MotionDataTransferred := FALSE;         // B133[1].3
    
    // Reset gripper sequences
    CloseGripperCmd := FALSE;
    OpenGripperCmd := FALSE;
    CloseGripperActive := FALSE;
    OpenGripperActive := FALSE;
    CloseGripperComplete := FALSE;
    OpenGripperComplete := FALSE;
    
    // Clear outputs
    GripperOpenOutput := FALSE;
    GripperCloseOutput := FALSE;
    
    // Return to idle state
    Step := 0;
    BottleStatus := 0;
    
    // Reset timers
    tFeedEnableDelay(IN := FALSE);
    tServoStartDelay(IN := FALSE);
    tGripperCloseDelay(IN := FALSE);
    tGripperOpenDelay(IN := FALSE);
    tCamDelay(IN := FALSE);
END_IF

// Reset Sequence (Enhanced based on ladder logic)
IF bMachineResetEnable THEN
    // Step 1: Clear faults first
    fbReset(Axis := Axis, Execute := TRUE);
    
    // Step 2: Ensure gripper is open before homing
    IF fbReset.Done AND NOT ServoFault THEN
        IF NOT GripperIsOpen THEN
            OpenGripperCmd := TRUE;
        ELSE
            // Step 3: Home the servo after gripper is confirmed open
            fbHome(Axis := Axis, Execute := TRUE);
        END_IF
    END_IF
    
    // Step 4: Complete reset when homing is done
    IF fbHome.Done THEN
        ServoHomed := TRUE;
        MotionSeqComplete := TRUE;   // B133[1].15
        MotionAtRegister := 15;      // Home position
        bMachineResetEnable := FALSE; // Clear reset request
    END_IF
ELSE
    fbReset(Axis := Axis, Execute := FALSE);
END_IF

//=============================================================================
// MANUAL MODE CONTROL (Enhanced)
//=============================================================================

IF bOptifillMachineInManualMode THEN
    
    // Manual Servo Power Control
    fbPower(Axis := Axis, Enable := ServoOn);
    ServoReady := fbPower.Status;   
    ServoFault := fbPower.Error;  
    
    // Manual Fault Reset
    fbReset(Axis := Axis, Execute := FaultResetRequest);
    IF fbReset.Done THEN
        ServoFault := FALSE;        
    END_IF
    
    // Jog Limit Protection (Based on ladder rungs 81-82)
    // Prevent forward jog if at forward limit
    IF ActualPosition <= -141.0 AND MotionSeqRegister <> 15 THEN
        JogFwdCmd := FALSE; // Inhibit forward jog
    END_IF
    
    // Prevent reverse jog if at reverse limit  
    IF ActualPosition >= 0.0 AND MotionSeqRegister <> 15 THEN
        JogRevCmd := FALSE; // Inhibit reverse jog
    END_IF
    
    // Manual Jog Controls (Enhanced with limits)
    fbJogFwd(
        Axis := Axis,
        Execute := (ServoReady AND ServoHomed AND JogFwdCmd AND bCapperDoorClosed),  
        Velocity := 6.5,                       
        Acceleration := 26.65,
        Deceleration := 26.65
    );
    
    fbJogRev(
        Axis := Axis,
        Execute := (ServoReady AND ServoHomed AND JogRevCmd AND bCapperDoorClosed),
        Velocity := 6.5,                      
        Acceleration := 26.65,
        Deceleration := 26.65
    );
    
    // Manual Position Commands (Enhanced with safety checks)
    IF ServoReady AND ServoHomed AND bCapperDoorClosed THEN
        // Move to Labeler position
        IF GoToLabelerManual AND GripperIsOpen THEN
            fbMoveAbs(
                Axis := Axis,
                Execute := TRUE,
                Position := PosLabeler,
                Velocity := 130.0,
                Acceleration := 533.0,
                Deceleration := 533.0
            );
            TargetPosition := PosLabeler;
            TargetName := 'Labeler';
        END_IF;
        
        // Move to Cabinets 1-5
        FOR i := 1 TO 5 DO
            IF GoToCabinetManual[i] THEN
                fbMoveAbs(
                    Axis := Axis,
                    Execute := TRUE,
                    Position := PosCabinet[i],
                    Velocity := 130.0,
                    Acceleration := 533.0,
                    Deceleration := 533.0
                );
                TargetPosition := PosCabinet[i];
                TargetName := CONCAT('Cabinet ', INT_TO_STRING(i));
            END_IF;
        END_FOR;
        
        // Move to Camera
        IF GoToCameraManual THEN
            fbMoveAbs(
                Axis := Axis,
                Execute := TRUE,
                Position := PosCamera,
                Velocity := 130.0,
                Acceleration := 533.0,
                Deceleration := 533.0
            );
            TargetPosition := PosCamera;
            TargetName := 'Camera';
        END_IF;
        
        // Move to Capper (with additional safety checks)
        IF GoToCapperManual AND bBottleCapperChuckHeadUp AND bBottleLiftGripperOpenAtCapper THEN
            fbMoveAbs(
                Axis := Axis,
                Execute := TRUE,
                Position := PosCapper,
                Velocity := 130.0,
                Acceleration := 533.0,
                Deceleration := 533.0
            );
            TargetPosition := PosCapper;
            TargetName := 'Capper';
        END_IF;
        
        // Home the servo
        IF GoHomeManual THEN
            fbHome(Axis := Axis, Execute := TRUE);
            IF fbHome.Done THEN
                ServoHomed := TRUE;
                TargetPosition := PosHome;
                TargetName := 'Home';
            END_IF;
        END_IF;
    END_IF;
    
    // Manual Gripper Controls
    IF NOT fbMoveAbs.Busy THEN // Only allow gripper operation when not moving
        // Open Gripper
        IF OpenGripperCmd THEN
            GripperOpenOutput := TRUE;
            GripperCloseOutput := FALSE;
        END_IF;
        
        // Close Gripper
        IF CloseGripperCmd THEN
            GripperOpenOutput := FALSE;
            GripperCloseOutput := TRUE;
        END_IF;
    END_IF;
END_IF;

//=============================================================================
// POSITION TEACHING (Enhanced with validation)
//=============================================================================

IF TeachMode AND bOptifillMachineInManualMode AND NOT fbMoveAbs.Busy THEN
    // Save current position as Labeler
    IF SaveLabelerPos THEN
        PosLabeler := ActualPosition;
        SaveLabelerPos := FALSE;
    END_IF;
    
    // Save current position as one of the Cabinets
    FOR i := 1 TO 5 DO
        IF SaveCabinetPos[i] THEN
            PosCabinet[i] := ActualPosition;
            SaveCabinetPos[i] := FALSE;
        END_IF;
    END_FOR;
    
    // Save current position as Camera
    IF SaveCameraPos THEN
        PosCamera := ActualPosition;
        SaveCameraPos := FALSE;
    END_IF;
    
    // Save current position as Capper
    IF SaveCapperPos THEN
        PosCapper := ActualPosition;
        SaveCapperPos := FALSE;
    END_IF;
END_IF;

//=============================================================================
// ENHANCED GRIPPER CONTROL SEQUENCES
//=============================================================================

// Close gripper sequence (Enhanced with better fault handling)
IF CloseGripperCmd AND NOT CloseGripperActive AND NOT CloseGripperComplete AND NOT fbMoveAbs.Busy THEN
    CloseGripperActive := TRUE;           // B133[2].0
    GripperCloseOutput := TRUE;          
    GripperOpenOutput := FALSE;
    tGripperCloseDelay(IN := TRUE, PT := T#200MS);
END_IF;

// Complete close when feedback indicates closed or timeout
IF CloseGripperActive THEN
    IF bBottleTransferGripperClosed OR GripperIsClosed OR tGripperCloseDelay.Q THEN
        CloseGripperActive := FALSE;
        CloseGripperComplete := TRUE;     // B133[2].15
        GripperCloseOutput := FALSE;
        CloseGripperCmd := FALSE;         // Clear command
        tGripperCloseDelay(IN := FALSE);
    END_IF;
END_IF;

// Open gripper sequence (Enhanced with better fault handling)
IF OpenGripperCmd AND NOT OpenGripperActive AND NOT OpenGripperComplete AND NOT fbMoveAbs.Busy THEN
    OpenGripperActive := TRUE;            // B133[3].0
    GripperOpenOutput := TRUE;            
    GripperCloseOutput := FALSE;
    tGripperOpenDelay(IN := TRUE, PT := T#200MS);
END_IF;

IF OpenGripperActive THEN
    IF bBottleTransferGripperOpen OR GripperIsOpen OR tGripperOpenDelay.Q THEN
        OpenGripperActive := FALSE;
        OpenGripperComplete := TRUE;      // B133[3].15
        GripperOpenOutput := FALSE;
        OpenGripperCmd := FALSE;          // Clear command
        tGripperOpenDelay(IN := FALSE);
    END_IF;
END_IF;

//=============================================================================
// AUTO SEQUENCE STATE MACHINE (Enhanced)
//=============================================================================

CASE Step OF

    // Step 0 - Idle, wait for cycle start to go to Labeler 
    0:
        IF bOptifillMachineInAutoMode AND bCycleStart AND ServoReady AND 
           ServoHomed AND bAirSupplyOK AND bCapperDoorClosed THEN
            // Require gripper open before moving to Labeler 
            IF GripperIsOpen THEN
                MotionSeqRegister := 1;            
                TargetPosition := PosLabeler;
                TargetName := 'Labeler';
                MotionDataTransferred := FALSE;    
                Step := 10;
            ELSE
                // Request gripper open first
                OpenGripperCmd := TRUE;
            END_IF;
        END_IF;

    // Step 10 - Wait At Labeler
    10:
        IF MotionAtRegister = 1 THEN
            // Ensure gripper is open 
            IF NOT GripperIsOpen THEN
                OpenGripperCmd := TRUE;
            ELSE
                // Ready to inspect bottle - wait for bottle status
                // Bottle status should be set by external logic
                IF BottleStatus <> 0 THEN
                    Step := 20;
                END_IF
            END_IF;
        END_IF;

    // Step 20 - Decide routing based on BottleStatus
    20:
        // Before leaving Labeler, gripper must be closed
        IF NOT GripperIsClosed THEN
            CloseGripperCmd := TRUE;
            // Remain in step until closed
        ELSE
            CASE BottleStatus OF
                1:  // Passed - pick a cabinet 
                    IF (CabinetIndex >= 1) AND (CabinetIndex <= 5) THEN
                        MotionSeqRegister := 1 + CabinetIndex;            
                        TargetPosition := PosCabinet[CabinetIndex];
                        TargetName := CONCAT('Cabinet ', INT_TO_STRING(CabinetIndex));
                        Step := 30;
                    ELSE
                        // Invalid Cabinet Index - return to labeler wait
                        Step := 10;
                    END_IF

                2:  // RejectEmpty - direct to Capper
                    MotionSeqRegister := 14;        
                    TargetPosition := PosCapper;
                    TargetName := 'Capper';
                    Step := 100;

                3:  // RejectFull - direct to Capper  
                    MotionSeqRegister := 14;
                    TargetPosition := PosCapper;
                    TargetName := 'Capper';
                    Step := 100;

                ELSE
                    // No valid bottle status - wait at labeler
                    Step := 10;
            END_CASE;
        END_IF;

    // Step 30 - Arrived at Cabinet, wait for filler handshake
    30:
        IF (MotionAtRegister >= 2) AND (MotionAtRegister <= 6) THEN
            // Wait for fill complete signal
            IF FillComplete THEN
                MotionSeqRegister := 13;          
                TargetPosition := PosCamera;
                TargetName := 'Camera';
                Step := 40;
            END_IF;
        END_IF;

    // Step 40 - At Camera, wait for camera trigger or timeout
    40:
        IF MotionAtRegister = 13 THEN
            IF CameraTriggerOK THEN
                // Camera OK, proceed to capper
                Step := 50;
            ELSE
                // Camera timeout delay
                tCamDelay(IN := TRUE, PT := T#500MS); 
                IF tCamDelay.Q THEN
                    Step := 50;
                    tCamDelay(IN := FALSE);
                END_IF;
            END_IF;
        END_IF;

    // Step 50 - Command move to Capper
    50:
        MotionSeqRegister := 14;   
        TargetPosition := PosCapper;
        TargetName := 'Capper';
        Step := 60;

    // Step 60 - Wait at Capper for handoff complete
    60:
        IF MotionAtRegister = 14 THEN
            // Wait for capper to confirm handoff
            IF HandoffComplete THEN
                OpenGripperCmd := TRUE;
                Step := 70;
            END_IF;
        END_IF;

    // Step 70 - Wait for gripper open complete, then return to labeler
    70:
        IF OpenGripperComplete THEN
            MotionSeqRegister := 1;   
            TargetPosition := PosLabeler;
            TargetName := 'Labeler';
            Step := 0;                  
            BottleStatus := 0;
            OpenGripperComplete := FALSE; 
        END_IF;

    // Step 100 - Direct capper reject path
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
        // Safety fallback
        Step := 0;
END_CASE;

//=============================================================================
// MOTION EXECUTION ENGINE (Enhanced)
//=============================================================================

// Execute motion commands when register is set
IF (MotionSeqRegister <> 0) AND MotionSeqInProgress THEN
    IF ServoReady AND ServoHomed AND (NOT ServoFault) AND bAirSupplyOK AND bCapperDoorClosed THEN
        IF NOT fbMoveAbs.Execute AND NOT MotionDataTransferred THEN
            CASE MotionSeqRegister OF
                1:  // Labeler
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosLabeler, 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                2:  // Cabinet 1
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCabinet[1], 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                3:  // Cabinet 2
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCabinet[2], 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                4:  // Cabinet 3
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCabinet[3], 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                5:  // Cabinet 4
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCabinet[4], 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                6:  // Cabinet 5
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCabinet[5], 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                13: // Camera
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCamera, 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                14: // Capper
                    fbMoveAbs(
                        Axis := Axis, 
                        Execute := TRUE, 
                        Position := PosCapper, 
                        Velocity := 130.0, 
                        Acceleration := 533.0, 
                        Deceleration := 533.0
                    );
                    
                15: // Home
                    fbHome(Axis := Axis, Execute := TRUE);
                    
                ELSE
                    // Invalid register value - clear it
                    MotionSeqRegister := 0;
            END_CASE;
            
            // Set data transferred flag
            IF MotionSeqRegister <> 0 THEN
                MotionGoingTo := MotionSeqRegister;
                MotionDataTransferred := TRUE;    
            END_IF
        END_IF;
    END_IF;
END_IF;

//=============================================================================
// MOTION COMPLETION AND ERROR HANDLING
//=============================================================================

// Handle move completion
IF fbMoveAbs.Done THEN
    // Move finished successfully
    MotionAtRegister := MotionGoingTo;  
    MotionSeqRegister := 0;
    MotionDataTransferred := FALSE;     
    fbMoveAbs.Execute := FALSE;
    
    // Set in position flag for specific positions
    CASE MotionAtRegister OF
        1: InPosition := (ActualPosition >= -0.2 AND ActualPosition <= 0.5); // Labeler tolerance
        15: InPosition := TRUE; // Home position
        ELSE
            InPosition := TRUE; // Other positions
    END_CASE;         
END_IF;

// Handle home completion
IF fbHome.Done THEN
    ServoHomed := TRUE;
    MotionAtRegister := 15; // Home register
    MotionSeqRegister := 0;
    MotionDataTransferred := FALSE;
    fbHome.Execute := FALSE;
    InPosition := TRUE;
END_IF;

// Enhanced Error Handling
IF fbMoveAbs.Error OR fbHome.Error OR fbJogFwd.Error OR fbJogRev.Error THEN
    ServoFault := TRUE;
    
    // Stop all motion
    fbMoveAbs.Execute := FALSE;
    fbHome.Execute := FALSE;
    fbJogFwd.Execute := FALSE;
    fbJogRev.Execute := FALSE;
    
    // Clear motion registers
    MotionSeqRegister := 0; 
    MotionGoingTo := 0;
    MotionDataTransferred := FALSE;
    MotionSeqComplete := FALSE;
    
    // Return to safe state
    Step := 0;
    BottleStatus := 0;
END_IF;

//=============================================================================
// STATUS FLAGS AND CLEANUP
//=============================================================================

// Update actual position for display
ActualPosition := Axis.NcToPlc.ActPos;

// Clear completion flags after they've been processed
IF MotionSeqComplete AND NOT MotionSeqActive THEN
    MotionSeqComplete := FALSE;
END_IF;

// Auto-clear gripper complete flags after a cycle
IF NOT bOptifillMachineInAutoMode THEN
    IF OpenGripperComplete AND NOT OpenGripperCmd THEN
        OpenGripperComplete := FALSE;
    END_IF;
    
    IF CloseGripperComplete AND NOT CloseGripperCmd THEN
        CloseGripperComplete := FALSE;
    END_IF;
END_IF;

// Diagnostic - Set motion sequence complete when returning to step 0
IF Step = 0 AND MotionAtRegister = 1 AND bOptifillMachineInAutoMode THEN
    MotionSeqComplete := TRUE;   // B133[1].15
END_IF;
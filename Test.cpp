PROGRAM BottleTransfer
VAR
    // Servo Drive Variables
    Axis_SV                 : AXIS_REF;
    MC_Power               : MC_Power;
    MC_Home                : MC_Home;
    MC_MoveAbsolute        : MC_MoveAbsolute;
    MC_Jog                 : MC_Jog;
    MC_Stop                : MC_Stop;
    MC_Reset               : MC_Reset;
    
    // Position Arrays
    POSITION_4CAB          : ARRAY[1..15] OF LREAL := [0.0, 20.915, 53.825, 86.875, 119.875, 0, 0, 0, 0, 0, 0, 0, 131.699, 140.56, 0]; // 4 Cabinet System
    POSITION_5CAB          : ARRAY[1..15] OF LREAL := [0.0, 20.915, 53.825, 86.875, 119.875, 153.0, 0, 0, 0, 0, 0, 0, 131.699, 140.56, 0]; // 5 Cabinet System
    R_POSITION             : LREAL := -131.699; // Calculated position for move
    
    // System Control Variables
    bAutoMode              : BOOL := FALSE;
    bManualMode            : BOOL := FALSE;
    bSystemReset           : BOOL := FALSE;
    bCycleStart            : BOOL := FALSE;
    bEmergencyStop         : BOOL := FALSE;
    bTeachMode             : BOOL := FALSE;
    
    // Servo Drive Status
    bServoReady            : BOOL := FALSE;
    bServoEnabled          : BOOL := FALSE;
    bServoHomed            : BOOL := FALSE;
    bServoInPosition       : BOOL := FALSE;
    bServoMoving           : BOOL := FALSE;
    bServoFault            : BOOL := FALSE;
    
    // Motion Sequence Variables
    nMotionSequenceRegister : INT := 0;
    nGoingToRegister       : INT := 0;
    nAtRegister           : INT := 0;
    bMotionSequenceActive : BOOL := FALSE;
    bMotionSequenceComplete : BOOL := FALSE;
    bDataTransferred      : BOOL := FALSE;
    
    // Gripper Control
    bOpenGripper          : BOOL := FALSE;
    bCloseGripper         : BOOL := FALSE;
    bGripperOpen          : BOOL := FALSE;
    bGripperClosed        : BOOL := FALSE;
    bGripperSequenceActive : BOOL := FALSE;
    bGripperSequenceComplete : BOOL := FALSE;
    
    // Position Commands (Auto Mode)
    bGoToLabeler          : BOOL := FALSE;
    bGoToCabinet1         : BOOL := FALSE;
    bGoToCabinet2         : BOOL := FALSE;
    bGoToCabinet3         : BOOL := FALSE;
    bGoToCabinet4         : BOOL := FALSE;
    bGoToCabinet5         : BOOL := FALSE;
    bGoToCamera           : BOOL := FALSE;
    bGoToCapperFromLabeler : BOOL := FALSE;
    bGoToCapperFromCabinet : BOOL := FALSE;
    bGoToCapperFromCamera  : BOOL := FALSE;
    
    // Manual Mode Commands
    bManualHome           : BOOL := FALSE;
    bManualJogForward     : BOOL := FALSE;
    bManualJogReverse     : BOOL := FALSE;
    bManualGoToLabeler    : BOOL := FALSE;
    bManualGoToCabinet1   : BOOL := FALSE;
    bManualGoToCabinet2   : BOOL := FALSE;
    bManualGoToCabinet3   : BOOL := FALSE;
    bManualGoToCabinet4   : BOOL := FALSE;
    bManualGoToCabinet5   : BOOL := FALSE;
    bManualGoToCamera     : BOOL := FALSE;
    bManualGoToCapper     : BOOL := FALSE;
    bManualOpenGripper    : BOOL := FALSE;
    bManualCloseGripper   : BOOL := FALSE;
    
    // System Configuration
    bEnableCabinet1       : BOOL := TRUE;
    bEnableCabinet2       : BOOL := TRUE;
    bEnableCabinet3       : BOOL := TRUE;
    bEnableCabinet4       : BOOL := TRUE;
    bEnableCabinet5       : BOOL := FALSE; // Set to TRUE for 5 cabinet system
    bEnableVisionSystem   : BOOL := TRUE;
    
    // Timers
    TON_ServoFeedEnable   : TON;
    TON_ServoStartDelay   : TON;
    TON_GripperDelay      : TON;
    TON_ResetDelay        : TON;
    TON_HomeDelay         : TON;
    TON_CycleDelay        : TON;
    
    // Reset Sequence Variables
    bResetSequenceActive  : BOOL := FALSE;
    bResetSequenceComplete : BOOL := FALSE;
    nResetStep            : INT := 0;
    
    // Fault Variables
    nAxisFaultBits        : DWORD := 0;
    bAxisFaultActive      : BOOL := FALSE;
    
    // HMI Display Variables
    sCurrentPosition      : STRING := '';
    sTargetPosition       : STRING := '';
    rCurrentPositionMM    : LREAL := 0.0;
    rTargetPositionMM     : LREAL := 0.0;
    sServoStatus          : STRING := 'Not Ready';
    
    // Internal State Variables
    nMainSequenceStep     : INT := 0;
    bSequenceActive       : BOOL := FALSE;
    bFirstScan            : BOOL := TRUE;
    
    // Bottle Status Variables
    bBottlePassed         : BOOL := FALSE;
    bBottleRejected       : BOOL := FALSE;
    bFillComplete         : BOOL := FALSE;
    bHandoffComplete      : BOOL := FALSE;
    
    // Safety Interlocks
    bCapperDoorClosed     : BOOL := TRUE;
    bAirPressureOK        : BOOL := TRUE;
    bControlPowerOn       : BOOL := TRUE;
    
    // One Shot Variables
    R_TRIG_CycleStart     : R_TRIG;
    R_TRIG_Reset          : R_TRIG;
    R_TRIG_Home           : R_TRIG;
    R_TRIG_Manual         : R_TRIG;
    F_TRIG_PowerOff       : F_TRIG;
    
    // Position Teaching Variables
    bSavePosition         : BOOL := FALSE;
    nTeachPosition        : INT := 0; // Position number to teach (1-15)
    
END_VAR

VAR_INPUT
    // Physical Inputs
    bHomePositionSensor   : BOOL;
    bGripperOpenSensor    : BOOL;
    bGripperClosedSensor  : BOOL;
    bCapperAccessDoorClosed : BOOL;
    bAirSupplyOK          : BOOL;
    bMCRControlPowerOn    : BOOL;
    bServoAtPrePosition   : BOOL;
    bServoHomeCompleted   : BOOL;
    
    // HMI Inputs
    bHMI_AutoMode         : BOOL;
    bHMI_ManualMode       : BOOL;
    bHMI_Reset            : BOOL;
    bHMI_CycleStart       : BOOL;
    bHMI_Home             : BOOL;
    bHMI_JogForward       : BOOL;
    bHMI_JogReverse       : BOOL;
    bHMI_OpenGripper      : BOOL;
    bHMI_CloseGripper     : BOOL;
    bHMI_TeachMode        : BOOL;
    bHMI_SavePosition     : BOOL;
    nHMI_TeachPositionSelect : INT;
    
    // Process Inputs
    bBottleAtLabeler      : BOOL;
    bBottlePassedInspection : BOOL;
    bBottleRejectedEmpty  : BOOL;
    bCabinetFillComplete  : BOOL;
    bBottleRejectedFull   : BOOL;
    bCameraInspectionComplete : BOOL;
    bCapperHandoffComplete : BOOL;
    
END_VAR

VAR_OUTPUT
    // Physical Outputs
    bServoEnable          : BOOL;
    bServoReset           : BOOL;
    bOpenGripperOutput    : BOOL;
    bCloseGripperOutput   : BOOL;
    
    // Status Outputs to HMI
    bSystemReady          : BOOL;
    bSystemRunning        : BOOL;
    bSystemFault          : BOOL;
    
END_VAR

// ===== BOTTLE TRANSFER MAIN LOGIC =====

// First Scan Initialization
IF bFirstScan THEN
    bFirstScan := FALSE;
    nMotionSequenceRegister := 0;
    nGoingToRegister := 0;
    nAtRegister := 0;
    bMotionSequenceActive := FALSE;
    bResetSequenceActive := FALSE;
    nMainSequenceStep := 0;
END_IF

// Update Input Mappings
bControlPowerOn := bMCRControlPowerOn;
bAirPressureOK := bAirSupplyOK;
bCapperDoorClosed := bCapperAccessDoorClosed;
bGripperOpen := bGripperOpenSensor;
bGripperClosed := bGripperClosedSensor;

// One Shot Triggers
R_TRIG_CycleStart(CLK := bHMI_CycleStart);
R_TRIG_Reset(CLK := bHMI_Reset);
R_TRIG_Home(CLK := bHMI_Home);
R_TRIG_Manual(CLK := bHMI_ManualMode);
F_TRIG_PowerOff(CLK := bControlPowerOn);

// System Mode Control
IF bHMI_AutoMode AND NOT bHMI_ManualMode THEN
    bAutoMode := TRUE;
    bManualMode := FALSE;
ELSIF bHMI_ManualMode AND NOT bHMI_AutoMode THEN
    bAutoMode := FALSE;
    bManualMode := TRUE;
END_IF

// ===== SERVO DRIVE CONTROL =====

// Power Control
MC_Power(
    Axis := Axis_SV,
    Enable := bControlPowerOn AND bAirPressureOK AND NOT F_TRIG_PowerOff.Q,
    Enable_Positive := TRUE,
    Enable_Negative := TRUE
);

bServoEnabled := MC_Power.Status;
bServoReady := MC_Power.Status AND NOT MC_Power.Error;

// Servo Status Updates
bServoInPosition := ABS(Axis_SV.NcToPlc.ActPos - Axis_SV.NcToPlc.SetPos) < 0.1;
bServoMoving := MC_MoveAbsolute.Busy OR MC_Jog.Busy OR MC_Home.Busy;
bServoFault := MC_Power.Error OR MC_Home.Error OR MC_MoveAbsolute.Error OR MC_Jog.Error;

// Update HMI Status
rCurrentPositionMM := Axis_SV.NcToPlc.ActPos;
IF bServoFault THEN
    sServoStatus := 'Fault';
ELSIF bServoMoving THEN
    sServoStatus := 'Moving';
ELSIF bServoInPosition THEN
    sServoStatus := 'In Position';
ELSIF bServoReady THEN
    sServoStatus := 'Ready';
ELSE
    sServoStatus := 'Not Ready';
END_IF

// ===== RESET SEQUENCE =====
IF (R_TRIG_Reset.Q OR F_TRIG_PowerOff.Q) AND NOT bResetSequenceActive THEN
    bResetSequenceActive := TRUE;
    nResetStep := 1;
    bResetSequenceComplete := FALSE;
END_IF

IF bResetSequenceActive THEN
    CASE nResetStep OF
        1: // Clear all sequences and open gripper
            bMotionSequenceActive := FALSE;
            bGripperSequenceActive := FALSE;
            nMotionSequenceRegister := 0;
            bOpenGripper := TRUE;
            bCloseGripper := FALSE;
            IF bGripperOpen THEN
                nResetStep := 2;
            END_IF
            
        2: // Reset servo and start timers
            MC_Reset(Axis := Axis_SV, Execute := TRUE);
            TON_ResetDelay(IN := TRUE, PT := T#8S);
            IF MC_Reset.Done OR TON_ResetDelay.Q THEN
                MC_Reset(Execute := FALSE);
                nResetStep := 3;
                TON_ResetDelay(IN := FALSE);
            END_IF
            
        3: // Home sequence
            IF bServoReady THEN
                TON_CycleDelay(IN := TRUE, PT := T#1S);
                IF TON_CycleDelay.Q THEN
                    nMotionSequenceRegister := 15; // Home position
                    nResetStep := 4;
                    TON_CycleDelay(IN := FALSE);
                END_IF
            END_IF
            
        4: // Execute home
            MC_Home(
                Axis := Axis_SV,
                Execute := TRUE,
                Position := 0.0
            );
            IF MC_Home.Done THEN
                MC_Home(Execute := FALSE);
                nAtRegister := 15;
                bResetSequenceComplete := TRUE;
                bResetSequenceActive := FALSE;
                nResetStep := 0;
                bServoHomed := TRUE;
            END_IF
    END_CASE
END_IF

// ===== GRIPPER CONTROL =====

// Open Gripper Sequence
IF (bHMI_OpenGripper AND bManualMode) OR bOpenGripper THEN
    IF NOT bGripperOpen AND NOT bGripperSequenceActive THEN
        bGripperSequenceActive := TRUE;
        bOpenGripperOutput := TRUE;
        bCloseGripperOutput := FALSE;
        TON_GripperDelay(IN := TRUE, PT := T#200MS);
    END_IF
    
    IF bGripperOpen AND TON_GripperDelay.Q THEN
        bGripperSequenceActive := FALSE;
        bGripperSequenceComplete := TRUE;
        bOpenGripper := FALSE;
        TON_GripperDelay(IN := FALSE);
    END_IF
END_IF

// Close Gripper Sequence  
IF (bHMI_CloseGripper AND bManualMode) OR bCloseGripper THEN
    IF NOT bGripperClosed AND NOT bGripperSequenceActive THEN
        bGripperSequenceActive := TRUE;
        bCloseGripperOutput := TRUE;
        bOpenGripperOutput := FALSE;
        TON_GripperDelay(IN := TRUE, PT := T#200MS);
    END_IF
    
    IF bGripperClosed AND TON_GripperDelay.Q THEN
        bGripperSequenceActive := FALSE;
        bGripperSequenceComplete := TRUE;
        bCloseGripper := FALSE;
        TON_GripperDelay(IN := FALSE);
    END_IF
END_IF

// ===== MANUAL MODE OPERATIONS =====
IF bManualMode AND bServoReady AND bCapperDoorClosed THEN
    
    // Manual Homing
    IF bHMI_Home AND bGripperOpen THEN
        nMotionSequenceRegister := 15;
        MC_Home(Axis := Axis_SV, Execute := TRUE, Position := 0.0);
    END_IF
    
    // Manual Jogging
    IF bHMI_JogForward AND NOT bHMI_JogReverse THEN
        MC_Jog(
            Axis := Axis_SV,
            JogForward := TRUE,
            JogBackwards := FALSE,
            Velocity := 6.5
        );
    ELSIF bHMI_JogReverse AND NOT bHMI_JogForward THEN
        MC_Jog(
            Axis := Axis_SV,
            JogForward := FALSE,
            JogBackwards := TRUE,
            Velocity := 6.5
        );
    ELSE
        MC_Jog(
            Axis := Axis_SV,
            JogForward := FALSE,
            JogBackwards := FALSE
        );
    END_IF
    
    // Manual Position Commands
    IF bManualGoToLabeler THEN
        nMotionSequenceRegister := 1;
        bManualGoToLabeler := FALSE;
    ELSIF bManualGoToCabinet1 AND bEnableCabinet1 THEN
        nMotionSequenceRegister := 2;
        bManualGoToCabinet1 := FALSE;
    ELSIF bManualGoToCabinet2 AND bEnableCabinet2 THEN
        nMotionSequenceRegister := 3;
        bManualGoToCabinet2 := FALSE;
    ELSIF bManualGoToCabinet3 AND bEnableCabinet3 THEN
        nMotionSequenceRegister := 4;
        bManualGoToCabinet3 := FALSE;
    ELSIF bManualGoToCabinet4 AND bEnableCabinet4 THEN
        nMotionSequenceRegister := 5;
        bManualGoToCabinet4 := FALSE;
    ELSIF bManualGoToCabinet5 AND bEnableCabinet5 THEN
        nMotionSequenceRegister := 6;
        bManualGoToCabinet5 := FALSE;
    ELSIF bManualGoToCamera THEN
        nMotionSequenceRegister := 13;
        bManualGoToCamera := FALSE;
    ELSIF bManualGoToCapper THEN
        nMotionSequenceRegister := 14;
        bManualGoToCapper := FALSE;
    END_IF
    
    // Teaching Mode
    IF bHMI_TeachMode AND bHMI_SavePosition AND (nHMI_TeachPositionSelect >= 1) AND (nHMI_TeachPositionSelect <= 15) THEN
        IF bEnableCabinet5 THEN
            POSITION_5CAB[nHMI_TeachPositionSelect] := Axis_SV.NcToPlc.ActPos;
        ELSE
            POSITION_4CAB[nHMI_TeachPositionSelect] := Axis_SV.NcToPlc.ActPos;
        END_IF
    END_IF
    
END_IF

// ===== AUTO MODE OPERATIONS =====
IF bAutoMode AND bServoReady AND NOT bResetSequenceActive THEN
    
    // Auto Mode Position Commands
    IF bGoToLabeler THEN
        nMotionSequenceRegister := 1;
        bGoToLabeler := FALSE;
    ELSIF bGoToCabinet1 THEN
        nMotionSequenceRegister := 2;
        bGoToCabinet1 := FALSE;
    ELSIF bGoToCabinet2 THEN
        nMotionSequenceRegister := 3;
        bGoToCabinet2 := FALSE;
    ELSIF bGoToCabinet3 THEN
        nMotionSequenceRegister := 4;
        bGoToCabinet3 := FALSE;
    ELSIF bGoToCabinet4 THEN
        nMotionSequenceRegister := 5;
        bGoToCabinet4 := FALSE;
    ELSIF bGoToCabinet5 THEN
        nMotionSequenceRegister := 6;
        bGoToCabinet5 := FALSE;
    ELSIF bGoToCamera THEN
        nMotionSequenceRegister := 13;
        bGoToCamera := FALSE;
    ELSIF bGoToCapperFromLabeler OR bGoToCapperFromCabinet OR bGoToCapperFromCamera THEN
        nMotionSequenceRegister := 14;
        bGoToCapperFromLabeler := FALSE;
        bGoToCapperFromCabinet := FALSE;
        bGoToCapperFromCamera := FALSE;
    END_IF
    
END_IF

// ===== MOTION EXECUTION =====
IF nMotionSequenceRegister > 0 AND nMotionSequenceRegister <= 15 AND bServoReady THEN
    
    // Transfer data and start motion
    IF NOT bDataTransferred THEN
        nGoingToRegister := nMotionSequenceRegister;
        nAtRegister := 0;
        bDataTransferred := TRUE;
        bMotionSequenceActive := TRUE;
    END_IF
    
    // Select position based on system configuration
    IF bEnableCabinet5 THEN
        R_POSITION := POSITION_5CAB[nMotionSequenceRegister];
    ELSE
        R_POSITION := POSITION_4CAB[nMotionSequenceRegister];
    END_IF
    
    // Execute movement
    MC_MoveAbsolute(
        Axis := Axis_SV,
        Execute := TRUE,
        Position := R_POSITION,
        Velocity := 130.0,
        Acceleration := 533.0,
        Deceleration := 533.0
    );
    
    // Check if move is complete
    IF MC_MoveAbsolute.Done THEN
        MC_MoveAbsolute(Execute := FALSE);
        nAtRegister := nGoingToRegister;
        nMotionSequenceRegister := 0;
        bDataTransferred := FALSE;
        bMotionSequenceActive := FALSE;
        bMotionSequenceComplete := TRUE;
        
        // Update HMI display
        CASE nAtRegister OF
            1: sCurrentPosition := 'Labeler';
            2: sCurrentPosition := 'Cabinet 1';
            3: sCurrentPosition := 'Cabinet 2';
            4: sCurrentPosition := 'Cabinet 3';
            5: sCurrentPosition := 'Cabinet 4';
            6: sCurrentPosition := 'Cabinet 5';
            13: sCurrentPosition := 'Camera';
            14: sCurrentPosition := 'Capper';
            15: sCurrentPosition := 'Home';
            ELSE sCurrentPosition := 'Unknown';
        END_CASE
        
        rTargetPositionMM := R_POSITION;
    END_IF
END_IF

// ===== MAIN AUTO SEQUENCE =====
IF bAutoMode AND R_TRIG_CycleStart.Q THEN
    nMainSequenceStep := 1;
    bSequenceActive := TRUE;
END_IF

IF bAutoMode AND bSequenceActive THEN
    CASE nMainSequenceStep OF
        1: // Go to Labeler (Idle Position)
            bGoToLabeler := TRUE;
            bOpenGripper := TRUE; // Ensure gripper is open
            IF nAtRegister = 1 AND bGripperOpen THEN
                nMainSequenceStep := 2;
            END_IF
            
        2: // Wait for bottle and check status
            IF bBottleAtLabeler THEN
                IF bBottlePassedInspection THEN
                    // Determine which cabinet to go to (simplified - use first available)
                    IF bEnableCabinet1 THEN
                        nMainSequenceStep := 10; // Go to Cabinet 1
                    ELSIF bEnableCabinet2 THEN
                        nMainSequenceStep := 11; // Go to Cabinet 2
                    END_IF
                ELSIF bBottleRejectedEmpty THEN
                    nMainSequenceStep := 20; // Go to Capper
                END_IF
            END_IF
            
        10: // Go to Cabinet 1
            bGoToCabinet1 := TRUE;
            IF nAtRegister = 2 THEN
                nMainSequenceStep := 30; // Wait for fill
            END_IF
            
        11: // Go to Cabinet 2
            bGoToCabinet2 := TRUE;
            IF nAtRegister = 3 THEN
                nMainSequenceStep := 30; // Wait for fill
            END_IF
            
        20: // Go to Capper
            bGoToCapperFromLabeler := TRUE;
            IF nAtRegister = 14 THEN
                nMainSequenceStep := 50; // Handoff sequence
            END_IF
            
        30: // Wait for fill complete at cabinet
            IF bCabinetFillComplete THEN
                IF bBottlePassedInspection THEN
                    nMainSequenceStep := 40; // Go to Camera
                ELSIF bBottleRejectedFull THEN
                    nMainSequenceStep := 20; // Go to Capper
                END_IF
            END_IF
            
        40: // Go to Camera
            bGoToCamera := TRUE;
            IF nAtRegister = 13 THEN
                nMainSequenceStep := 41; // Wait at camera
            END_IF
            
        41: // Wait at Camera (timer delay or PPS signal)
            TON_CycleDelay(IN := TRUE, PT := T#2S); // 2 second delay
            IF TON_CycleDelay.Q OR bCameraInspectionComplete THEN
                TON_CycleDelay(IN := FALSE);
                nMainSequenceStep := 20; // Go to Capper
            END_IF
            
        50: // Handoff at Capper
            IF bCapperHandoffComplete THEN
                bOpenGripper := TRUE;
                IF bGripperOpen THEN
                    nMainSequenceStep := 1; // Return to Labeler
                END_IF
            END_IF
    END_CASE
END_IF

// ===== FAULT HANDLING =====
IF bServoFault THEN
    MC_Reset(Axis := Axis_SV, Execute := TRUE);
    bSystemFault := TRUE;
ELSE
    MC_Reset(Execute := FALSE);
    bSystemFault := FALSE;
END_IF

// ===== SYSTEM STATUS =====
bSystemReady := bServoReady AND bControlPowerOn AND bAirPressureOK AND NOT bSystemFault;
bSystemRunning := bSequenceActive OR bMotionSequenceActive OR bGripperSequenceActive;

// ===== HMI INTERFACE VARIABLES AND FUNCTIONS =====

VAR_GLOBAL
    // HMI Display Structure
    HMI_BottleTransfer : ST_HMI_BottleTransfer;
END_VAR

TYPE ST_HMI_BottleTransfer :
STRUCT
    // Position Information
    sCurrentPosition        : STRING(50) := '';           // Current position name
    rCurrentPositionMM      : LREAL := 0.0;              // Current position in mm
    sTargetPosition         : STRING(50) := '';           // Target position name  
    rTargetPositionMM       : LREAL := 0.0;              // Target position in mm
    
    // Servo Status
    sServoStatus           : STRING(20) := 'Not Ready';   // Ready, Moving, In Position, Fault
    bServoReady            : BOOL := FALSE;
    bServoMoving           : BOOL := FALSE;
    bServoInPosition       : BOOL := FALSE;
    bServoFault            : BOOL := FALSE;
    bServoHomed            : BOOL := FALSE;
    
    // System Status
    bSystemReady           : BOOL := FALSE;
    bSystemRunning         : BOOL := FALSE;
    bSystemFault           : BOOL := FALSE;
    bAutoMode              : BOOL := FALSE;
    bManualMode            : BOOL := FALSE;
    bTeachMode             : BOOL := FALSE;
    
    // Gripper Status
    bGripperOpen           : BOOL := FALSE;
    bGripperClosed         : BOOL := FALSE;
    sGripperStatus         : STRING(20) := 'Unknown';
    
    // Manual Controls
    bManualHome            : BOOL := FALSE;
    bManualJogForward      : BOOL := FALSE;
    bManualJogReverse      : BOOL := FALSE;
    bManualOpenGripper     : BOOL := FALSE;
    bManualCloseGripper    : BOOL := FALSE;
    
    // Position Commands
    bGoToLabeler           : BOOL := FALSE;
    bGoToCabinet1          : BOOL := FALSE;
    bGoToCabinet2          : BOOL := FALSE;
    bGoToCabinet3          : BOOL := FALSE;
    bGoToCabinet4          : BOOL := FALSE;
    bGoToCabinet5          : BOOL := FALSE;
    bGoToCamera            : BOOL := FALSE;
    bGoToCapper            : BOOL := FALSE;
    
    // Teaching Mode
    nTeachPositionSelect   : INT := 1;                    // Position to teach (1-15)
    bSavePosition          : BOOL := FALSE;
    arPositionNames        : ARRAY[1..15] OF STRING(20);
    arPositionValues       : ARRAY[1..15] OF LREAL;
    
    // System Controls
    bCycleStart            : BOOL := FALSE;
    bReset                 : BOOL := FALSE;
    bClearFault            : BOOL := FALSE;
    
    // Configuration
    bEnableCabinet1        : BOOL := TRUE;
    bEnableCabinet2        : BOOL := TRUE;
    bEnableCabinet3        : BOOL := TRUE;
    bEnableCabinet4        : BOOL := TRUE;
    bEnableCabinet5        : BOOL := FALSE;
    bEnableVisionSystem    : BOOL := TRUE;
    
    // Alarms and Messages
    sAlarmMessage          : STRING(100) := '';
    bAlarmActive           : BOOL := FALSE;
    sStatusMessage         : STRING(100) := '';
    
    // Diagnostics
    nSequenceStep          : INT := 0;
    rAxisVelocity          : LREAL := 0.0;
    rAxisAcceleration      : LREAL := 0.0;
    nAxisErrorCode         : DWORD := 0;
    
END_STRUCT
END_TYPE

// ===== HMI UPDATE FUNCTION =====
FUNCTION UpdateHMI : BOOL
VAR_INPUT
    pBottleTransfer : POINTER TO BottleTransfer;
END_VAR

// Update current position display
HMI_BottleTransfer.rCurrentPositionMM := pBottleTransfer^.Axis_SV.NcToPlc.ActPos;

// Update position names
HMI_BottleTransfer.arPositionNames[1] := 'Labeler';
HMI_BottleTransfer.arPositionNames[2] := 'Cabinet 1';
HMI_BottleTransfer.arPositionNames[3] := 'Cabinet 2';
HMI_BottleTransfer.arPositionNames[4] := 'Cabinet 3';
HMI_BottleTransfer.arPositionNames[5] := 'Cabinet 4';
HMI_BottleTransfer.arPositionNames[6] := 'Cabinet 5';
HMI_BottleTransfer.arPositionNames[13] := 'Camera';
HMI_BottleTransfer.arPositionNames[14] := 'Capper';
HMI_BottleTransfer.arPositionNames[15] := 'Home';

// Update position values based on system configuration
IF pBottleTransfer^.bEnableCabinet5 THEN
    FOR i := 1 TO 15 DO
        HMI_BottleTransfer.arPositionValues[i] := pBottleTransfer^.POSITION_5CAB[i];
    END_FOR
ELSE
    FOR i := 1 TO 15 DO
        HMI_BottleTransfer.arPositionValues[i] := pBottleTransfer^.POSITION_4CAB[i];
    END_FOR
END_IF

// Update current position name
CASE pBottleTransfer^.nAtRegister OF
    1: HMI_BottleTransfer.sCurrentPosition := 'Labeler';
    2: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 1';
    3: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 2';
    4: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 3';
    5: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 4';
    6: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 5';
    13: HMI_BottleTransfer.sCurrentPosition := 'Camera';
    14: HMI_BottleTransfer.sCurrentPosition := 'Capper';
    15: HMI_BottleTransfer.sCurrentPosition := 'Home';
    ELSE HMI_BottleTransfer.sCurrentPosition := 'Moving';
END_CASE

// Update target position
IF pBottleTransfer^.nGoingToRegister > 0 THEN
    HMI_BottleTransfer.sTargetPosition := HMI_BottleTransfer.arPositionNames[pBottleTransfer^.nGoingToRegister];
    HMI_BottleTransfer.rTargetPositionMM := HMI_BottleTransfer.arPositionValues[pBottleTransfer^.nGoingToRegister];
ELSE
    HMI_BottleTransfer.sTargetPosition := HMI_BottleTransfer.sCurrentPosition;
    HMI_BottleTransfer.rTargetPositionMM := HMI_BottleTransfer.rCurrentPositionMM;
END_IF

// Update servo status
HMI_BottleTransfer.bServoReady := pBottleTransfer^.bServoReady;
HMI_BottleTransfer.bServoMoving := pBottleTransfer^.bServoMoving;
HMI_BottleTransfer.bServoInPosition := pBottleTransfer^.bServoInPosition;
HMI_BottleTransfer.bServoFault := pBottleTransfer^.bServoFault;
HMI_BottleTransfer.bServoHomed := pBottleTransfer^.bServoHomed;
HMI_BottleTransfer.sServoStatus := pBottleTransfer^.sServoStatus;

// Update gripper status
HMI_BottleTransfer.bGripperOpen := pBottleTransfer^.bGripperOpen;
HMI_BottleTransfer.bGripperClosed := pBottleTransfer^.bGripperClosed;
IF pBottleTransfer^.bGripperOpen THEN
    HMI_BottleTransfer.sGripperStatus := 'Open';
ELSIF pBottleTransfer^.bGripperClosed THEN
    HMI_BottleTransfer.sGripperStatus := 'Closed';
ELSE
    HMI_BottleTransfer.sGripperStatus := 'Moving';
END_IF

// Update system status
HMI_BottleTransfer.bSystemReady := pBottleTransfer^.bSystemReady;
HMI_BottleTransfer.bSystemRunning := pBottleTransfer^.bSystemRunning;
HMI_BottleTransfer.bSystemFault := pBottleTransfer^.bSystemFault;
HMI_BottleTransfer.bAutoMode := pBottleTransfer^.bAutoMode;
HMI_BottleTransfer.bManualMode := pBottleTransfer^.bManualMode;
HMI_BottleTransfer.bTeachMode := pBottleTransfer^.bTeachMode;

// Update diagnostics
HMI_BottleTransfer.nSequenceStep := pBottleTransfer^.nMainSequenceStep;
HMI_BottleTransfer.rAxisVelocity := pBottleTransfer^.Axis_SV.NcToPlc.ActVelo;
HMI_BottleTransfer.nAxisErrorCode := pBottleTransfer^.Axis_SV.NcToPlc.ErrorCode;

// Generate status messages
IF pBottleTransfer^.bSystemFault THEN
    HMI_BottleTransfer.sStatusMessage := 'System Fault - Check servo status';
    HMI_BottleTransfer.bAlarmActive := TRUE;
ELSIF NOT pBottleTransfer^.bControlPowerOn THEN
    HMI_BottleTransfer.sStatusMessage := 'Control power off';
    HMI_BottleTransfer.bAlarmActive := TRUE;
ELSIF NOT pBottleTransfer^.bAirPressureOK THEN
    HMI_BottleTransfer.sStatusMessage := 'Air pressure low';
    HMI_BottleTransfer.bAlarmActive := TRUE;
ELSIF pBottleTransfer^.bResetSequenceActive THEN
    HMI_BottleTransfer.sStatusMessage := 'Reset sequence active';
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSIF pBottleTransfer^.bSequenceActive THEN
    HMI_BottleTransfer.sStatusMessage := 'Auto sequence running';
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSIF pBottleTransfer^.bManualMode THEN
    HMI_BottleTransfer.sStatusMessage := 'Manual mode active';
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSIF pBottleTransfer^.bSystemReady THEN
    HMI_BottleTransfer.sStatusMessage := 'System ready';
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSE
    HMI_BottleTransfer.sStatusMessage := 'System not ready';
    HMI_BottleTransfer.bAlarmActive := TRUE;
END_IF

UpdateHMI := TRUE;

END_FUNCTION

// ===== POSITION TEACHING FUNCTION =====
FUNCTION TeachPosition : BOOL
VAR_INPUT
    nPosition : INT;        // Position number (1-15)
    rValue : LREAL;         // Position value in mm
    b5CabinetSystem : BOOL; // TRUE for 5 cabinet system
END_VAR

IF nPosition >= 1 AND nPosition <= 15 THEN
    IF b5CabinetSystem THEN
        // Update 5 cabinet position array (this would be in the main program)
        // POSITION_5CAB[nPosition] := rValue;
    ELSE
        // Update 4 cabinet position array (this would be in the main program)
        // POSITION_4CAB[nPosition] := rValue;
    END_IF
    TeachPosition := TRUE;
ELSE
    TeachPosition := FALSE;
END_IF

END_FUNCTION


// ===== SAFETY INTERLOCKS AND UTILITY FUNCTIONS =====

FUNCTION_BLOCK FB_SafetyInterlocks
VAR_INPUT
    bControlPowerOn         : BOOL;
    bAirPressureOK          : BOOL;
    bCapperDoorClosed       : BOOL;
    bEmergencyStop          : BOOL;
    bServoReady            : BOOL;
    bGripperOpen           : BOOL;
    nCurrentPosition       : INT;
END_VAR

VAR_OUTPUT
    bMotionPermitted       : BOOL := FALSE;
    bJogPermitted          : BOOL := FALSE;
    bGripperOperationPermitted : BOOL := FALSE;
    sInterlockMessage      : STRING(100) := '';
END_VAR

VAR
    bAllSafetyOK          : BOOL := FALSE;
END_VAR

// Check all safety conditions
bAllSafetyOK := bControlPowerOn AND bAirPressureOK AND NOT bEmergencyStop;

// Motion interlocks
bMotionPermitted := bAllSafetyOK AND bServoReady AND bCapperDoorClosed;

// Jogging interlocks (additional check for position limits)
bJogPermitted := bMotionPermitted AND (nCurrentPosition <> 0); // Not at unknown position

// Gripper operation interlocks  
bGripperOperationPermitted := bAllSafetyOK;

// Generate interlock messages
IF NOT bControlPowerOn THEN
    sInterlockMessage := 'Control power is OFF';
ELSIF NOT bAirPressureOK THEN
    sInterlockMessage := 'Air pressure is LOW';
ELSIF bEmergencyStop THEN
    sInterlockMessage := 'Emergency stop is ACTIVE';
ELSIF NOT bCapperDoorClosed THEN
    sInterlockMessage := 'Capper access door is OPEN';
ELSIF NOT bServoReady THEN
    sInterlockMessage := 'Servo drive is NOT READY';
ELSE
    sInterlockMessage := 'All interlocks satisfied';
END_IF

END_FUNCTION_BLOCK

// ===== POSITION VALIDATION FUNCTION =====
FUNCTION ValidatePosition : BOOL
VAR_INPUT
    nPosition : INT;
    bEnableCabinet5 : BOOL;
END_VAR

CASE nPosition OF
    1:   ValidatePosition := TRUE;  // Labeler - always valid
    2:   ValidatePosition := TRUE;  // Cabinet 1 - always valid
    3:   ValidatePosition := TRUE;  // Cabinet 2 - always valid  
    4:   ValidatePosition := TRUE;  // Cabinet 3 - always valid
    5:   ValidatePosition := TRUE;  // Cabinet 4 - always valid
    6:   ValidatePosition := bEnableCabinet5;  // Cabinet 5 - only if enabled
    13:  ValidatePosition := TRUE;  // Camera - always valid
    14:  ValidatePosition := TRUE;  // Capper - always valid
    15:  ValidatePosition := TRUE;  // Home - always valid
    ELSE ValidatePosition := FALSE; // Invalid position
END_CASE

END_FUNCTION

// ===== BOTTLE SEQUENCE LOGIC =====
FUNCTION_BLOCK FB_BottleSequence
VAR_INPUT
    bAutoMode              : BOOL;
    bSequenceStart         : BOOL;
    nCurrentPosition       : INT;
    bBottleAtLabeler       : BOOL;
    bBottlePassed          : BOOL;
    bBottleRejectedEmpty   : BOOL;
    bBottleRejectedFull    : BOOL;
    bFillComplete          : BOOL;
    bCameraComplete        : BOOL;
    bHandoffComplete       : BOOL;
    bEnableCabinet1        : BOOL;
    bEnableCabinet2        : BOOL;
    bEnableCabinet3        : BOOL;
    bEnableCabinet4        : BOOL;
    bEnableCabinet5        : BOOL;
END_VAR

VAR_OUTPUT
    bGoToLabeler           : BOOL := FALSE;
    bGoToCabinet1          : BOOL := FALSE;
    bGoToCabinet2          : BOOL := FALSE;
    bGoToCabinet3          : BOOL := FALSE;
    bGoToCabinet4          : BOOL := FALSE;
    bGoToCabinet5          : BOOL := FALSE;
    bGoToCamera            : BOOL := FALSE;
    bGoToCapper            : BOOL := FALSE;
    bOpenGripper           : BOOL := FALSE;
    bCloseGripper          : BOOL := FALSE;
    bSequenceActive        : BOOL := FALSE;
    nSequenceStep          : INT := 0;
END_VAR

VAR
    R_TRIG_Start           : R_TRIG;
    TON_CameraDelay        : TON;
    nCabinetSelection      : INT := 0;
END_VAR

R_TRIG_Start(CLK := bSequenceStart);

IF bAutoMode AND R_TRIG_Start.Q THEN
    bSequenceActive := TRUE;
    nSequenceStep := 1;
END_IF

IF bSequenceActive THEN
    CASE nSequenceStep OF
        0: // Idle
            bSequenceActive := FALSE;
            
        1: // Go to Labeler (Idle Position)
            bGoToLabeler := TRUE;
            bOpenGripper := TRUE;
            IF nCurrentPosition = 1 THEN
                bGoToLabeler := FALSE;
                nSequenceStep := 2;
            END_IF
            
        2: // Wait for bottle at labeler
            IF bBottleAtLabeler THEN
                IF bBottlePassed THEN
                    // Select cabinet based on availability
                    IF bEnableCabinet1 THEN
                        nCabinetSelection := 1;
                        nSequenceStep := 10;
                    ELSIF bEnableCabinet2 THEN
                        nCabinetSelection := 2;
                        nSequenceStep := 10;
                    ELSIF bEnableCabinet3 THEN
                        nCabinetSelection := 3;
                        nSequenceStep := 10;
                    ELSIF bEnableCabinet4 THEN
                        nCabinetSelection := 4;
                        nSequenceStep := 10;
                    ELSIF bEnableCabinet5 THEN
                        nCabinetSelection := 5;
                        nSequenceStep := 10;
                    END_IF
                ELSIF bBottleRejectedEmpty THEN
                    nSequenceStep := 40; // Go directly to capper
                END_IF
            END_IF
            
        10: // Go to selected cabinet
            CASE nCabinetSelection OF
                1: bGoToCabinet1 := TRUE;
                2: bGoToCabinet2 := TRUE;
                3: bGoToCabinet3 := TRUE;
                4: bGoToCabinet4 := TRUE;
                5: bGoToCabinet5 := TRUE;
            END_CASE
            
            IF nCurrentPosition = (nCabinetSelection + 1) THEN // Cabinet positions are 2-6
                bGoToCabinet1 := FALSE;
                bGoToCabinet2 := FALSE;
                bGoToCabinet3 := FALSE;
                bGoToCabinet4 := FALSE;
                bGoToCabinet5 := FALSE;
                nSequenceStep := 20;
            END_IF
            
        20: // Wait for fill complete at cabinet
            IF bFillComplete THEN
                IF NOT bBottleRejectedFull THEN
                    nSequenceStep := 30; // Go to camera
                ELSE
                    nSequenceStep := 40; // Go to capper
                END_IF
            END_IF
            
        30: // Go to camera
            bGoToCamera := TRUE;
            IF nCurrentPosition = 13 THEN
                bGoToCamera := FALSE;
                nSequenceStep := 31;
            END_IF
            
        31: // Wait at camera
            TON_CameraDelay(IN := TRUE, PT := T#2S);
            IF TON_CameraDelay.Q OR bCameraComplete THEN
                TON_CameraDelay(IN := FALSE);
                nSequenceStep := 40;
            END_IF
            
        40: // Go to capper
            bGoToCapper := TRUE;
            IF nCurrentPosition = 14 THEN
                bGoToCapper := FALSE;
                nSequenceStep := 50;
            END_IF
            
        50: // Handoff at capper
            IF bHandoffComplete THEN
                bOpenGripper := TRUE;
                nSequenceStep := 60;
            END_IF
            
        60: // Return to labeler
            bGoToLabeler := TRUE;
            IF nCurrentPosition = 1 THEN
                bGoToLabeler := FALSE;
                nSequenceStep := 0; // Complete sequence
            END_IF
    END_CASE
END_IF

END_FUNCTION_BLOCK

// ===== DIAGNOSTIC FUNCTION =====
FUNCTION_BLOCK FB_Diagnostics
VAR_INPUT
    Axis : REFERENCE TO AXIS_REF;
    bUpdateDiagnostics : BOOL;
END_VAR

VAR_OUTPUT
    rActualPosition    : LREAL;
    rActualVelocity    : LREAL;
    rTargetPosition    : LREAL;
    nAxisState         : INT;
    nErrorCode         : DWORD;
    bAxisError         : BOOL;
    sAxisStatus        : STRING(50);
END_VAR

IF bUpdateDiagnostics THEN
    rActualPosition := Axis.NcToPlc.ActPos;
    rActualVelocity := Axis.NcToPlc.ActVelo;
    rTargetPosition := Axis.NcToPlc.SetPos;
    nAxisState := Axis.NcToPlc.StateDWord;
    nErrorCode := Axis.NcToPlc.ErrorCode;
    bAxisError := (nErrorCode <> 0);
    
    // Generate status string
    IF bAxisError THEN
        sAxisStatus := CONCAT('Error: ', DWORD_TO_STRING(nErrorCode));
    ELSIF ABS(rActualVelocity) > 0.1 THEN
        sAxisStatus := 'Moving';
    ELSIF ABS(rActualPosition - rTargetPosition) < 0.1 THEN
        sAxisStatus := 'In Position';
    ELSE
        sAxisStatus := 'Ready';
    END_IF
END_IF

END_FUNCTION_BLOCK

// ===== POSITION LIMITS CHECK =====
FUNCTION CheckPositionLimits : BOOL
VAR_INPUT
    rCurrentPosition : LREAL;
    bJogForward : BOOL;
    bJogReverse : BOOL;
END_VAR

VAR_CONSTANT
    rMinPosition : LREAL := -141.0;  // Minimum allowed position
    rMaxPosition : LREAL := 160.0;   // Maximum allowed position
END_VAR

CheckPositionLimits := TRUE;

// Check forward limit
IF bJogForward AND (rCurrentPosition >= rMaxPosition) THEN
    CheckPositionLimits := FALSE;
END_IF

// Check reverse limit  
IF bJogReverse AND (rCurrentPosition <= rMinPosition) THEN
    CheckPositionLimits := FALSE;
END_IF

END_FUNCTION

PROGRAM MAIN
VAR
    // Main bottle transfer instance
    BottleTransfer_Instance : BottleTransfer;
    
    // Function block instances
    SafetyInterlocks : FB_SafetyInterlocks;
    BottleSequence : FB_BottleSequence;
    Diagnostics : FB_Diagnostics;
    
    // Timers for main program
    TON_ScanTime : TON;
    
    // I/O Mapping (these would be mapped to actual hardware)
    // Inputs
    bHomePositionSensor AT %I* : BOOL;
    bGripperOpenSensor AT %I* : BOOL;
    bGripperClosedSensor AT %I* : BOOL;
    bCapperAccessDoorClosed AT %I* : BOOL;
    bAirSupplyOK AT %I* : BOOL;
    bMCRControlPowerOn AT %I* : BOOL;
    bServoAtPrePosition AT %I* : BOOL;
    bServoHomeCompleted AT %I* : BOOL;
    
    // Outputs
    bServoEnable AT %Q* : BOOL;
    bServoReset AT %Q* : BOOL;
    bOpenGripperOutput AT %Q* : BOOL;
    bCloseGripperOutput AT %Q* : BOOL;
    
END_VAR

// ===== MAIN PROGRAM EXECUTION =====

// Map physical I/O to program variables
BottleTransfer_Instance.bHomePositionSensor := bHomePositionSensor;
BottleTransfer_Instance.bGripperOpenSensor := bGripperOpenSensor;
BottleTransfer_Instance.bGripperClosedSensor := bGripperClosedSensor;
BottleTransfer_Instance.bCapperAccessDoorClosed := bCapperAccessDoorClosed;
BottleTransfer_Instance.bAirSupplyOK := bAirSupplyOK;
BottleTransfer_Instance.bMCRControlPowerOn := bMCRControlPowerOn;
BottleTransfer_Instance.bServoAtPrePosition := bServoAtPrePosition;
BottleTransfer_Instance.bServoHomeCompleted := bServoHomeCompleted;

// Map HMI inputs to program variables
BottleTransfer_Instance.bHMI_AutoMode := HMI_BottleTransfer.bAutoMode;
BottleTransfer_Instance.bHMI_ManualMode := HMI_BottleTransfer.bManualMode;
BottleTransfer_Instance.bHMI_Reset := HMI_BottleTransfer.bReset;
BottleTransfer_Instance.bHMI_CycleStart := HMI_BottleTransfer.bCycleStart;
BottleTransfer_Instance.bHMI_Home := HMI_BottleTransfer.bManualHome;
BottleTransfer_Instance.bHMI_JogForward := HMI_BottleTransfer.bManualJogForward;
BottleTransfer_Instance.bHMI_JogReverse := HMI_BottleTransfer.bManualJogReverse;
BottleTransfer_Instance.bHMI_OpenGripper := HMI_BottleTransfer.bManualOpenGripper;
BottleTransfer_Instance.bHMI_CloseGripper := HMI_BottleTransfer.bManualCloseGripper;
BottleTransfer_Instance.bHMI_TeachMode := HMI_BottleTransfer.bTeachMode;
BottleTransfer_Instance.bHMI_SavePosition := HMI_BottleTransfer.bSavePosition;
BottleTransfer_Instance.nHMI_TeachPositionSelect := HMI_BottleTransfer.nTeachPositionSelect;

// Execute Safety Interlocks
SafetyInterlocks(
    bControlPowerOn := BottleTransfer_Instance.bControlPowerOn,
    bAirPressureOK := BottleTransfer_Instance.bAirPressureOK,
    bCapperDoorClosed := BottleTransfer_Instance.bCapperDoorClosed,
    bEmergencyStop := FALSE, // Map to actual E-Stop
    bServoReady := BottleTransfer_Instance.bServoReady,
    bGripperOpen := BottleTransfer_Instance.bGripperOpen,
    nCurrentPosition := BottleTransfer_Instance.nAtRegister
);

// Execute Bottle Sequence Logic
BottleSequence(
    bAutoMode := BottleTransfer_Instance.bAutoMode,
    bSequenceStart := BottleTransfer_Instance.bCycleStart,
    nCurrentPosition := BottleTransfer_Instance.nAtRegister,
    bBottleAtLabeler := TRUE, // Map to actual sensor
    bBottlePassed := TRUE, // Map to actual inspection result
    bBottleRejectedEmpty := FALSE, // Map to actual inspection result
    bBottleRejectedFull := FALSE, // Map to actual inspection result
    bFillComplete := TRUE, // Map to cabinet fill complete signal
    bCameraComplete := FALSE, // Map to camera inspection complete
    bHandoffComplete := TRUE, // Map to capper handoff complete signal
    bEnableCabinet1 := BottleTransfer_Instance.bEnableCabinet1,
    bEnableCabinet2 := BottleTransfer_Instance.bEnableCabinet2,
    bEnableCabinet3 := BottleTransfer_Instance.bEnableCabinet3,
    bEnableCabinet4 := BottleTransfer_Instance.bEnableCabinet4,
    bEnableCabinet5 := BottleTransfer_Instance.bEnableCabinet5
);

// Map sequence outputs back to main program
IF BottleTransfer_Instance.bAutoMode THEN
    BottleTransfer_Instance.bGoToLabeler := BottleSequence.bGoToLabeler;
    BottleTransfer_Instance.bGoToCabinet1 := BottleSequence.bGoToCabinet1;
    BottleTransfer_Instance.bGoToCabinet2 := BottleSequence.bGoToCabinet2;
    BottleTransfer_Instance.bGoToCabinet3 := BottleSequence.bGoToCabinet3;
    BottleTransfer_Instance.bGoToCabinet4 := BottleSequence.bGoToCabinet4;
    BottleTransfer_Instance.bGoToCabinet5 := BottleSequence.bGoToCabinet5;
    BottleTransfer_Instance.bGoToCamera := BottleSequence.bGoToCamera;
    BottleTransfer_Instance.bGoToCapperFromLabeler := BottleSequence.bGoToCapper;
    BottleTransfer_Instance.bGoToCapperFromCabinet := BottleSequence.bGoToCapper;
    BottleTransfer_Instance.bGoToCapperFromCamera := BottleSequence.bGoToCapper;
    BottleTransfer_Instance.bOpenGripper := BottleSequence.bOpenGripper;
    BottleTransfer_Instance.bCloseGripper := BottleSequence.bCloseGripper;
    BottleTransfer_Instance.nMainSequenceStep := BottleSequence.nSequenceStep;
    BottleTransfer_Instance.bSequenceActive := BottleSequence.bSequenceActive;
END_IF

// Execute Main Bottle Transfer Program
BottleTransfer_Instance();

// Execute Diagnostics
Diagnostics(
    Axis := BottleTransfer_Instance.Axis_SV,
    bUpdateDiagnostics := TRUE
);

// Map program outputs to physical outputs
bServoEnable := BottleTransfer_Instance.bServoEnable;
bServoReset := BottleTransfer_Instance.bServoReset;
bOpenGripperOutput := BottleTransfer_Instance.bOpenGripperOutput;
bCloseGripperOutput := BottleTransfer_Instance.bCloseGripperOutput;

// Update HMI
UpdateHMI(ADR(BottleTransfer_Instance));

// ===== MANUAL MODE COMMAND MAPPING =====
IF BottleTransfer_Instance.bManualMode AND SafetyInterlocks.bMotionPermitted THEN
    
    // Manual position commands from HMI
    IF HMI_BottleTransfer.bGoToLabeler THEN
        BottleTransfer_Instance.bManualGoToLabeler := TRUE;
        HMI_BottleTransfer.bGoToLabeler := FALSE;
    END_IF
    
    IF HMI_BottleTransfer.bGoToCabinet1 THEN
        BottleTransfer_Instance.bManualGoToCabinet1 := TRUE;
        HMI_BottleTransfer.bGoToCabinet1 := FALSE;
    END_IF
    
    IF HMI_BottleTransfer.bGoToCabinet2 THEN
        BottleTransfer_Instance.bManualGoToCabinet2 := TRUE;
        HMI_BottleTransfer.bGoToCabinet2 := FALSE;
    END_IF
    
    IF HMI_BottleTransfer.bGoToCabinet3 THEN
        BottleTransfer_Instance.bManualGoToCabinet3 := TRUE;
        HMI_BottleTransfer.bGoToCabinet3 := FALSE;
    END_IF

    IF HMI_BottleTransfer.bGoToCabinet4 THEN
        BottleTransfer_Instance.bManualGoToCabinet4 := TRUE;
        HMI_BottleTransfer.bGoToCabinet4 := FALSE;
    END_IF
    IF HMI_BottleTransfer.bGoToCabinet5 THEN
        BottleTransfer_Instance.bManualGoToCabinet5 := TRUE;
        HMI_BottleTransfer.bGoToCabinet5 := FALSE;      
    END_IF
    IF HMI_BottleTransfer.bGoToCamera THEN
        BottleTransfer_Instance.bManualGoToCamera := TRUE;
        HMI_BottleTransfer.bGoToCamera := FALSE;
    END_IF
    IF HMI_BottleTransfer.bGoToCapper THEN
        BottleTransfer_Instance.bManualGoToCapper := TRUE;
        HMI_BottleTransfer.bGoToCapper := FALSE;            
    END_IF  
    IF HMI_BottleTransfer.bGoToHome THEN
        BottleTransfer_Instance.bManualGoToHome := TRUE;
        HMI_BottleTransfer.bGoToHome := FALSE;      
    END_IF  
END_IF
// ===== TEACHING MODE COMMAND MAPPING =====
IF BottleTransfer_Instance.bTeachMode AND SafetyInterlocks.bMotionPermitted THEN
    IF HMI_BottleTransfer.bSavePosition THEN
        TeachPosition(
            nPosition := HMI_BottleTransfer.nTeachPositionSelect,
            rValue := BottleTransfer_Instance.Axis_SV.NcToPlc.ActPos,
            b5CabinetSystem := BottleTransfer_Instance.bEnableCabinet5
        );
        HMI_BottleTransfer.bSavePosition := FALSE;
    END_IF  
END_IF
// ===== SCAN TIME MONITOR =====
TON_ScanTime(IN := TRUE, PT := T#100MS);    
IF TON_ScanTime.Q THEN
    // Scan time exceeded 100ms
    // Handle scan time fault (e.g., set system fault, log event)
    TON_ScanTime(IN := FALSE);  
ELSE
    TON_ScanTime(IN := FALSE);
END_IF
END_PROGRAM

// ===== MOVEMENT COMMAND EXECUTION =====
IF BottleTransfer_Instance.bMotionSequenceActive AND SafetyInterlocks.bMotionPermitted THEN
    // Determine target position based on command
    IF BottleTransfer_Instance.bGoToLabeler THEN
        nGoingToRegister := 1;
    ELSIF BottleTransfer_Instance.bGoToCabinet1 THEN
        nGoingToRegister := 2;
    ELSIF BottleTransfer_Instance.bGoToCabinet2 THEN
        nGoingToRegister := 3;
    ELSIF BottleTransfer_Instance.bGoToCabinet3 THEN
        nGoingToRegister := 4;
    ELSIF BottleTransfer_Instance.bGoToCabinet4 THEN
        nGoingToRegister := 5;
    ELSIF BottleTransfer_Instance.bGoToCabinet5 THEN
        nGoingToRegister := 6;
    ELSIF BottleTransfer_Instance.bGoToCamera THEN
        nGoingToRegister := 13;
    ELSIF BottleTransfer_Instance.bGoToCapperFromLabeler OR 
          BottleTransfer_Instance.bGoToCapperFromCabinet OR 
          BottleTransfer_Instance.bGoToCapperFromCamera THEN
        nGoingToRegister := 14;
    ELSIF BottleTransfer_Instance.bManualGoToHome THEN
        nGoingToRegister := 15;
    ELSE
        nGoingToRegister := 0; // No valid command
    END_IF
    
    // Validate position
    IF ValidatePosition(nPosition := nGoingToRegister, bEnableCabinet5 := BottleTransfer_Instance.bEnableCabinet5) THEN
        // Get target position value
        IF BottleTransfer_Instance.bEnableCabinet5 THEN
            R_POSITION := BottleTransfer_Instance.POSITION_5CAB[nGoingToRegister];
        ELSE
            R_POSITION := BottleTransfer_Instance.POSITION_4CAB[nGoingToRegister];
        END_IF
    ELSE
        // Invalid position command - abort move
        nGoingToRegister := 0;
        BottleTransfer_Instance.bMotionSequenceActive := FALSE;
    END_IF
    
    // Execute move if not already moving and a valid command exists
    IF NOT MC_MoveAbsolute.Execute AND (nGoingToRegister <> 0) AND NOT bDataTransferred THEN
        MC_MoveAbsolute(
            Axis := BottleTransfer_Instance.Axis_SV,
            Execute := TRUE,
            Position := R_POSITION,
            Velocity := 130.0,
            Acceleration := 533.0,
            Deceleration := 533.0
        );
        bDataTransferred := TRUE;
        BottleTransfer_Instance.bMotionSequenceActive := TRUE;
        BottleTransfer_Instance.nAtRegister := 0; // Unknown during move
        BottleTransfer_Instance.sServoStatus := 'Moving';
    END_IF
    // Monitor move completion
    IF MC_MoveAbsolute.Done THEN
        MC_MoveAbsolute(Execute := FALSE);
        BottleTransfer_Instance.bMotionSequenceActive := FALSE;
        BottleTransfer_Instance.nAtRegister := nGoingToRegister;
        nGoingToRegister := 0;
        bDataTransferred := FALSE;
        BottleTransfer_Instance.sServoStatus := 'In Position';
    ELSIF MC_MoveAbsolute.Error THEN
        MC_MoveAbsolute(Execute := FALSE);
        BottleTransfer_Instance.bMotionSequenceActive := FALSE;
        nGoingToRegister := 0;
        bDataTransferred := FALSE;
        BottleTransfer_Instance.sServoStatus := 'Error';
        BottleTransfer_Instance.bSystemFault := TRUE;
    END_IF
ELSE
    // If motion not permitted, abort any ongoing move
    IF MC_MoveAbsolute.Execute THEN
        MC_MoveAbsolute(Execute := FALSE);
        BottleTransfer_Instance.bMotionSequenceActive := FALSE;
        nGoingToRegister := 0;
        bDataTransferred := FALSE;
        BottleTransfer_Instance.sServoStatus := 'Aborted';
    END_IF      
    BottleTransfer_Instance.bMotionSequenceActive := FALSE;
    nGoingToRegister := 0;
    bDataTransferred := FALSE;
END_IF
// ===== GRIPPER COMMAND EXECUTION =====
IF BottleTransfer_Instance.bGripperSequenceActive AND SafetyInterlocks.bGripperOperationPermitted THEN
    // Execute gripper commands
    IF BottleTransfer_Instance.bGripperOpen THEN
        // Open gripper
        Gripper_Open();
    ELSIF BottleTransfer_Instance.bGripperClose THEN
        // Close gripper
        Gripper_Close();
    END_IF
END_IF
// Monitor gripper status
IF BottleTransfer_Instance.bGripperOpen THEN
    IF bGripperOpenSensor THEN
        BottleTransfer_Instance.bGripperOpen := FALSE;
        BottleTransfer_Instance.bGripperClosed := FALSE;
        BottleTransfer_Instance.bGripperSequenceActive := FALSE;
    END_IF                          
ELSIF BottleTransfer_Instance.bGripperClose THEN
    IF bGripperClosedSensor THEN    
        BottleTransfer_Instance.bGripperOpen := FALSE;
        BottleTransfer_Instance.bGripperClosed := TRUE;
        BottleTransfer_Instance.bGripperSequenceActive := FALSE;                        
    END_IF
END_IF              
ELSE
    // If gripper operation not permitted, abort any ongoing gripper action
    IF BottleTransfer_Instance.bGripperOpen OR BottleTransfer_Instance.bGripperClose THEN
        BottleTransfer_Instance.bGripperOpen := FALSE;
        BottleTransfer_Instance.bGripperClose := FALSE;
        BottleTransfer_Instance.bGripperSequenceActive := FALSE;
    END_IF      
    BottleTransfer_Instance.bGripperOpen := FALSE;
    BottleTransfer_Instance.bGripperClose := FALSE;
    BottleTransfer_Instance.bGripperSequenceActive := FALSE;    
END_IF
// ===== JOG COMMAND EXECUTION =====
IF BottleTransfer_Instance.bManualMode AND SafetyInterlocks.bJogPermitted THEN      
    // Execute jog commands
    IF BottleTransfer_Instance.bHMI_JogForward THEN
        // Check position limits before jogging forward
        IF CheckPositionLimits(rCurrentPosition := BottleTransfer_Instance.Axis_SV.NcToPlc.ActPos, bJogForward := TRUE, bJogReverse := FALSE) THEN
            MC_MoveVelocity(
                Axis := BottleTransfer_Instance.Axis_SV,
                Execute := TRUE,
                Velocity := 50.0,
                Acceleration := 200.0,
                Deceleration := 200.0
            );
            BottleTransfer_Instance.sServoStatus := 'Jogging Forward';
        ELSE
            // At forward limit - stop jog
            MC_MoveVelocity(Execute := FALSE);
            BottleTransfer_Instance.sServoStatus := 'At Forward Limit';
        END_IF
    ELSIF BottleTransfer_Instance.bHMI_JogReverse THEN
        // Check position limits before jogging reverse
        IF CheckPositionLimits(rCurrentPosition := BottleTransfer_Instance.Axis_SV.NcToPlc.ActPos, bJogForward := FALSE, bJogReverse := TRUE) THEN
            MC_MoveVelocity(
                Axis := BottleTransfer_Instance.Axis_SV,
                Execute := TRUE,
                Velocity := -50.0,
                Acceleration := 200.0,
                Deceleration := 200.0
            );
            BottleTransfer_Instance.sServoStatus := 'Jogging Reverse';
        ELSE
            // At reverse limit - stop jog
            MC_MoveVelocity(Execute := FALSE);
            BottleTransfer_Instance.sServoStatus := 'At Reverse Limit';
        END_IF
    ELSE
        // No jog command - stop any ongoing jog
        IF MC_MoveVelocity.Execute THEN
            MC_MoveVelocity(Execute := FALSE);
            BottleTransfer_Instance.sServoStatus := 'Jog Stopped';
        END_IF
    END_IF
ELSE
    // If jogging not permitted, stop any ongoing jog
    IF MC_MoveVelocity.Execute THEN
        MC_MoveVelocity(Execute := FALSE);
        BottleTransfer_Instance.sServoStatus := 'Jog Aborted';
    END_IF      
    BottleTransfer_Instance.bHMI_JogForward := FALSE;
    BottleTransfer_Instance.bHMI_JogReverse := FALSE;
    BottleTransfer_Instance.sServoStatus := 'Jog Not Permitted';
END_IF
// ===== HOMING SEQUENCE =====
IF BottleTransfer_Instance.bHMI_Home AND SafetyInterlocks.bMotionPermitted THEN
    IF NOT MC_Home.Execute AND NOT BottleTransfer_Instance.bMotionSequenceActive THEN
        // Start homing sequence
        MC_Home(
            Axis := BottleTransfer_Instance.Axis_SV,
            Execute := TRUE,
            HomeMode := MC_HOME_MODE_1, // Example home mode
            Velocity := 50.0,
            Acceleration := 200.0,
            Deceleration := 200.0
        );
        BottleTransfer_Instance.bHomingSequenceActive := TRUE;
        BottleTransfer_Instance.sServoStatus := 'Homing';
    END_IF  
    // Monitor homing completion
    IF MC_Home.Done THEN
        MC_Home(Execute := FALSE);
        BottleTransfer_Instance.bHomingSequenceActive := FALSE;
        BottleTransfer_Instance.nAtRegister := 15; // Home position
        BottleTransfer_Instance.sServoStatus := 'Homed';
    ELSIF MC_Home.Error THEN
        MC_Home(Execute := FALSE);
        BottleTransfer_Instance.bHomingSequenceActive := FALSE;
        BottleTransfer_Instance.sServoStatus := 'Homing Error';
        BottleTransfer_Instance.bSystemFault := TRUE;       
    END_IF
ELSE            
    // If homing not permitted, abort any ongoing homing
    IF MC_Home.Execute THEN
        MC_Home(Execute := FALSE);
        BottleTransfer_Instance.bHomingSequenceActive := FALSE;
        BottleTransfer_Instance.sServoStatus := 'Homing Aborted';
    END_IF      
    BottleTransfer_Instance.bHMI_Home := FALSE;
    BottleTransfer_Instance.bHomingSequenceActive := FALSE;     
END_IF
// ===== HMI UPDATE FUNCTION =====  
FUNCTION UpdateHMI : BOOL
VAR_INPUT
    pBottleTransfer : POINTER TO BottleTransfer;    
END_VAR
VAR
    nCurrentPos : INT;
END_VAR
// Update current position
nCurrentPos := pBottleTransfer^.nAtRegister;
pBottleTransfer^.rCurrentPositionMM := pBottleTransfer^.Axis_SV.NcToPlc.ActPos;
CASE nCurrentPos OF
    0: HMI_BottleTransfer.sCurrentPosition := 'Unknown';
    1: HMI_BottleTransfer.sCurrentPosition := 'Labeler';
    2: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 1';
    3: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 2';
    4: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 3';
    5: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 4';
    6: HMI_BottleTransfer.sCurrentPosition := 'Cabinet 5';      
    13: HMI_BottleTransfer.sCurrentPosition := 'Camera';
    14: HMI_BottleTransfer.sCurrentPosition := 'Capper';
    15: HMI_BottleTransfer.sCurrentPosition := 'Home';
    ELSE HMI_BottleTransfer.sCurrentPosition := 'Invalid';  
END_CASE
// Update target position
IF pBottleTransfer^.bMotionSequenceActive OR pBottleTransfer^.bHomingSequenceActive THEN
    HMI_BottleTransfer.rTargetPositionMM := pBottleTransfer^.Axis_SV.NcToPlc.SetPos;
ELSE
    HMI_BottleTransfer.rTargetPositionMM := pBottleTransfer^.rCurrentPositionMM;    
END_IF
// Update servo status
HMI_BottleTransfer.sServoStatus := pBottleTransfer^.sServoStatus;   
HMI_BottleTransfer.bServoEnabled := pBottleTransfer^.bServoEnabled;
HMI_BottleTransfer.bServoAtPrePosition := pBottleTransfer^.bServoAtPrePosition;
HMI_BottleTransfer.bServoInMotion := pBottleTransfer^.bMotionSequenceActive OR  pBottleTransfer^.bHomingSequenceActive;
HMI_BottleTransfer.bServoError := (pBottleTransfer^.Axis_SV.NcToPlc.ErrorCode <> 0);    
// Update gripper status
IF pBottleTransfer^.bGripperOpen THEN
    HMI_BottleTransfer.sGripperStatus := 'Open';
ELSE
    HMI_BottleTransfer.sGripperStatus := 'Closed';
END_IF  
IF pBottleTransfer^.bGripperSequenceActive THEN
    HMI_BottleTransfer.sGripperStatus := HMI_BottleTransfer.sGripperStatus + ' - Moving';       
END_IF
HMI_BottleTransfer.bGripperError := NOT (pBottleTransfer^.bGripperOpenSensor OR pBottleTransfer^.bGripperClosedSensor);
// Update mode status   
HMI_BottleTransfer.sModeStatus := pBottleTransfer^.sModeStatus;         
HMI_BottleTransfer.bAutoMode := pBottleTransfer^.bAutoMode;
HMI_BottleTransfer.bManualMode := pBottleTransfer^.bManualMode;
HMI_BottleTransfer.bTeachMode := pBottleTransfer^.bTeachMode;
// Update system status 
HMI_BottleTransfer.bSystemFault := pBottleTransfer^.bSystemFault;
HMI_BottleTransfer.bSystemReady := pBottleTransfer^.bSystemReady;
HMI_BottleTransfer.bControlPowerOn := pBottleTransfer^.bControlPowerOn;
HMI_BottleTransfer.bAirPressureOK := pBottleTransfer^.bAirPressureOK;
HMI_BottleTransfer.bCapperDoorClosed := pBottleTransfer^.bCapperDoorClosed;
HMI_BottleTransfer.bServoReady := pBottleTransfer^.bServoReady;     
HMI_BottleTransfer.bAtHomePosition := pBottleTransfer^.bAtHomePosition;
// Update status message and alarm
IF pBottleTransfer^.bSystemFault THEN
    HMI_BottleTransfer.sStatusMessage := 'System fault - check diagnostics';
    HMI_BottleTransfer.bAlarmActive := TRUE;        
ELSIF pBottleTransfer^.bHomingSequenceActive THEN
    HMI_BottleTransfer.sStatusMessage := 'Homing in progress';
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSIF pBottleTransfer^.bMotionSequenceActive THEN
    HMI_BottleTransfer.sStatusMessage := 'Moving to position';          
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSIF pBottleTransfer^.bGripperSequenceActive THEN
    HMI_BottleTransfer.sStatusMessage := 'Gripper moving';          
    HMI_BottleTransfer.bAlarmActive := FALSE;
ELSIF pBottleTransfer^.bSystemReady THEN            
    HMI_BottleTransfer.sStatusMessage := 'System ready';
    HMI_BottleTransfer.bAlarmActive := FALSE;        
ELSE
    HMI_BottleTransfer.sStatusMessage := 'System not ready - check interlocks';
    HMI_BottleTransfer.bAlarmActive := TRUE;        
END_IF
UpdateHMI := TRUE;      
END_FUNCTION
FUNCTION TeachPosition : BOOL
VAR_INPUT
    nPosition : INT; // Position number to teach (1-15)
    rValue : LREAL;  // Position value in mm    
END_VAR_INPUT
VAR_INPUT
    b5CabinetSystem : BOOL; // TRUE if 5 cabinet system, FALSE if 4 cabinet system
END_VAR_INPUT
VAR 
    bValidPosition : BOOL;                  
END_VAR 
// Validate position number
bValidPosition := ValidatePosition(nPosition := nPosition, bEnableCabinet5 := b5CabinetSystem);
IF bValidPosition THEN
    // Store position value
    IF b5CabinetSystem THEN
        BottleTransfer_Instance.POSITION_5CAB[nPosition] := rValue;
    ELSE
        BottleTransfer_Instance.POSITION_4CAB[nPosition] := rValue;
    END_IF
    TeachPosition := TRUE; // Successfully taught
ELSE
    TeachPosition := FALSE; // Invalid position number
END_IF
END_FUNCTION        

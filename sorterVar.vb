(*
====================================================================
BOTTLE SORTER - VARIABLE DECLARATION LIST
TwinCAT 3.1 Structured Text
Mapped from Allen-Bradley Ladder Logic
====================================================================
*)

VAR_GLOBAL
    
    // ========== DIGITAL INPUTS (Mapped from SLOT01 & SLOT02) ==========
    
    // Servo Position Feedback (Binary Encoded 1-7)
    bServoPositionMetBit1 AT %I*     : BOOL;    // Lane position bit 0
    bServoPositionMetBit2 AT %I*     : BOOL;    // Lane position bit 1
    bServoPositionMetBit3 AT %I*     : BOOL;    // Lane position bit 2
    
    // Servo Status Inputs
    bServoMoving AT %I*              : BOOL;    // Servo in motion
    bServoHomed AT %I*               : BOOL;    // HEND - Servo homed
    bServoInPosition AT %I*          : BOOL;    // PEND/WEND - Position reached
    bServoReady AT %I*               : BOOL;    // Servo drive ready
    bServoAlarm AT %I*               : BOOL;    // Servo alarm active
    
    // Bottle Lift Position Sensors
    bBottleLiftUp AT %I*             : BOOL;    // _14PXBOTTLE LIFT UP_j1108
    bBottleLiftDown AT %I*           : BOOL;    // _15PXBOTTLE LIFT DOWN_j1110
    bBottleLiftAtSlowDownPosition AT %I* : BOOL; // Transition point sensor
    
    // Lift Gripper Sensors
    bLiftGripperOpenAtSorter AT %I*  : BOOL;    // _13PXBOTTLE LIFT GRIPPER OPEN AT SORTER_j1106
    bLiftGripperOpenAtCapper AT %I*  : BOOL;    // _10PXBOTTLE LIFT GRIPPER OPEN AT CAPPER_J1017
    
    // Sorter Gate Sensors
    bSorterGateOpen AT %I*           : BOOL;    // _17PXBOTTLE SORTER GATE OPEN_j1117
    bSorterGateClosed AT %I*         : BOOL;    // _18PXBOTTLE SORTER GATE CLOSED_j1119
    bSorterLaneEmpty AT %I*          : BOOL;    // _9PEBOTTLE SORTER LANE EMPTY_j1121
    bSorterLaneInPosition AT %I*     : BOOL;    // _16PXBOTTLE SORTER LANE IN POSITION_j1115
    
    // Shared Inputs from Other Systems
    bCapperChuckAtCap AT %I*         : BOOL;    // From Capper system
    bBottleAtLift AT %I*             : BOOL;    // Bottle present sensor
    bTransferComplete AT %I*         : BOOL;    // From transfer/capper complete
    
    // System Status Inputs
    bOptifillMachineInAutoMode AT %I* : BOOL;   // B33[3].4
    bOptifillMachineInManualMode AT %I* : BOOL; // B33[3].2
    bAirSupplyOK AT %I*              : BOOL;    // _1PSAIR SUPPLY OK
    bControlPowerOn AT %I*           : BOOL;    // MCRCONTROL POWER ON
    bMachineResetEnable AT %I*       : BOOL;    // B33[4].0
    bFaultResetRequest AT %I*        : BOOL;    // B33[5].1
    bAutoSequenceEnable AT %I*       : BOOL;    // B33[6].1
    bCycleStart AT %I*               : BOOL;    // B33[6].0
    
    // HMI Manual Control Inputs (N11[180])
    bManualIndexToLane1 AT %I*       : BOOL;    // N11[180].0
    bManualIndexToLane2 AT %I*       : BOOL;    // N11[180].1
    bManualIndexToLane3 AT %I*       : BOOL;    // N11[180].2
    bManualIndexToLane4 AT %I*       : BOOL;    // N11[180].3
    bManualIndexToLane5 AT %I*       : BOOL;    // N11[180].4
    bManualIndexToLane6 AT %I*       : BOOL;    // N11[180].5
    bManualIndexToLane7 AT %I*       : BOOL;    // N11[180].6
    bManualResetSorter AT %I*        : BOOL;    // N11[180].7
    bManualOpenSorterGate AT %I*     : BOOL;    // N11[180].8
    bManualCloseSorterGate AT %I*    : BOOL;    // N11[180].9
    bManualHomeSorter AT %I*         : BOOL;    // N11[180].10
    
    // ========== DIGITAL OUTPUTS (Mapped to SLOT03) ==========
    
    // Servo Control Outputs (Binary Position Command)
    bServoPositionCmdBit1 AT %Q*     : BOOL;    // Lane command bit 0
    bServoPositionCmdBit2 AT %Q*     : BOOL;    // Lane command bit 1
    bServoPositionCmdBit3 AT %Q*     : BOOL;    // Lane command bit 2
    bServoOperationMode AT %Q*       : BOOL;    // 0=Normal, 1=Teaching
    bServoJISE AT %Q*                : BOOL;    // 0=Jog to Index, 1=Inching
    bServoJogForward AT %Q*          : BOOL;    // Manual jog forward
    bServoJogReverse AT %Q*          : BOOL;    // Manual jog reverse
    bServoHome AT %Q*                : BOOL;    // Home command
    bServoCSTR_PWRT AT %Q*           : BOOL;    // Start/Position Write
    bServoAlarmReset AT %Q*          : BOOL;    // Clear servo alarm
    
    // Bottle Lift Control Outputs
    bLowerBottleLiftSlow AT %Q*      : BOOL;    // SLOT03 O[1].4 - Slow descent
    bRaiseBottleLiftSlow AT %Q*      : BOOL;    // SLOT03 O[1].5 - Slow ascent
    bLowerBottleLiftFast AT %Q*      : BOOL;    // SLOT03 O[1].6 - Fast descent
    bRaiseBottleLiftFast AT %Q*      : BOOL;    // SLOT03 O[1].7 - Fast ascent
    
    // Lift Gripper Control (Shared with Capper)
    bOpenLiftGripperOutput AT %Q*    : BOOL;    // _7ASOLOPEN BOTTLE LIFT GRIPPER
    bCloseLiftGripperOutput AT %Q*   : BOOL;    // _7BSOLCLOSE BOTTLE LIFT GRIPPER
    
    // Sorter Gate Control
    bOpenSorterGateOutput AT %Q*     : BOOL;    // _15ASOLRETRACT BOTTLE SORTER GATE
    bCloseSorterGateOutput AT %Q*    : BOOL;    // _15BSOLEXTEND BOTTLE SORTER GATE
    
    // Sorter Motion Control (Pneumatic)
    bSorterMoveTowardLane7 AT %Q*    : BOOL;    // _13ASOLMOVE BOTTLE SORTER TOWARDS LANE 7
    bSorterMoveTowardLane1 AT %Q*    : BOOL;    // _13BSOLMOVE BOTTLE SORTER DESCENDING
    bSorterHomeBrakeRelease AT %Q*   : BOOL;    // _14SOLHOME/RELEASE BRAKE
    
    // ========== ANALOG/INTEGER VALUES ==========
    
    // Current Status Registers (Mapped from N187)
    iSorterCurrentLane               : INT;     // N187[0] - Current lane (1-7)
    iSorterLoadLane                  : INT;     // N187[1] - Load target lane
    iSorterUnloadLane                : INT;     // N187[2] - Unload target lane
    iSorterTargetLane                : INT;     // N187[3] - Commanded lane
    
    // Sorter Lane Assignment (Tote IDs per lane)
    aSorterLaneToteID                : ARRAY[1..7] OF INT;   // Tote ID assigned to each lane
    aSorterLaneBottleCount           : ARRAY[1..7] OF INT;   // Bottle count per lane
    aSorterLaneStatus                : ARRAY[1..7] OF INT;   // Status of each lane (e.g., empty, full)
    aSorterLanePriority               : ARRAY[1..7] OF INT;   // Priority of each lane (1-7, 7=Lowest)
    aSorterLanePosition                : ARRAY[1..7] OF INT;   // Position of each lane (1-7)

    // External Signals
    iBottleStatus AT %I*             : INT;     // N11[200] - 1=Pass, 2=RejectEmpty, 3=RejectFull
    iToteID AT %I*                   : INT;     // N11[201] - Current Tote ID from OFM      
    bUnloadSorterRequest AT %I*      : BOOL;    // N11[202] - Request to unload current lane
END_VAR_GLOBAL  

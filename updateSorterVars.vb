{attribute 'qualified_only'}
VAR_GLOBAL
    // ===========================
    // SORTER - STANDARD GVL NAMES
    // ===========================

    // --- Servo Position Feedback (inputs) ---
    bSorterServoPositionMetBit1     : BOOL;    // Position met bit 0 (feedback)
    bSorterServoPositionMetBit2     : BOOL;    // Position met bit 1
    bSorterServoPositionMetBit3     : BOOL;    // Position met bit 2

    // --- Servo Status Inputs ---
    bSorterServoMoving              : BOOL;    // servo moving
    bSorterServoHomed               : BOOL;    // HEND - servo homed
    bSorterServoInPosition          : BOOL;    // PEND/WEND - position reached / position write complete
    bSorterServoReady               : BOOL;    // servo drive ready
    bSorterServoAlarm               : BOOL;    // alarm active

    // --- Lift sensors / shared signals ---
    bCapperChuckAtCap               : BOOL;    // capper chuck at cap (shared)
    bLiftGripperOpenAtSorter        : BOOL;    // gripper open at sorter (shared with capper)
    bLiftGripperOpenAtCapper        : BOOL;    // gripper open at capper side
    bBottleLiftUp                   : BOOL;    // lift up sensor
    bBottleLiftDown                 : BOOL;    // lift down sensor
    bBottleLiftAtSlowDownPosition   : BOOL;    // lift slow-down transition position

    // --- Sorter gate / lane sensors ---
    bSorterGateOpen                 : BOOL;    // gate open input
    bSorterGateClosed               : BOOL;    // gate closed input
    bSorterLaneEmpty                : BOOL;    // lane empty sensor
    bSorterLaneInPosition           : BOOL;    // lane in position sensor

    // --- HMI / Manual (N11[180] etc) ---
    bManualIndexToLane1             : BOOL;
    bManualIndexToLane2             : BOOL;
    bManualIndexToLane3             : BOOL;
    bManualIndexToLane4             : BOOL;
    bManualIndexToLane5             : BOOL;
    bManualIndexToLane6             : BOOL;
    bManualIndexToLane7             : BOOL;
    bManualResetSorter              : BOOL;
    bManualOpenSorterGate           : BOOL;
    bManualCloseSorterGate          : BOOL;
    bManualHomeSorter               : BOOL;
    bManualHomeServo                : BOOL;    // manual servo home (HMI)

    // --- DIGITAL OUTPUTS (servo control) ---
    bSorterServoPositionCmdBit1     : BOOL;    // position command bit 0
    bSorterServoPositionCmdBit2     : BOOL;    // position command bit 1
    bSorterServoPositionCmdBit3     : BOOL;    // position command bit 2
    bSorterServoOperationMode       : BOOL;    // 0=Normal,1=Teaching
    bSorterServoJISE                : BOOL;    // Jog/Index vs Inching
    bSorterServoJogForward          : BOOL;    // jog forward output
    bSorterServoJogReverse          : BOOL;    // jog reverse output
    bSorterServoHome                : BOOL;    // home command output
    bSorterServoCSTR_PWRT           : BOOL;    // Start / Position Write pulse
    bSorterServoAlarmReset          : BOOL;    // servo alarm reset (output)

    // --- Lift gripper outputs (shared with capper) ---
    bOpenLiftGripperOutput          : BOOL;    // open gripper output (standard name)
    bCloseLiftGripperOutput         : BOOL;    // close gripper output (standard name)

    // --- Bottle lift motion outputs ---
    bLowerBottleLiftSlow            : BOOL;    // lower slow
    bRaiseBottleLiftSlow            : BOOL;    // raise slow
    bLowerBottleLiftFast            : BOOL;    // lower fast
    bRaiseBottleLiftFast            : BOOL;    // raise fast

    // --- Sorter gate outputs ---
    bOpenSorterGateOutput           : BOOL;    // open gate (retract)
    bCloseSorterGateOutput          : BOOL;    // close gate (extend)

    // --- Sorter motion / brake ---
    bSorterMoveTowardLane7          : BOOL;    // motion command toward lane 7 (if used)
    bSorterMoveTowardLane1          : BOOL;    // motion toward lane 1 (if used)
    bSorterHomeBrakeRelease         : BOOL;    // home / release brake output

    // ======================
    // Status / Sequence Vars
    // ======================
    iSorterStep                     : INT;     // main state machine step
    bSorterSequenceActive           : BOOL;    // sequence running flag
    bSorterSequenceComplete         : BOOL;    // sequence complete flag
    bLiftSequenceActive             : BOOL;    // lift sub-sequence running
    bLiftRaiseComplete              : BOOL;    // lift raise finished
    bLiftLowerComplete              : BOOL;    // lift lower finished
    bSorterInPosition               : BOOL;    // logical in-position for sorter (standard)
    bSorterMovingToPosition         : BOOL;    // moving toward target
    bSorterFault                     : BOOL;   // fault flag
    sSorterFaultMessage             : STRING(80); // fault message for HMI

    // Current / Target lane display
    iSorterCurrentLane              : INT;     // mapped register N187[0]
    iSorterLoadLane                 : INT;     // mapped register N187[1]
    iSorterUnloadLane               : INT;     // mapped register N187[2]
    iSorterTargetLane               : INT;     // mapped register N187[3]
    iTargetLane                     : INT;     // working target lane chosen by logic
    iCurrentLane                    : INT;     // working current lane value
    sTargetLaneName                 : STRING(32); // HMI friendly name for target (e.g., 'Lane 1' / 'Reject Lane')

    // ==================
    // Lane / Tote Arrays
    // ==================
    aSorterLaneToteID               : ARRAY[1..7] OF INT;   // Tote ID assigned to each lane (standard)
    aSorterLaneBottleCount          : ARRAY[1..7] OF INT;   // Bottle count per lane (standard)
    aSorterLaneStatus               : ARRAY[1..7] OF INT;   // Status per lane (optional)
    aSorterLanePriority             : ARRAY[1..7] OF INT;   // Priority per lane (optional)
    aSorterLanePosition             : ARRAY[1..7] OF INT;   // Position index per lane (optional)

    // External signals
    iToteID                         : INT;     // incoming Tote ID (N11[201])
    bUnloadSorterRequest            : BOOL;    // request to unload current lane (N11[202])

    // ===========
    // Timers (GVL standard names)
    // ===========
    tGripperOpenDelay               : TON;     // delay to wait for gripper open (PT = GRIPPER_OPEN_DELAY)
    tLiftDelayAfterOpen             : TON;     // delay after gripper open (PT = LIFT_DELAY_AFTER_OPEN)
    tSorterPositionTimeout          : TON;     // timeout waiting for sorter position (PT = SORTER_POSITION_TIMEOUT)
    tGateTimeout                    : TON;     // timeout waiting for gate open/close (PT = GATE_TIMEOUT)

    // Misc / helpers (optional)
    bSorterServoPositionCmdPulse    : BOOL;    // helper pulse if needed for CSTR/PWRT sequencing
END_VAR

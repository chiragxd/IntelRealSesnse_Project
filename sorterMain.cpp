PROGRAM BottleSorter
VAR
    // ========== INPUTS ==========
    // Servo Inputs
    bServoPositionMetBit1 : BOOL;
    bServoPositionMetBit2 : BOOL;
    bServoPositionMetBit3 : BOOL;
    bServoMoving : BOOL;
    bServoHomed : BOOL;              // HEND
    bServoInPosition : BOOL;         // PEND/WEND
    bServoReady : BOOL;
    bServoAlarm : BOOL;
    
    // Shared with Capper
    bCapperChuckAtCap : BOOL;
    
    // Lift Gripper Sensors
    bLiftGripperOpenAtSorter : BOOL;
    bLiftGripperOpenAtCapper : BOOL;
    
    // Sorter Gate Sensors
    bSorterGateOpen : BOOL;
    bSorterGateClose : BOOL;
    bSorterLaneEmpty : BOOL;
    
    // Lift Position Sensors
    bBottleLiftUp : BOOL;
    bBottleLiftDown : BOOL;
    bBottleLiftAtSlowDownPosition : BOOL;
    
    // System Inputs
    bOptifillMachineInAutoMode : BOOL;
    bOptifillMachineInManualMode : BOOL;
    bMachineResetEnable : BOOL;
    bFaultResetRequest : BOOL;
    bAirSupplyOK : BOOL;
    bControlPowerOn : BOOL;
    
    // Manual Control Requests
    bManualOpenSorterGate : BOOL;
    bManualCloseSorterGate : BOOL;
    bManualOpenLiftGripper : BOOL;
    bManualCloseLiftGripper : BOOL;
    bManualIndexToLane : ARRAY[1..7] OF BOOL;
    bManualResetSorter : BOOL;
    bManualHomeServo : BOOL;
    
    // External Signals
    iBottleStatus : INT;              // 1=Pass, 2=RejectEmpty, 3=RejectFull
    bTransferComplete : BOOL;         // From Capper
    bBottleAtLift : BOOL;
    iToteID : INT;                    // From OFM
    bUnloadSorterRequest : BOOL;
    
    // ========== OUTPUTS ==========
    // Servo Control Outputs
    bServoPositionCmdBit1 : BOOL;
    bServoPositionCmdBit2 : BOOL;
    bServoPositionCmdBit3 : BOOL;
    bServoOperationMode : BOOL;       // 0=Normal, 1=Teaching
    bServoJISE : BOOL;                // 0=JogToIndex, 1=Inching
    bServoJogForward : BOOL;
    bServoJogReverse : BOOL;
    bServoHome : BOOL;
    bServoCSTR_PWRT : BOOL;           // Start/Position Write
    bServoAlarmReset : BOOL;
    
    // Lift Control Outputs
    bLowerBottleLiftSlow : BOOL;
    bRaiseBottleLiftSlow : BOOL;
    bLowerBottleLiftFast : BOOL;
    bRaiseBottleLiftFast : BOOL;
    
    // Gripper Control Outputs
    bOpenLiftGripper : BOOL;
    bCloseLiftGripper : BOOL;
    
    // Gate Control Outputs
    bOpenSorterGateOutput : BOOL;
    bCloseSorterGateOutput : BOOL;
    
    // ========== INTERNAL VARIABLES ==========
    iSorterStep : INT := 0;
    iCurrentLane : INT := 1;          // Current lane position (1-7, 7=Reject)
    iTargetLane : INT := 1;           // Target lane position
    sTargetLaneName : STRING;
    
    bSorterSequenceActive : BOOL;
    bSorterSequenceComplete : BOOL;
    bSorterInPosition : BOOL;
    bSorterMovingToPosition : BOOL;
    
    bLiftSequenceActive : BOOL;
    bLiftRaiseComplete : BOOL;
    bLiftLowerComplete : BOOL;
    
    // Sorter Lane Assignment Array
    aSorterLaneAssignment : ARRAY[1..7] OF INT;  // Stores ToteID for each lane
    aSorterLaneCount : ARRAY[1..7] OF INT;       // Bottle count per lane
    
    // Timers
    tGripperOpenDelay : TON;
    tLiftDelayAfterOpen : TON;
    tSorterPositionTimeout : TON;
    tGateTimeout : TON;
    
    // Constants
    GRIPPER_OPEN_DELAY : TIME := T#200MS;
    LIFT_DELAY_AFTER_OPEN : TIME := T#500MS;
    SORTER_POSITION_TIMEOUT : TIME := T#10S;
    GATE_TIMEOUT : TIME := T#3S;
    
    // Status Flags
    bSorterFault : BOOL;
    bLiftFault : BOOL;
    sSorterFaultMessage : STRING;
    
END_VAR

// ========== GLOBAL ABORT ==========
IF bMachineResetEnable OR bFaultResetRequest OR bServoAlarm OR NOT bAirSupplyOK OR NOT bControlPowerOn THEN
    // Abort all sequences
    iSorterStep := 0;
    bSorterSequenceActive := FALSE;
    bSorterSequenceComplete := FALSE;
    bLiftSequenceActive := FALSE;
    bLiftRaiseComplete := FALSE;
    bLiftLowerComplete := FALSE;
    
    // Reset outputs
    bOpenLiftGripper := FALSE;
    bCloseLiftGripper := FALSE;
    bOpenSorterGateOutput := FALSE;
    bCloseSorterGateOutput := FALSE;
    bLowerBottleLiftSlow := FALSE;
    bRaiseBottleLiftSlow := FALSE;
    bLowerBottleLiftFast := FALSE;
    bRaiseBottleLiftFast := FALSE;
    
    // Reset timers
    tGripperOpenDelay(IN := FALSE);
    tLiftDelayAfterOpen(IN := FALSE);
    tSorterPositionTimeout(IN := FALSE);
    tGateTimeout(IN := FALSE);
    
    // Servo abort
    bServoJogForward := FALSE;
    bServoJogReverse := FALSE;
    bServoCSTR_PWRT := FALSE;
END_IF;

// ========== FAULT RESET ==========
IF bFaultResetRequest THEN
    bSorterFault := FALSE;
    bLiftFault := FALSE;
    bServoAlarmReset := TRUE;
ELSE
    bServoAlarmReset := FALSE;
END_IF;

// ========== MANUAL MODE ==========
IF bOptifillMachineInManualMode AND NOT bOptifillMachineInAutoMode THEN
    
    // Manual Servo Home
    IF bManualHomeServo THEN
        bServoHome := TRUE;
    ELSE
        bServoHome := FALSE;
    END_IF;
    
    // Manual Gripper Control
    IF bManualOpenLiftGripper THEN
        bOpenLiftGripper := TRUE;
        bCloseLiftGripper := FALSE;
    END_IF;
    
    IF bManualCloseLiftGripper THEN
        bCloseLiftGripper := TRUE;
        bOpenLiftGripper := FALSE;
    END_IF;
    
    // Manual Gate Control
    IF bManualOpenSorterGate THEN
        bOpenSorterGateOutput := TRUE;
        bCloseSorterGateOutput := FALSE;
    END_IF;
    
    IF bManualCloseSorterGate THEN
        bCloseSorterGateOutput := TRUE;
        bOpenSorterGateOutput := FALSE;
    END_IF;
    
    // Manual Lane Selection
    IF bManualIndexToLane[1] THEN
        iTargetLane := 1;
        sTargetLaneName := 'Lane 1';
    ELSIF bManualIndexToLane[2] THEN
        iTargetLane := 2;
        sTargetLaneName := 'Lane 2';
    ELSIF bManualIndexToLane[3] THEN
        iTargetLane := 3;
        sTargetLaneName := 'Lane 3';
    ELSIF bManualIndexToLane[4] THEN
        iTargetLane := 4;
        sTargetLaneName := 'Lane 4';
    ELSIF bManualIndexToLane[5] THEN
        iTargetLane := 5;
        sTargetLaneName := 'Lane 5';
    ELSIF bManualIndexToLane[6] THEN
        iTargetLane := 6;
        sTargetLaneName := 'Lane 6';
    ELSIF bManualIndexToLane[7] THEN
        iTargetLane := 7;
        sTargetLaneName := 'Reject Lane';
    END_IF;
    
    // Execute manual positioning
    IF iTargetLane > 0 AND iTargetLane <= 7 THEN
        SetServoPosition(iTargetLane);
    END_IF;
    
    // Manual Sorter Reset
    IF bManualResetSorter THEN
        ResetSorterLanes();
    END_IF;
    
END_IF;

// ========== AUTO MODE SEQUENCE ==========
IF bOptifillMachineInAutoMode AND NOT bOptifillMachineInManualMode THEN
    
    CASE iSorterStep OF
        
        // ===== STEP 0: IDLE - WAIT FOR BOTTLE =====
        0:
            IF bTransferComplete AND bBottleAtLift AND bServoReady AND bServoHomed THEN
                // Determine target lane based on bottle status
                DetermineLaneAssignment();
                
                IF iTargetLane > 0 THEN
                    bSorterSequenceActive := TRUE;
                    iSorterStep := 10;
                END_IF;
            END_IF;
        
        // ===== STEP 10: POSITION SORTER TO TARGET LANE =====
        10:
            IF NOT bSorterInPosition THEN
                SetServoPosition(iTargetLane);
                bSorterMovingToPosition := TRUE;
                iSorterStep := 20;
            ELSE
                iSorterStep := 30;  // Already in position
            END_IF;
        
        // ===== STEP 20: WAIT FOR SORTER IN POSITION =====
        20:
            tSorterPositionTimeout(IN := TRUE, PT := SORTER_POSITION_TIMEOUT);
            
            IF CheckServoPosition(iTargetLane) THEN
                iCurrentLane := iTargetLane;
                bSorterInPosition := TRUE;
                bSorterMovingToPosition := FALSE;
                tSorterPositionTimeout(IN := FALSE);
                iSorterStep := 30;
            ELSIF tSorterPositionTimeout.Q THEN
                // Position timeout fault
                bSorterFault := TRUE;
                sSorterFaultMessage := 'Sorter Position Timeout';
                iSorterStep := 999;
            END_IF;
        
        // ===== STEP 30: VERIFY LANE STATUS =====
        30:
            // Check if lane is ready for bottle
            IF (aSorterLaneCount[iTargetLane] = 0 AND bSorterLaneEmpty) OR 
               (aSorterLaneCount[iTargetLane] > 0 AND aSorterLaneAssignment[iTargetLane] = iToteID) THEN
                // Lane is ready
                iSorterStep := 40;
            ELSE
                // Lane conflict - should not happen with proper assignment
                bSorterFault := TRUE;
                sSorterFaultMessage := 'Lane Assignment Error';
                iSorterStep := 999;
            END_IF;
        
        // ===== STEP 40: RAISE BOTTLE LIFT =====
        40:
            IF bCapperChuckAtCap THEN
                bLiftSequenceActive := TRUE;
                bRaiseBottleLiftFast := TRUE;
                iSorterStep := 50;
            END_IF;
        
        // ===== STEP 50: SLOW DOWN AT TRANSITION =====
        50:
            IF bBottleLiftAtSlowDownPosition THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := TRUE;
                iSorterStep := 60;
            END_IF;
        
        // ===== STEP 60: WAIT FOR LIFT UP =====
        60:
            IF bBottleLiftUp THEN
                bRaiseBottleLiftSlow := FALSE;
                bLiftRaiseComplete := TRUE;
                iSorterStep := 70;
            END_IF;
        
        // ===== STEP 70: OPEN LIFT GRIPPER =====
        70:
            bOpenLiftGripper := TRUE;
            bCloseLiftGripper := FALSE;
            
            tGripperOpenDelay(IN := TRUE, PT := GRIPPER_OPEN_DELAY);
            
            IF bLiftGripperOpenAtSorter OR tGripperOpenDelay.Q THEN
                tGripperOpenDelay(IN := FALSE);
                iSorterStep := 80;
            END_IF;
        
        // ===== STEP 80: DELAY AFTER GRIPPER OPEN =====
        80:
            tLiftDelayAfterOpen(IN := TRUE, PT := LIFT_DELAY_AFTER_OPEN);
            
            IF tLiftDelayAfterOpen.Q THEN
                tLiftDelayAfterOpen(IN := FALSE);
                // Update lane count
                aSorterLaneCount[iTargetLane] := aSorterLaneCount[iTargetLane] + 1;
                IF aSorterLaneCount[iTargetLane] = 1 THEN
                    aSorterLaneAssignment[iTargetLane] := iToteID;
                END_IF;
                iSorterStep := 90;
            END_IF;
        
        // ===== STEP 90: LOWER BOTTLE LIFT =====
        90:
            bLowerBottleLiftFast := TRUE;
            iSorterStep := 100;
        
        // ===== STEP 100: SLOW DOWN AT TRANSITION =====
        100:
            IF bBottleLiftAtSlowDownPosition THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := TRUE;
                iSorterStep := 110;
            END_IF;
        
        // ===== STEP 110: WAIT FOR LIFT DOWN =====
        110:
            IF bBottleLiftDown AND bLiftGripperOpenAtCapper THEN
                bLowerBottleLiftSlow := FALSE;
                bLiftLowerComplete := TRUE;
                bLiftSequenceActive := FALSE;
                bSorterSequenceComplete := TRUE;
                iSorterStep := 0;  // Return to idle
            END_IF;
        
        // ===== STEP 200: UNLOAD SORTER LANE =====
        200:
            IF bUnloadSorterRequest THEN
                bOpenSorterGateOutput := TRUE;
                bCloseSorterGateOutput := FALSE;
                iSorterStep := 210;
            END_IF;
        
        // ===== STEP 210: WAIT FOR GATE OPEN =====
        210:
            tGateTimeout(IN := TRUE, PT := GATE_TIMEOUT);
            
            IF bSorterGateOpen THEN
                tGateTimeout(IN := FALSE);
                iSorterStep := 220;
            ELSIF tGateTimeout.Q THEN
                bSorterFault := TRUE;
                sSorterFaultMessage := 'Gate Open Timeout';
                iSorterStep := 999;
            END_IF;
        
        // ===== STEP 220: DELAY FOR BOTTLE DROP =====
        220:
            tLiftDelayAfterOpen(IN := TRUE, PT := T#2S);
            
            IF tLiftDelayAfterOpen.Q THEN
                tLiftDelayAfterOpen(IN := FALSE);
                iSorterStep := 230;
            END_IF;
        
        // ===== STEP 230: CLOSE GATE =====
        230:
            bCloseSorterGateOutput := TRUE;
            bOpenSorterGateOutput := FALSE;
            
            tGateTimeout(IN := TRUE, PT := GATE_TIMEOUT);
            
            IF bSorterGateClose THEN
                tGateTimeout(IN := FALSE);
                // Clear lane data
                aSorterLaneCount[iCurrentLane] := 0;
                aSorterLaneAssignment[iCurrentLane] := 0;
                iSorterStep := 0;
            ELSIF tGateTimeout.Q THEN
                bSorterFault := TRUE;
                sSorterFaultMessage := 'Gate Close Timeout';
                iSorterStep := 999;
            END_IF;
        
        // ===== STEP 999: FAULT STATE =====
        999:
            IF bFaultResetRequest THEN
                bSorterFault := FALSE;
                sSorterFaultMessage := '';
                iSorterStep := 0;
            END_IF;
        
        ELSE
            iSorterStep := 0;  // Safety fallback
    END_CASE;
    
END_IF;

// ========== HELPER FUNCTIONS ==========

// Determine Lane Assignment based on bottle status and tote ID
FUNCTION DetermineLaneAssignment : VOID
VAR
    i : INT;
    bLaneFound : BOOL := FALSE;
END_VAR

    CASE iBottleStatus OF
        1:  // Pass - Find lane with matching ToteID or next available
            // First check if ToteID already has a lane
            FOR i := 1 TO 6 DO
                IF aSorterLaneAssignment[i] = iToteID AND iToteID > 0 THEN
                    iTargetLane := i;
                    sTargetLaneName := CONCAT('Lane ', INT_TO_STRING(i));
                    bLaneFound := TRUE;
                    EXIT;
                END_IF;
            END_FOR;
            
            // If not found, find next empty lane
            IF NOT bLaneFound THEN
                FOR i := 1 TO 6 DO
                    IF aSorterLaneCount[i] = 0 THEN
                        iTargetLane := i;
                        sTargetLaneName := CONCAT('Lane ', INT_TO_STRING(i));
                        bLaneFound := TRUE;
                        EXIT;
                    END_IF;
                END_FOR;
            END_IF;
            
            IF NOT bLaneFound THEN
                bSorterFault := TRUE;
                sSorterFaultMessage := 'No Available Lane';
            END_IF;
        
        2, 3:  // RejectEmpty or RejectFull - Send to reject lane
            iTargetLane := 7;
            sTargetLaneName := 'Reject Lane';
        
        ELSE
            iTargetLane := 0;
    END_CASE;
END_FUNCTION

// Set Servo Position Command
FUNCTION SetServoPosition : VOID
VAR_INPUT
    iPosition : INT;
END_VAR

    // Convert position (1-7) to binary bits
    bServoPositionCmdBit1 := (iPosition AND 16#01) <> 0;
    bServoPositionCmdBit2 := (iPosition AND 16#02) <> 0;
    bServoPositionCmdBit3 := (iPosition AND 16#04) <> 0;
    
    // Pulse start command
    bServoCSTR_PWRT := TRUE;
END_FUNCTION

// Check if Servo is at target position
FUNCTION CheckServoPosition : BOOL
VAR_INPUT
    iPosition : INT;
END_VAR
VAR
    bBit1Match, bBit2Match, bBit3Match : BOOL;
END_VAR

    bBit1Match := ((iPosition AND 16#01) <> 0) = bServoPositionMetBit1;
    bBit2Match := ((iPosition AND 16#02) <> 0) = bServoPositionMetBit2;
    bBit3Match := ((iPosition AND 16#04) <> 0) = bServoPositionMetBit3;
    
    CheckServoPosition := bBit1Match AND bBit2Match AND bBit3Match AND bServoInPosition;
END_FUNCTION

// Reset all sorter lanes
FUNCTION ResetSorterLanes : VOID
VAR
    i : INT;
END_VAR

    FOR i := 1 TO 7 DO
        aSorterLaneCount[i] := 0;
        aSorterLaneAssignment[i] := 0;
    END_FOR;
END_FUNCTION

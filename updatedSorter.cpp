// ========== GLOBAL ABORT ==========
IF bMachineResetEnable OR bFaultResetRequest OR bSorterServoAlarm OR NOT bAirSupplyOK OR NOT bControlPowerOn THEN
    // Abort all sequences
    iSorterStep := 0;
    bSorterSequenceActive := FALSE;
    bSorterSequenceComplete := FALSE;
    bLiftSequenceActive := FALSE;
    bLiftRaiseComplete := FALSE;
    bLiftLowerComplete := FALSE;
    
    // Reset outputs
    bOpenLiftGripperOutput := FALSE;
    bCloseLiftGripperOutput := FALSE;
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
    bSorterServoJogForward := FALSE;
    bSorterServoJogReverse := FALSE;
    bSorterServoCSTR_PWRT := FALSE;
END_IF;

// ========== FAULT RESET ==========
IF bFaultResetRequest THEN
    bSorterFault := FALSE;
    bLiftFault := FALSE;
    bSorterServoAlarmReset := TRUE;
ELSE
    bSorterServoAlarmReset := FALSE;
END_IF;

// ========== MANUAL MODE ==========
IF bOptifillMachineInManualMode AND NOT bOptifillMachineInAutoMode THEN
    
    // Manual Servo Home
    IF bManualHomeServo THEN
        bSorterServoHome := TRUE;
    ELSE
        bSorterServoHome := FALSE;
    END_IF;
    
    // Manual Gripper Control
    IF bManualOpenLiftGripper THEN
        bOpenLiftGripperOutput := TRUE;
        bCloseLiftGripperOutput := FALSE;
    END_IF;
    
    IF bManualCloseLiftGripper THEN
        bCloseLiftGripperOutput := TRUE;
        bOpenLiftGripperOutput := FALSE;
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
    IF bManualIndexToLane1 THEN
        iTargetLane := 1;
        sTargetLaneName := 'Lane 1';
    ELSIF bManualIndexToLane2 THEN
        iTargetLane := 2;
        sTargetLaneName := 'Lane 2';
    ELSIF bManualIndexToLane3 THEN
        iTargetLane := 3;
        sTargetLaneName := 'Lane 3';
    ELSIF bManualIndexToLane4 THEN
        iTargetLane := 4;
        sTargetLaneName := 'Lane 4';
    ELSIF bManualIndexToLane5 THEN
        iTargetLane := 5;
        sTargetLaneName := 'Lane 5';
    ELSIF bManualIndexToLane6 THEN
        iTargetLane := 6;
        sTargetLaneName := 'Lane 6';
    ELSIF bManualIndexToLane7 THEN
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
            IF bTransferComplete AND bBottleAtLift AND bSorterServoReady AND bSorterServoHomed THEN
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
                iSorterCurrentLane := iTargetLane;
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
            IF (aSorterLaneBottleCount[iTargetLane] = 0 AND bSorterLaneEmpty) OR 
               (aSorterLaneBottleCount[iTargetLane] > 0 AND aSorterLaneToteID[iTargetLane] = iToteID) THEN
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
            bOpenLiftGripperOutput := TRUE;
            bCloseLiftGripperOutput := FALSE;
            
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
                aSorterLaneBottleCount[iTargetLane] := aSorterLaneBottleCount[iTargetLane] + 1;
                IF aSorterLaneBottleCount[iTargetLane] = 1 THEN
                    aSorterLaneToteID[iTargetLane] := iToteID;
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
            
            IF bSorterGateClosed THEN
                tGateTimeout(IN := FALSE);
                // Clear lane data
                aSorterLaneBottleCount[iSorterCurrentLane] := 0;
                aSorterLaneToteID[iSorterCurrentLane] := 0;
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
                IF aSorterLaneToteID[i] = iToteID AND iToteID > 0 THEN
                    iTargetLane := i;
                    sTargetLaneName := CONCAT('Lane ', INT_TO_STRING(i));
                    bLaneFound := TRUE;
                    EXIT;
                END_IF;
            END_FOR;
            
            // If not found, find next empty lane
            IF NOT bLaneFound THEN
                FOR i := 1 TO 6 DO
                    IF aSorterLaneBottleCount[i] = 0 THEN
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
    bSorterServoPositionCmdBit1 := (iPosition AND 16#01) <> 0;
    bSorterServoPositionCmdBit2 := (iPosition AND 16#02) <> 0;
    bSorterServoPositionCmdBit3 := (iPosition AND 16#04) <> 0;
    
    // Pulse start command
    bSorterServoCSTR_PWRT := TRUE;
END_FUNCTION

// Check if Servo is at target position
FUNCTION CheckServoPosition : BOOL
VAR_INPUT
    iPosition : INT;
END_VAR
VAR
    bBit1Match, bBit2Match, bBit3Match : BOOL;
END_VAR

    bBit1Match := ((iPosition AND 16#01) <> 0) = bSorterServoPositionMetBit1;
    bBit2Match := ((iPosition AND 16#02) <> 0) = bSorterServoPositionMetBit2;
    bBit3Match := ((iPosition AND 16#04) <> 0) = bSorterServoPositionMetBit3;
    
    CheckServoPosition := bBit1Match AND bBit2Match AND bBit3Match AND bSorterServoInPosition;
END_FUNCTION

// Reset all sorter lanes
FUNCTION ResetSorterLanes : VOID
VAR
    i : INT;
END_VAR

    FOR i := 1 TO 7 DO
        aSorterLaneBottleCount[i] := 0;
        aSorterLaneToteID[i] := 0;
    END_FOR;
END_FUNCTION

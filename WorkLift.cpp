(*
Bottle LIFT – Auto sequence with speed transition + Manual bits (via capper HMI for gripper).
- Uses existing door / safety / position inputs.
- Interlocked with Capper (chuck at cap side) before raising.
- Speed change at the “speed transition” prox both up and down.
*)

VAR
    // ===== Internal State / Helpers =====
    iLiftStep           : INT := 0;         // state machine
    tOpenGripDly        : TON;              // delay after opening gripper
    tRaiseTmo           : TON;              // up timeout
    tLowerTmo           : TON;              // down timeout
    tGripOpenTmo        : TON;              // confirm open timeout
    bFaultLift          : BOOL;             // local fault flag

    // convenience aliases
    bAuto    : BOOL := bOptifillMachineInAutoMode;     // B33[3].4
    bManual  : BOOL := bOptifillMachineInManualMode;   // B33[3].2
END_VAR


// ---------------------
// Manual (gripper only)
// ---------------------
// You already have the capper HMI gripper command bit:
//   bLiftGripperOpenCmd  (GVL capper section)
// Map it here so either station can show same action.
IF bManual THEN
    // Manual gripper open/close (open cmd dominates; otherwise stay as-is)
    IF bLiftGripperOpenCmd THEN
        bOpenBottleLiftGripper := TRUE;
        bCloseBottleLiftGripper := FALSE;
    END_IF;
    // Manual raise/lower of the lift isn’t required per sorter HMI spec,
    // so we leave raise/lower to Auto here.
END_IF;


// ------------------------------------
// Auto: Full Lift cycle (Sorter <-> Capper)
// ------------------------------------
IF bAuto AND bAutoSequenceEnable THEN

    CASE iLiftStep OF

        0:  // Idle: prerequisites for a new cycle
            bFaultLift := FALSE;
            tRaiseTmo(IN := FALSE);
            tLowerTmo(IN := FALSE);
            tGripOpenTmo(IN := FALSE);
            tOpenGripDly(IN := FALSE);

            // All safeties closed (doors) + air/power + servo panel ready
            IF bBottleAtLift                                  // bottle present at lift
               AND bCapperDoorClosed AND bSorterDoorClosed    // doors closed
               AND bControlPowerAndAirOn AND bSafetyOK THEN   // system OK

                // The capper chuck must be AT CAP side before we raise the lift (ladder shows at-cap check)
                IF bBottleCapperChuckHeadAtCapEscapement THEN
                    // Start raising (fast first)
                    bRaiseBottleLiftFast := TRUE;
                    bRaiseBottleLiftSlow := FALSE;
                    bLowerBottleLiftFast := FALSE;
                    bLowerBottleLiftSlow := FALSE;

                    tRaiseTmo(IN := TRUE, PT := T#6s);
                    iLiftStep := 10;
                END_IF;
            END_IF;

        10: // Raising: switch to slow at “speed transition”, then wait for UP
            IF bBottleLiftAtSpeedTransition THEN
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := TRUE;      // slow finish
            END_IF;

            IF bBottleLiftUp THEN                  // UP_j1108
                // Stop motion
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := FALSE;
                tRaiseTmo(IN := FALSE);
                // Verify sorter lane really ready (belt & prox); sorter PRG did this too
                IF bBottleSorterLaneInPosition THEN
                    // Open gripper to drop bottle into lane
                    bOpenBottleLiftGripper := TRUE;
                    bCloseBottleLiftGripper := FALSE;

                    // confirm open within timeout, then small settle delay
                    tGripOpenTmo(IN := TRUE, PT := T#2s);
                    iLiftStep := 20;
                ELSE
                    // lane not ready -> fault to avoid dangling bottle above lane
                    bFaultLift := TRUE;
                    iLiftStep := 900;
                END_IF;
            ELSIF tRaiseTmo.Q THEN
                // didn’t reach UP in time
                bRaiseBottleLiftFast := FALSE;
                bRaiseBottleLiftSlow := FALSE;
                bFaultLift := TRUE;
                iLiftStep := 900;
            END_IF;

        20: // Verify gripper open, short dwell, then go down fast
            IF bBottleLiftGripperOpenAtSorter THEN
                tGripOpenTmo(IN := FALSE);
                tOpenGripDly(IN := TRUE, PT := T#300ms);

                IF tOpenGripDly.Q THEN
                    tOpenGripDly(IN := FALSE);
                    // Lower (fast first)
                    bLowerBottleLiftFast := TRUE;
                    bLowerBottleLiftSlow := FALSE;
                    bRaiseBottleLiftFast := FALSE;
                    bRaiseBottleLiftSlow := FALSE;

                    tLowerTmo(IN := TRUE, PT := T#6s);
                    iLiftStep := 30;
                END_IF;
            ELSIF tGripOpenTmo.Q THEN
                // open confirm failed
                tGripOpenTmo(IN := FALSE);
                bFaultLift := TRUE;
                iLiftStep := 900;
            END_IF;

        30: // Lowering: switch to slow at transition, then wait for DOWN
            IF bBottleLiftAtSpeedTransition THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := TRUE;
            END_IF;

            IF bBottleLiftDown THEN                // DOWN_j1110
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := FALSE;
                tLowerTmo(IN := FALSE);

                // All done; signal clear when gripper is open at capper (handoff zone free)
                IF bBottleLiftGripperOpenAtCapper THEN
                    iLiftStep := 0;                // cycle complete
                ELSE
                    // we’re down but capper not open yet – just wait here safely
                END_IF;
            ELSIF tLowerTmo.Q THEN
                bLowerBottleLiftFast := FALSE;
                bLowerBottleLiftSlow := FALSE;
                bFaultLift := TRUE;
                iLiftStep := 900;
            END_IF;

        900: // Fault: stop all lift solenoids and wait for system reset
            bRaiseBottleLiftFast := FALSE;
            bRaiseBottleLiftSlow := FALSE;
            bLowerBottleLiftFast := FALSE;
            bLowerBottleLiftSlow := FALSE;
            // gripper remains as-is; operator decides next action
            ; // no-op

    END_CASE
ELSE
    // Auto disabled -> ensure motion outputs are off
    bRaiseBottleLiftFast := FALSE;
    bRaiseBottleLiftSlow := FALSE;
    bLowerBottleLiftFast := FALSE;
    bLowerBottleLiftSlow := FALSE;
END_IF;

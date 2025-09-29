VAR_GLOBAL
    // ----- Inputs (likely already in your GVL) -----
    bCapTrackHighLevel         : BOOL;   // cap track high level
    bCapTrackLowLevel          : BOOL;   // cap track low level
    bCapStufferDown            : BOOL;   // stuffer down prox
    bCapperChuckAtCapSide      : BOOL;   // assembly at cap side
    bCapperChuckAtLiftSide     : BOOL;   // assembly at lift side
    bCapperChuckHigh           : BOOL;   // chuck up
    bCapperChuckDown           : BOOL;   // chuck down
    bCapOnBottle               : BOOL;   // confirm cap on bottle (sensor)
    bBottleAtLift              : BOOL;   // bottle present at lift (handoff area)
    iBottleStatus              : INT;    // 1=Pass, 2=RejectEmpty, 3=RejectFull

    // Safeties / run conditions (these should already exist)
    bMCR_PowerOn               : BOOL;
    bCapperDoorClosed          : BOOL;
    bSorterDoorClosed          : BOOL;
    bAirSupplyOK               : BOOL;
    bSafetyOK                  : BOOL;

    // Modes / control
    bAutoMode                  : BOOL;   // auto enable
    bManualMode                : BOOL;   // manual mode
    bCycleStartOneShot         : BOOL;   // HMI cycle start pulse
    bResetEnable               : BOOL;   // HMI reset/fault reset

    // ----- Outputs (some may already exist; keep only missing ones) -----
    // pneumatics
    bExtendCapperChuckToLift   : BOOL;   // extend assembly to lift side
    bRetractCapperChuckToCap   : BOOL;   // retract assembly to cap side
    bLowerCapperChuck          : BOOL;   // lower chuck (down solenoid)
    bRaiseCapperChuck          : BOOL;   // raise chuck (up solenoid) – if hardware has separate up valve
    bRaiseCapStuffer           : BOOL;   // raise cap stuffer
    // motors
    bStartCapHopperMotor       : BOOL;   // cap hopper motor (keep-full)
    bStartCapperMotor          : BOOL;   // ***capper torque motor*** (THIS was missing)
    bEngageCapperClutch        : BOOL;   // clutch to transmit torque
    // (if you have a disengage output, add: bDisengageCapperClutch : BOOL)

    // ----- HMI MANUAL (likely already have some; add only if needed) -----
    bHMI_RaiseStuffer          : BOOL;
    bHMI_ExtendChuckToLift     : BOOL;
    bHMI_RetractChuckToCap     : BOOL;
    bHMI_LowerChuck            : BOOL;
    bHMI_RaiseChuck            : BOOL;
    bHMI_StartCapperMotor      : BOOL;
    bHMI_StopCapperMotor       : BOOL;
    bHMI_EngageClutch          : BOOL;
    bHMI_DisengageClutch       : BOOL;
    bHMI_StartCapHopper        : BOOL;
    bHMI_StopCapHopper         : BOOL;

    // ----- Timers (IEC TON; add to GVL if you centralize timers) -----
    tCapTrackNotFullDly        : TON;    // start hopper after not-full
    tCapTrackFullDly           : TON;    // stop hopper after full
    tStufferRaiseHold          : TON;    // keep stuffer raised for a moment
    tChuckDownDwell            : TON;    // dwell at down (apply torque time)
    tMotorSpinup               : TON;    // spin motor before clutch
    tClutchEngageDly           : TON;    // engage clutch delay
    tStepTimeout               : TON;    // generic timeout per step

    // ----- Params (tweak from HMI) -----
    nTrackNotFull_ms           : UDINT := 1500; // delay to start hopper
    nTrackFull_ms              : UDINT := 800;  // delay to stop hopper
    nStufferRaise_ms           : UDINT := 1200;
    nMotorSpinup_ms            : UDINT := 400;
    nClutchEngage_ms           : UDINT := 250;
    nChuckDownDwell_ms         : UDINT := 1200;
    nStepTimeout_ms            : UDINT := 5000;

    // ----- Status / faults -----
    bCapperFault               : BOOL := FALSE;
    bCapOnBottleError          : BOOL := FALSE;  // failed cap check at end
END_VAR

PROGRAM PRG_BottleCapper
VAR
    iCapperStep : INT := 0;   // state machine (auto)
    bPermissive : BOOL;       // safety/run permissive
END_VAR

// ======================================================
// PERMISSIVES (doors, air, safety, power)
// ======================================================
bPermissive :=
    bMCR_PowerOn AND
    bSafetyOK AND
    bCapperDoorClosed AND
    bSorterDoorClosed AND
    bAirSupplyOK;

// ======================================================
// CAP HOPPER KEEP-FULL (Auto)  — (like ladder T164)
// ======================================================
IF bAutoMode AND NOT bManualMode AND bPermissive THEN
    // start when NOT full persists
    tCapTrackNotFullDly(IN := (NOT bCapTrackHighLevel AND bCapTrackLowLevel), PT := UDINT_TO_TIME(nTrackNotFull_ms));
    IF tCapTrackNotFullDly.Q THEN
        bStartCapHopperMotor := TRUE;
    END_IF;

    // stop when FULL persists
    tCapTrackFullDly(IN := bCapTrackHighLevel, PT := UDINT_TO_TIME(nTrackFull_ms));
    IF tCapTrackFullDly.Q THEN
        bStartCapHopperMotor := FALSE;
    END_IF;
END_IF;

// Manual hopper override
IF bManualMode AND bPermissive THEN
    IF bHMI_StartCapHopper THEN bStartCapHopperMotor := TRUE END_IF;
    IF bHMI_StopCapHopper  THEN bStartCapHopperMotor := FALSE END_IF;
END_IF;

// Safety fallback
IF NOT bPermissive THEN
    bStartCapHopperMotor := FALSE;
END_IF;


// ======================================================
// MANUAL MODE — direct device control
// ======================================================
IF bManualMode AND bPermissive THEN
    // Stuffer
    bRaiseCapStuffer := bHMI_RaiseStuffer;

    // Assembly position
    bExtendCapperChuckToLift := bHMI_ExtendChuckToLift;
    bRetractCapperChuckToCap := bHMI_RetractChuckToCap;

    // Chuck up/down
    bLowerCapperChuck := bHMI_LowerChuck;
    bRaiseCapperChuck := bHMI_RaiseChuck;   // if separate up valve exists

    // Motor & clutch
    IF bHMI_StartCapperMotor THEN bStartCapperMotor := TRUE END_IF;
    IF bHMI_StopCapperMotor  THEN bStartCapperMotor := FALSE END_IF;

    bEngageCapperClutch := bHMI_EngageClutch;
    // if you have discrete disengage output:
    // bDisengageCapperClutch := bHMI_DisengageClutch;
END_IF;

// Safety fallback in manual too
IF NOT bPermissive THEN
    bRaiseCapStuffer := FALSE;
    bExtendCapperChuckToLift := FALSE;
    bRetractCapperChuckToCap := FALSE;
    bLowerCapperChuck := FALSE;
    bRaiseCapperChuck := FALSE;
    bStartCapperMotor := FALSE;
    bEngageCapperClutch := FALSE;
END_IF;


// ======================================================
// AUTO MODE — BOTTLE CAPPING SEQUENCE (from requirements)
// ======================================================
IF bAutoMode AND NOT bManualMode THEN

    CASE iCapperStep OF

        // --------------------------------------------------
        // 0: Idle – wait for bottle and status
        // --------------------------------------------------
        0:
            bCapperFault := FALSE;
            bCapOnBottleError := FALSE;

            // Reset outputs to safe defaults
            bExtendCapperChuckToLift := FALSE;
            bRetractCapperChuckToCap := FALSE;
            bLowerCapperChuck := FALSE;
            bRaiseCapperChuck := FALSE;
            bEngageCapperClutch := FALSE;

            IF bPermissive AND bCycleStartOneShot THEN
                // If bottle rejected empty → subsystem complete (skip capping)
                IF iBottleStatus = 2 THEN
                    // RejectEmptyBottle path: do nothing here
                    iCapperStep := 0;     // capper not used
                ELSE
                    // Require bottle at lift to begin capping (Pass or RejectFull)
                    IF bBottleAtLift THEN
                        // Ensure assembly is at CAP side & chuck up
                        IF bCapperChuckAtCapSide AND bCapperChuckHigh THEN
                            iCapperStep := 10;
                        ELSE
                            // drive back to cap side & chuck up
                            bRetractCapperChuckToCap := TRUE;
                            bExtendCapperChuckToLift := FALSE;
                            bLowerCapperChuck := FALSE;
                            bRaiseCapperChuck := TRUE;
                            // timeout guarding the move
                            tStepTimeout(IN := TRUE, PT := UDINT_TO_TIME(nStepTimeout_ms));
                            iCapperStep := 5;
                        END_IF;
                    END_IF;
                END_IF;
            END_IF;

        // 5: Wait to reach CAP side + chuck up
        5:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF (bCapperChuckAtCapSide AND bCapperChuckHigh) THEN
                // stop driving
                bRetractCapperChuckToCap := FALSE;
                bRaiseCapperChuck := FALSE;
                tStepTimeout(IN := FALSE);
                iCapperStep := 10;
            ELSIF tStepTimeout.Q THEN
                bCapperFault := TRUE;     // position not reached
                iCapperStep := 900;
            END_IF;

        // --------------------------------------------------
        // 10: If track low & chuck up & at cap side -> Raise stuffer
        // --------------------------------------------------
        10:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSE
                IF (bCapTrackLowLevel AND bCapperChuckHigh AND bCapperChuckAtCapSide) THEN
                    bRaiseCapStuffer := TRUE;
                    tStufferRaiseHold(IN := TRUE, PT := UDINT_TO_TIME(nStufferRaise_ms));
                    iCapperStep := 20;
                ELSE
                    // If high level ok, you may skip raising stuffer — go pick/move
                    iCapperStep := 25;
                END_IF;
            END_IF;

        // 20: Hold stuffer up for a moment, then release
        20:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF tStufferRaiseHold.Q THEN
                bRaiseCapStuffer := FALSE;               // let it fall back to DOWN
                tStufferRaiseHold(IN := FALSE);
                // Wait until stuffer-down is seen (mechanical settle)
                tStepTimeout(IN := TRUE, PT := UDINT_TO_TIME(nStepTimeout_ms));
                iCapperStep := 22;
            END_IF;

        // 22: Wait for stuffer down feedback
        22:
            IF bCapStufferDown THEN
                tStepTimeout(IN := FALSE);
                iCapperStep := 25;
            ELSIF tStepTimeout.Q OR NOT bPermissive THEN
                bCapperFault := TRUE;
                iCapperStep := 900;
            END_IF;

        // --------------------------------------------------
        // 25: Move assembly to LIFT side + start capper motor (spin-up)
        // --------------------------------------------------
        25:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSE
                bExtendCapperChuckToLift := TRUE;
                bRetractCapperChuckToCap := FALSE;

                bStartCapperMotor := TRUE;                              // Motor ON
                tMotorSpinup(IN := TRUE, PT := UDINT_TO_TIME(nMotorSpinup_ms));
                tStepTimeout(IN := TRUE, PT := UDINT_TO_TIME(nStepTimeout_ms));
                iCapperStep := 30;
            END_IF;

        // 30: Wait assembly at LIFT side; after spin-up, engage clutch
        30:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF bCapperChuckAtLiftSide THEN
                IF tMotorSpinup.Q THEN
                    bEngageCapperClutch := TRUE;                         // clutch in
                    tClutchEngageDly(IN := TRUE, PT := UDINT_TO_TIME(nClutchEngage_ms));
                    tMotorSpinup(IN := FALSE);
                    iCapperStep := 40;
                END_IF;
            ELSIF tStepTimeout.Q THEN
                bCapperFault := TRUE;    // couldn’t reach lift side
                iCapperStep := 900;
            END_IF;

        // --------------------------------------------------
        // 40: Lower chuck; dwell while torquing
        // --------------------------------------------------
        40:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF tClutchEngageDly.Q THEN
                bLowerCapperChuck := TRUE;       // chuck down
                bRaiseCapperChuck := FALSE;
                tChuckDownDwell(IN := TRUE, PT := UDINT_TO_TIME(nChuckDownDwell_ms));
                tStepTimeout(IN := TRUE, PT := UDINT_TO_TIME(nStepTimeout_ms));
                iCapperStep := 50;
            END_IF;

        // 50: Wait chuck down + dwell complete, then raise
        50:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF bCapperChuckDown AND tChuckDownDwell.Q THEN
                // Raise the chuck
                bLowerCapperChuck := FALSE;
                bRaiseCapperChuck := TRUE;
                tChuckDownDwell(IN := FALSE);
                tStepTimeout(IN := TRUE, PT := UDINT_TO_TIME(nStepTimeout_ms));
                iCapperStep := 60;
            ELSIF tStepTimeout.Q THEN
                bCapperFault := TRUE;    // didn’t reach down/dwell
                iCapperStep := 900;
            END_IF;

        // --------------------------------------------------
        // 60: Chuck up reached → retract to CAP side; drop clutch/motor
        // --------------------------------------------------
        60:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF bCapperChuckHigh THEN
                // disengage torque path & stop motor as we leave bottle
                bEngageCapperClutch := FALSE;
                bStartCapperMotor := FALSE;

                // move back to cap side
                bExtendCapperChuckToLift := FALSE;
                bRetractCapperChuckToCap := TRUE;
                bRaiseCapperChuck := FALSE;

                tStepTimeout(IN := TRUE, PT := UDINT_TO_TIME(nStepTimeout_ms));
                iCapperStep := 70;
            END_IF;

        // 70: At CAP side? then verify cap-on-bottle → complete or alarm
        70:
            IF NOT bPermissive THEN
                iCapperStep := 900;
            ELSIF bCapperChuckAtCapSide THEN
                bRetractCapperChuckToCap := FALSE;

                IF bCapOnBottle THEN
                    // Success
                    iCapperStep := 0;
                ELSE
                    // Error – no cap
                    bCapOnBottleError := TRUE;
                    iCapperStep := 900;
                END_IF;
            ELSIF tStepTimeout.Q THEN
                bCapperFault := TRUE;    // didn’t return to CAP side
                iCapperStep := 900;
            END_IF;

        // --------------------------------------------------
        // 900: FAULT – stop everything and wait reset
        // --------------------------------------------------
        900:
            // Stop drives/air outputs to safe state
            bExtendCapperChuckToLift := FALSE;
            bRetractCapperChuckToCap := FALSE;
            bLowerCapperChuck := FALSE;
            bRaiseCapperChuck := FALSE;
            bEngageCapperClutch := FALSE;
            bStartCapperMotor := FALSE;

            // clear timers
            tCapTrackNotFullDly(IN := FALSE);
            tCapTrackFullDly(IN := FALSE);
            tStufferRaiseHold(IN := FALSE);
            tChuckDownDwell(IN := FALSE);
            tMotorSpinup(IN := FALSE);
            tClutchEngageDly(IN := FALSE);
            tStepTimeout(IN := FALSE);

            // Wait operator reset
            IF bResetEnable AND bCycleStartOneShot AND bPermissive THEN
                bCapperFault := FALSE;
                bCapOnBottleError := FALSE;
                iCapperStep := 0;
            END_IF;

    END_CASE;
END_IF; // auto


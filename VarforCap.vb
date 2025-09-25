VAR
    //----------------------------------------------------------------------
    // PHYSICAL INPUTS (from I/O, mapped to ladder addresses)
    //----------------------------------------------------------------------
    CapTrackHighLevel     : BOOL;   // B53[0].0  (Cap Track High)
    CapTrackLowLevel      : BOOL;   // B53[0].1  (Cap Track Low)
    CapStufferDown        : BOOL;   // B53[0].2  (Cap Stuffer Down)
    ChuckAtCapSide        : BOOL;   // B53[0].3  (Chuck at Cap Side)
    ChuckHigh             : BOOL;   // B53[0].4  (Chuck High)
    ChuckAtLiftSide       : BOOL;   // B53[0].5  (Chuck at Lift Side)
    ChuckDown             : BOOL;   // B53[0].6  (Chuck Down)
    CapOnBottle           : BOOL;   // B53[0].7  (Cap On Bottle Sensor)
    BottleAtLift          : BOOL;   // B53[1].0  (Bottle Present at Lift)
    TransferComplete      : BOOL;   // B53[1].1  (Bottle transfer complete)

    //----------------------------------------------------------------------
    // PHYSICAL OUTPUTS (to I/O, mapped to ladder addresses)
    //----------------------------------------------------------------------
    LiftGripperOpenOutput   : BOOL;   // O[0].0   (Open Lift Gripper Solenoid)
    LiftGripperCloseOutput  : BOOL;   // O[0].1   (Close Lift Gripper Solenoid)
    ChuckExtendToLiftOutput : BOOL;   // O[0].2   (Extend Chuck to Lift)
    ChuckRetractToCapOutput : BOOL;   // O[0].3   (Retract Chuck to Cap Side)
    ChuckLowerOutput        : BOOL;   // O[0].4   (Lower Chuck)
    ChuckRaiseOutput        : BOOL;   // O[0].5   (Raise Chuck, if separate valve)
    CapStufferRaiseOutput   : BOOL;   // O[0].6   (Raise Cap Stuffer)
    CapHopperMotorOutput    : BOOL;   // O[0].7   (Cap Hopper Motor ON)
    TransferGripperOpen     : BOOL;   // O[1].0   (Open Transfer Gripper, shared)

    //----------------------------------------------------------------------
    // AUTO SEQUENCE BITS (from ladder sequence registers)
    //----------------------------------------------------------------------
    InsertCapIntoChuckCmd   : BOOL;   // B33[13].0 (Request: Insert cap into chuck)
    LiftGripperCloseCmd     : BOOL;   // B33[13].1 (Request: Close lift gripper)
    TransferGripperOpenCmd  : BOOL;   // B33[13].2 (Request: Open transfer gripper)
    ChuckMoveToLiftCmd      : BOOL;   // B33[13].3 (Request: Chuck move to lift)
    ChuckMoveToCapCmd       : BOOL;   // B33[13].4 (Request: Chuck move to cap side)
    ChuckLowerCmd           : BOOL;   // B33[13].5 (Request: Chuck lower)
    ChuckRaiseCmd           : BOOL;   // B33[13].6 (Request: Chuck raise)
    CapStufferRaiseCmd      : BOOL;   // B33[13].7 (Request: Raise cap stuffer)
    CapCheckComplete        : BOOL;   // B133[10].0 (Sequence complete)
    CapCheckError           : BOOL;   // B133[10].1 (Cap missing / error)

    //----------------------------------------------------------------------
    // HMI COMMANDS
    //----------------------------------------------------------------------
    AutoMode                : BOOL;   // B33[3].4 (Auto)
    ManualMode              : BOOL;   // B33[3].2 (Manual)
    CycleStart              : BOOL;   // B33[2].8 (Cycle Start)
    ResetRequest            : BOOL;   // B33[5].1 (Fault Reset / Sequence Reset)

    //----------------------------------------------------------------------
    // STATE MACHINE
    //----------------------------------------------------------------------
    CapperStep              : INT := 0;   // N137[10] (Capper sequence step)
    CapperError             : BOOL;       // B133[11].0 (Capper error flag)

    //----------------------------------------------------------------------
    // TIMERS (converted from ladder TONs)
    //----------------------------------------------------------------------
    tCapHopperDelay         : TON;    // Delay to start hopper motor
    tStufferRaiseDelay      : TON;    // Time for stuffer raise
    tChuckLowerDelay        : TON;    // Time for chuck lower
    tChuckRaiseDelay        : TON;    // Time for chuck raise
    tCapCheckDelay          : TON;    // Cap check debounce
END_VAR

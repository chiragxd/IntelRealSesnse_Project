// Bottle Lift Internal Control
bBottleLiftCommandFastUp     : BOOL;   // Sequence request: raise lift fast
bBottleLiftCommandSlowUp     : BOOL;   // Sequence request: raise lift slow
bBottleLiftCommandFastDown   : BOOL;   // Sequence request: lower lift fast
bBottleLiftCommandSlowDown   : BOOL;   // Sequence request: lower lift slow

// Bottle Lift Status
bBottleLiftRaising           : BOOL;   // Lift currently moving upward
bBottleLiftLowering          : BOOL;   // Lift currently moving downward
bBottleLiftReady             : BOOL;   // Lift idle/ready for next command
iLiftStep                    : INT;    // Lift state machine step counter

// Sorter Internal Control
bSorterSequenceFault         : BOOL;   // Sorter fault flag (timeout, misposition)
bSorterLaneSelected          : BOOL;   // Lane target chosen successfully

// HMI Manual Lift Control
bLiftManualRaise             : BOOL;   // HMI command to jog lift up
bLiftManualLower             : BOOL;   // HMI command to jog lift down

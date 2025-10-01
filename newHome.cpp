PROGRAM PRG_Homing
VAR
    Axis        : AXIS_REF; 

    fbPower     : MC_Power;
    fbMoveVel   : MC_MoveVelocity;
    fbStop      : MC_Stop;
    fbSetPos    : MC_SetPosition;

    bStartHoming : BOOL;   
    bSensor      : BOOL;   
    bHomed       : BOOL;

    step : INT := 0;
    bTrig : BOOL; // used for rising edges
END_VAR


CASE step OF
    0: // Idle
        bHomed := FALSE;
        fbMoveVel.Execute := FALSE;
        fbStop.Execute := FALSE;
        fbSetPos.Execute := FALSE;
        IF bStartHoming THEN
            step := 10;
        END_IF;

    10: // Power axis
        fbPower(Axis := Axis, Enable := TRUE);
        IF fbPower.Status THEN
            step := 20;
        END_IF;

    20: // Move right until sensor ON
        fbMoveVel(
            Axis := Axis,
            Execute := TRUE,
            Velocity := 50.0,
            Direction := MC_Positive_Direction
        );
        IF bSensor THEN
            fbMoveVel.Execute := FALSE;   // stop velocity command
            fbStop(Axis := Axis, Execute := TRUE, Deceleration := 100.0);
            step := 25;
        END_IF;

    25: // Ensure stop finished before moving left
        fbStop.Execute := FALSE;
        step := 30;

    30: // Move left until sensor OFF
        fbMoveVel(
            Axis := Axis,
            Execute := TRUE,
            Velocity := 50.0,
            Direction := MC_Negative_Direction
        );
        IF NOT bSensor THEN
            fbMoveVel.Execute := FALSE;
            fbStop(Axis := Axis, Execute := TRUE, Deceleration := 100.0);
            step := 35;
        END_IF;

    35: // Ensure stop finished before setting position
        fbStop.Execute := FALSE;
        step := 40;

    40: // Set home = 0.0
        fbSetPos(Axis := Axis, Execute := TRUE, Position := 0.0);
        IF fbSetPos.Done THEN
            fbSetPos.Execute := FALSE;
            bHomed := TRUE;
            step := 0;
        END_IF;
END_CASE

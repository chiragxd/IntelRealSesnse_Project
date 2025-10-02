PROGRAM PRG_Homing
VAR
    Axis        : AXIS_REF;

    fbPower     : MC_Power;
    fbMoveRel   : MC_MoveRelative;
    fbStop      : MC_Stop;
    fbSetPos    : MC_SetPosition;

    bStartHoming : BOOL;   // Trigger from HMI
    bSensor      : BOOL;   // Home sensor input
    bHomed       : BOOL;

    step : INT := 0;
END_VAR


CASE step OF
    0: // Idle
        bHomed := FALSE;
        fbMoveRel.Execute := FALSE;
        fbStop.Execute := FALSE;
        fbSetPos.Execute := FALSE;

        IF bStartHoming THEN
            step := 10;
        END_IF;

    10: // Power on axis
        fbPower(Axis := Axis, Enable := TRUE);
        IF fbPower.Status THEN
            step := 20;
        END_IF;

    20: // Move positive a long distance
        fbMoveRel(
            Axis := Axis,
            Execute := TRUE,
            Distance := 1000.0,        // big distance so it will definitely hit sensor
            Velocity := 50.0,
            Acceleration := 200.0,
            Deceleration := 200.0
        );
        IF bSensor THEN
            fbMoveRel.Execute := FALSE;   // cancel relative move
            fbStop(Axis := Axis, Execute := TRUE, Deceleration := 100.0);
            step := 25;
        END_IF;

    25: // Wait for stop
        IF fbStop.Done THEN
            fbStop.Execute := FALSE;
            step := 30;
        END_IF;

    30: // Move negative a long distance
        fbMoveRel(
            Axis := Axis,
            Execute := TRUE,
            Distance := -1000.0,       // big negative distance
            Velocity := 50.0,
            Acceleration := 200.0,
            Deceleration := 200.0
        );
        IF NOT bSensor THEN
            fbMoveRel.Execute := FALSE;
            fbStop(Axis := Axis, Execute := TRUE, Deceleration := 100.0);
            step := 35;
        END_IF;

    35: // Wait for stop
        IF fbStop.Done THEN
            fbStop.Execute := FALSE;
            step := 40;
        END_IF;

    40: // Set home position = 0
        fbSetPos(
            Axis := Axis,
            Execute := TRUE,
            Position := 0.0
        );
        IF fbSetPos.Done THEN
            fbSetPos.Execute := FALSE;
            bHomed := TRUE;
            step := 0;  // done
        END_IF;
END_CASE

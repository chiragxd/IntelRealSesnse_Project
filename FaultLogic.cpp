i := 0;
BufferIndex := 0;

FOR i := 0 TO 999 BY 1 DO 
	
IF (GVL.Faults[i].Active = TRUE) THEN
	GVL.ActiveFault := TRUE;
	GVL.NoActiveFault := FALSE;
	
	IF ( BufferIndex < 10)THEN
	FaultBuffer[BufferIndex] := GVL.Faults[i].Message;
	BufferIndex := BufferIndex +1;
	END_IF
	
END_IF

END_FOR

FOR j := BufferIndex TO 9 BY 1 DO
	
FaultBuffer[j] := '';

END_FOR


IsBufferEmpty := TRUE;
FOR j:=0 TO 9 BY 1 DO
	IF(FaultBuffer[j] <> '') THEN
	IsBufferEmpty := FALSE;
	EXIT;	
	END_IF
END_FOR

IF(IsBufferEmpty = TRUE) THEN
	GVL.ActiveFault := FALSE;
	GVL.NoActiveFault := TRUE;
	MultiFaultIndex :=0;
	StartTimer := FALSE;
END_IF

IF (GVL.ActiveFault = TRUE ) THEN
	StartTimer := TRUE;
END_IF

Label : Waittime(IN:=StartTimer,PT := T#5.0S);


GVL.HMIFaultMessage := FaultBuffer[MultiFaultIndex];


IF(waittime.q = TRUE) THEN
	StartTimer := FALSE;
	
		IF(MultiFaultindex >=9 ) OR (FaultBuffer[MultiFaultindex+1] = '') THEN
			MultiFaultindex :=0;
		ELSE
			MultiFaultindex := MultiFaultindex + 1;
		END_IF
		
		JMP Label;
	
END_IF


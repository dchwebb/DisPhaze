*!*	? TwosComp(0xff77)
*!*	return
*	Will read in a mono 16bit sample
m.SampleFile = GETFILE("wav")
IF EMPTY(m.SampleFile)
	RETURN
ENDIF

m.f = FOPEN(m.SampleFile)

*	Check 'data' section exists
FSEEK(m.f, 0x48)
m.check = FREAD(m.f, 4)

IF m.check != "data"
	FCLOSE(m.f)
	RETURN MESSAGEBOX("Not a wave file")
ENDIF

m.c = ""

susp

*	Move to start of data section
FSEEK(m.f, 0x50)
DO WHILE !FEOF(m.f)
	m.a = FREAD(m.f, 2)
	m.val = TwosComp((ASC(RIGHT(m.a, 1)) * 256) + ASC(m.a))
	m.c = m.c + TRANSFORM(m.val) + ","
ENDDO

FCLOSE(m.f)
_CLIPTEXT = m.c

******************
PROCEDURE TwosComp
LPARAMETERS m.n

IF !BITTEST(m.n, 15)
	RETURN m.n
ENDIF

FOR m.x = 0 TO 15
	IF BITTEST(m.n, m.x)
		m.n = BITCLEAR(m.n, m.x)
	ELSE
		m.n = BITSET(m.n, m.x)
	ENDIF
ENDFOR
RETURN -1 * (m.n + 1)
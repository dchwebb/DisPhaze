m.c = ""
FOR m.x = 50 TO 1 STEP - 1
	m.c = m.c + "-" + TRANSFORM((m.x / 50) * 32000) + CHR(13)
ENDFOR
_CLIPTEXT = m.c


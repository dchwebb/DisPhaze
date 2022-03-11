*	Function to generate a 12bit sine wave lookup table
CLEAR
m.c = ""
m.n = 109
FOR m.x = 0 TO m.n - 1
	m.s = ROUND(2047 * (SIN(m.x * 2 * PI() / m.n) + 1), 0)
	m.c = m.c + TRANSFORM(m.s) + ", "
	*? m.s
ENDFOR
_CLIPTEXT = m.c


*	Function to derive 12bit ADC voltage reading to CV pitch
CLEAR
m.c = ""
FOR m.v = 0 TO 4095 STEP 4
	m.s = ROUND(1568 * (2 ^ (v / -582)), 0)
	m.c = m.c + TRANSFORM(m.s) + ", "
	? m.s
ENDFOR
_CLIPTEXT = m.c


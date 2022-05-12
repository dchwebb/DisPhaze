CREATE CURSOR compression (in N(10,5), out N(10,5), cmp N(10,5), look_up N(10,0))
m.s = ""
m.oldCmp = 0
FOR m.x = 1 TO 200
	m.lvl = m.x / 100
	m.cmpStart = 0.70
	
	DO CASE 
	CASE m.lvl < m.cmpStart
		INSERT INTO compression (in, out) VALUES (m.lvl, m.lvl)
*!*		CASE m.lvl > 1.36
*!*			INSERT INTO compression (in, out) VALUES (m.lvl, 1.0)
	OTHERWISE
	
		m.cmp = 1 - (m.lvl - m.cmpStart) * 0.41

		INSERT INTO compression (in, out, cmp) VALUES (m.lvl, m.cmp * m.lvl, m.cmp)
	ENDCASE
	
	m.s = m.s + TRANSFORM(compression.out) + CHR(13)
	
	m.oldCmp = compression.out
	

ENDFOR
		
_CLIPTEXT = m.s


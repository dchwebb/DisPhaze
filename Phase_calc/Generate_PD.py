# Script to convert a set of samples for a single cycle wave into a sine wave phase distortion set of points
import math
import sys
from pathlib import Path
from xml.etree.ElementTree import PI

lutSize = 1024
argCount = len(sys.argv)

# first parameter is script name
if (argCount == 1):
	print("Command line usage: Generate_PD.py [sample file path] [LUT size default 1024]")
	sys.exit()
fileName = sys.argv[1]

if (argCount > 2):
	lutSize = int(sys.argv[2])

outputFileName = "output/" + Path(fileName).stem + "_PD.txt"

sampleFile = open(fileName, "r")			# Open file containing samples as text list
samples = sampleFile.read()
sampleFile.close()
sampleList = samples.split(',')				# Split the samples into a list
sampleCount = samples.count(",")

pdPlot = ""									# list of interpolated input samples (for debugging)
outputPD = ""								# final output of PD samples

for x in range(lutSize):
	samplePos = sampleCount * (x / lutSize)
	posScale = (x / lutSize)
	
	#	Interpolate current and next sample and convert from 16 bit to -1 to + 1
	s1 = int(sampleList[round(samplePos)])

	# set last sample to zero
	if (round(samplePos) + 1 == sampleCount):
		s2 = 0
	else:
		s2 = int(sampleList[round(samplePos) + 1])
	
	y = s1 + ((s2 - s1) * (posScale - round(posScale, 0)))
	y = y / 34000
	pdPlot = pdPlot + str(round(y, 6)) + ','

	#	Locate point on sine wave that most closely matches distortion wave
	sin1 = math.asin(y) / (2 * math.pi)
	sin0 = sin1 - 1
	sin2 = 0.5 - sin1
	sin3 = sin1 + 1

	#	Pick closest sine wav point to previous value
	sin = 999
	if (abs(sin0  - posScale) <= abs(sin  - posScale)):
		sin = sin0
	if (abs(sin1  - posScale) <= abs(sin  - posScale)):
		sin = sin1
	if (abs(sin2  - posScale) <= abs(sin  - posScale)):
		sin = sin2
	if (abs(sin3  - posScale) <= abs(sin  - posScale)):
		sin = sin3
			
	pd = sin - posScale
	outputPD = outputPD + str(round(pd, 6)) + ","

print("Output phase distortion array data to " + outputFileName)
sampleFile = open(outputFileName, "w")			# Open file to write output PD samples
sampleFile.write(outputPD)
sampleFile.close()

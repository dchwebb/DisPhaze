# Script to extract samples from a mono 16 bit wave
import sys
import struct
from pathlib import Path
from numpy import true_divide

argCount = len(sys.argv)

# first parameter is script name
if (argCount == 1):
	print("Usage: pass wave file path in command line")
	sys.exit()

fileName = sys.argv[1]

wavFile = open(fileName, "rb")
outputFileName = "output/" + Path(fileName).stem + ".txt"

wavFile.seek(0, 2)                                     # move the cursor to the end of the file
rawDataSize = wavFile.tell()                           # Get the file size

sampleCount = 0
outputData = ""

wavFile.seek(0x48, 0)                                   # Go to 'data' marker to check if valid wave file
data = wavFile.read(4)

if (data != b'data'):
	print("Not a valid wave file")
else:
	wavFile.seek(0x50, 0)                               # Go to beginning of data section
	while True:
		sample = wavFile.read(2)
		if not sample:
			break

		sampleCount += 1
		signed16bit = str(struct.unpack('<h', sample)[0])	# converts bytes to signed 16-bit value
		outputData += str(signed16bit) + ","

wavFile.close()

if (sampleCount > 0):
	print("Found " + str(sampleCount) + " Samples. Saved file to: " + outputFileName)
	sampleFile = open(outputFileName, "w")			# Open file to write output text samples
	sampleFile.write(outputData)
	sampleFile.close()

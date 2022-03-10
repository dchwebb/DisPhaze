# Script to extract samples from a mono 16 bit wave
import sys
from pathlib import Path



argCount = len(sys.argv)

# first parameter is script name
if (argCount == 1):
	print("Usage: pass wave file path in command line")
	sys.exit()

fileName = sys.argv[1]
print("File: ", fileName)

wavFile = open(fileName, "rb")
#OutputFile = Path(wavFile).stem

wavFile.seek(0, 2)                                     # move the cursor to the end of the file
rawDataSize = wavFile.tell()                           # Get the file size

wavFile.close()
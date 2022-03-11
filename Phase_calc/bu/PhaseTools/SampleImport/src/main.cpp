#include <windows.h>
#include <Commdlg.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>

void toClipboard(const std::string &s) {
	OpenClipboard(0);
	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s.size());
	if (!hg) {
		CloseClipboard();
		return;
	}
	memcpy(GlobalLock(hg), s.c_str(), s.size());
	GlobalUnlock(hg);
	SetClipboardData(CF_TEXT, hg);
	CloseClipboard();
	GlobalFree(hg);
}

OPENFILENAME ofn;
char szFile[100];		// a memory buffer to contain the file name

//nt WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
int main()
{
	// open a file name
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = szFile;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "Wav\0*.wav\0All\0*.*\0\0";
	ofn.nFilterIndex = 1;
	//ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	GetOpenFileName(&ofn);

	if (ofn.lpstrFile) {
		std::ifstream stream(ofn.lpstrFile, std::ios::binary);
		std::string line;

		char buffer[4];
		stream.seekg(0x48);
		stream.read(buffer, 4);
		if (strncmp(buffer, "data", 4) != 0) {
			std::cout << "Not a wav file\n";
			MessageBox(NULL, "Not a wav file", "File Name", MB_OK);
			return 0;
		}

		// get length of file:
		stream.seekg(0, stream.end);
		int length = stream.tellg();
		stream.seekg(0, stream.beg);
		std::cout << "Length: " << length << std::endl;

		int ReadState = 0;
		short DecimalSample = 0;

		// Move to start of data section
		length = 0;
		stream.seekg(0x50);
		char sample[2];
		std::stringstream ss;		// output string
		while (!stream.eof())
		{
			length++;
			stream.read(sample, 2);
			DecimalSample = *(short*)sample;
			//DecimalSample = ((unsigned char)sample[1] << 8) + (unsigned char)sample[0];

			ss << DecimalSample << ",";
		}
		std::cout << "Number of samples: " << length << std::endl << std::endl;
		std::string s = ss.str();
		std::cout << s;
		std::replace(s.begin(), s.end(), ',', '\n');
		toClipboard(s);;
	}

	std::cin.get();
}

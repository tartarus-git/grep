#define _CRT_SECURE_NO_WARNINGS		// TODO: See if you can get away with not doing this.

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

// TODO: Make sure the preprocessor defs in the setup menu are ordered, looks kind of weird right now.

#include <iostream>

#ifdef PLATFORM_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#include <cstring>
#endif

#include <vector>

#define ANSI_ESCAPE_CODE_PREFIX "\033["
#define ANSI_ESCAPE_CODE_SUFFIX "m"
#define ANSI_ESCAPE_CODE_MIN_SIZE ((sizeof(ANSI_ESCAPE_CODE_PREFIX) - 1) + (sizeof(ANSI_ESCAPE_CODE_SUFFIX) - 1))

// POSIX functions
#ifdef PLATFORM_WINDOWS
#define isatty(x) _isatty(x)
#define fileno(x) _fileno(x)
#endif

const char* helpText = "grep takes a series of lines as input and outputs lines from input that have the specified string in them.\n" \
					   "\n" \
					   "usage: grep <search string>";

bool usingColors = false;

namespace color {
	char* red;
	char* reset;

	//const char* red = ANSI_ESCAPE_CODE_PREFIX "31" ANSI_ESCAPE_CODE_SUFFIX;
	//const char* green  = ANSI_ESCAPE_CODE_PREFIX "32" ANSI_ESCAPE_CODE_SUFFIX;
	//const char* reset = ANSI_ESCAPE_CODE_PREFIX "0" ANSI_ESCAPE_CODE_SUFFIX;
}

void releaseColorStrings() {
	delete[] color::red;
	delete[] color::reset;
}

namespace format {
	char* error;
	char* endl;
}

void releaseFormatStrings() {
	delete[] format::error;
	delete[] format::endl;
}

void releaseIOStyleStrings() {
	releaseFormatStrings();
	releaseColorStrings();
}

// Shows help text.
void showHelp() {
	std::cout << helpText << std::endl;
	releaseIOStyleStrings();
	exit(EXIT_SUCCESS);
}

char* targetString;
size_t targetString_len;

// args validation
void manageArgs(int argc, char** argv) {
	size_t actualLen;
	switch (argc) {
	case 1:
		std::cout << format::error << "too few arguments" << format::endl;
		showHelp();
	case 2:
		targetString_len = strlen(argv[1]);
		actualLen = targetString_len + 1;
		targetString = new char[actualLen];
		memcpy(targetString, argv[1], actualLen);
		return;
	default:
		std::cout << format::error << "too many arguments" << format::endl;
		releaseIOStyleStrings();
		exit(EXIT_SUCCESS);
	}
}

bool isTargetInString(const char* string, size_t stringLen, const char* target, size_t targetLen) {
	if (targetLen > stringLen) { return false; }

	for (int i = 0; i <= stringLen - targetLen; i++) {
		if (!strncmp(string + i, target, targetLen)) {			// Why the hell do I have to put ! in front of this. Make sure this is the right way to do it.
			return true;
		}
	}
	return false;
}

std::string getLine() {			// TODO: This needs to be better.
	std::vector<char> buffer;
	while (true) {
		int tempchar = getchar();
		if (tempchar < 0) {
			return "\0";
		}
		if (tempchar == '\n') {
			if (buffer.size() == 0) {
				continue;					// Skip all the empty lines because our system can't handle those anywhere other than here for now.
			}
			break;
		}
		buffer.push_back(tempchar);
	}
	buffer.push_back('\0');

	char* anotherBuffer = new char[buffer.size()];
	memcpy(anotherBuffer, buffer.data(), buffer.size());
	
	std::string result = anotherBuffer;
	delete[] anotherBuffer;
	return result;
}

int main(int argc, char** argv) {
	// Only enable virtual terminal processing if stdout is a tty. If we're piping, don't do that.
	if (isatty(fileno(stdout))) {
		usingColors = true;

#ifdef PLATFORM_WINDOWS			// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping.
		// If we pipe, we need it disabled and we don't need ANSI key codes, so only do this when we don't pipe.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { return EXIT_FAILURE; }
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { return EXIT_FAILURE; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { return EXIT_FAILURE; }
#endif

		color::red = new char[ANSI_ESCAPE_CODE_MIN_SIZE + 2 + 1];
		memcpy(color::red, ANSI_ESCAPE_CODE_PREFIX "31" ANSI_ESCAPE_CODE_SUFFIX, ANSI_ESCAPE_CODE_MIN_SIZE + 2 + 1);

		color::reset = new char[ANSI_ESCAPE_CODE_MIN_SIZE + 1 + 1];
		memcpy(color::reset, ANSI_ESCAPE_CODE_PREFIX "0" ANSI_ESCAPE_CODE_SUFFIX, ANSI_ESCAPE_CODE_MIN_SIZE + 1 + 1);

		// Allocate format strings.
		format::error = new char[ANSI_ESCAPE_CODE_MIN_SIZE + 2 + 7 + 1];
		memcpy(format::error, color::red, ANSI_ESCAPE_CODE_MIN_SIZE + 2);
		memcpy(format::error + ANSI_ESCAPE_CODE_MIN_SIZE + 2, "ERROR: ", 8);	// This copies the null termination character as well because of 8.

		format::endl = new char[ANSI_ESCAPE_CODE_MIN_SIZE + 1 + 1 + 1];
		memcpy(format::endl, color::reset, ANSI_ESCAPE_CODE_MIN_SIZE + 1);
		memcpy(format::endl + ANSI_ESCAPE_CODE_MIN_SIZE + 1, "\n", 2);				// This does too.
	}
	else {
		color::red = new char[1]; color::red[0] = '\0';
		color::reset = new char[1]; color::reset[0] = '\0';

		format::error = new char[7 + 1];
		memcpy(format::error, "ERROR: ", 7 + 1);		// This copies null termination along with the message.

		format::endl = new char[1 + 1];
		memcpy(format::error, "\n", 1 + 1);				// This does too.
	}

	manageArgs(argc, argv);

	// Main loop for searching through each line as it comes.
	while (true) {
		std::string inputString = getLine();
		if (inputString.size() == 0) {
			break;
		}
		if (isTargetInString(inputString.c_str(), inputString.size(), targetString, targetString_len)) {
			std::cout << inputString << std::endl;
		}
	}

	delete[] targetString;
	releaseIOStyleStrings();
}
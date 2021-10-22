#define _CRT_SECURE_NO_WARNINGS

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN					// TODO: Make sure this is the right define for the job.
#include <Windows.h>
#endif

// TODO: Make sure the preprocessor defs in the setup menu are ordered, looks kind of weird right now.

#include <iostream>

#ifdef PLATFORM_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cstring>
#include <vector>

#define ANSI_ESCAPE_CODE_PREFIX "\033["
#define ANSI_ESCAPE_CODE_SUFFIX "m"
#define ANSI_ESCAPE_CODE_MIN_SIZE ((sizeof(ANSI_ESCAPE_CODE_PREFIX) - 1) + (sizeof(ANSI_ESCAPE_CODE_SUFFIX) - 1))

// POSIX functions
#ifdef PLATFORM_WINDOWS
#define isatty(x) _isatty(x)
#define fileno(x) _fileno(x)
#endif

bool usingColors = false;

namespace color {
	char* red;
	char* reset;

	//const char* red = ANSI_ESCAPE_CODE_PREFIX "31" ANSI_ESCAPE_CODE_SUFFIX;
	//const char* green  = ANSI_ESCAPE_CODE_PREFIX "32" ANSI_ESCAPE_CODE_SUFFIX;
	//const char* reset = ANSI_ESCAPE_CODE_PREFIX "0" ANSI_ESCAPE_CODE_SUFFIX;
}

namespace format {
	char* error;
	char* endl;
}

void releaseColorStrings() {
	if (usingColors) {
		delete[] color::red;
	}
}

void releaseFormatStrings() {
	delete[] format::error;
}

// Shows help text.
void showHelp() {
	std::cout << format::error << "help text not implemented" << format::endl;
	exit(EXIT_SUCCESS);
	// TODO: implement
}

char* targetString;

// args validation
void manageArgs(int argc, char** argv) {
	switch (argc) {
	case 1:
		std::cout << format::error << "too few arguments" << format::endl;
		showHelp();
	case 2:
		targetString = new char[strlen(argv[1] + 1)];
		strcpy(targetString, argv[1]);
		return;
	default:
		std::cout << format::error << "too many arguments" << format::endl;
		exit(EXIT_SUCCESS);
	}
}

bool isTargetInString(const char* string, size_t stringLen, const char* target, size_t targetLen) {
	if (targetLen > stringLen) { return false; }

	for (int i = 0; i <= stringLen - targetLen; i++) {
		if (!strncmp(string + i, target, targetLen)) {
			return true;
		}
	}
	return false;
}

char* getLineFromInput() {
	std::vector<char> buffer;
	while (true) {
		int tempchar = getchar();
		if (tempchar < 0) {
			return nullptr;
		}
		if (tempchar == '\n') { break; }
		buffer.push_back(tempchar);
	}
	buffer.push_back('\0');

	char* anotherBuffer = new char[buffer.size()];
	memcpy(anotherBuffer, buffer.data(), buffer.size());
	return anotherBuffer;
}

int main(int argc, char** argv) {
	// Only enable virtual terminal processing if stdout is a tty. If we're piping, don't do that.
	if (isatty(fileno(stdout))) {
		usingColors = true;

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

#ifdef PLATFORM_WINDOWS			// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping.
		// If we pipe, we need it disabled and we don't need ANSI key codes, so only do this when we don't pipe.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { return EXIT_FAILURE; }
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { return EXIT_FAILURE; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { return EXIT_FAILURE; }
#endif
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
		const char* inputString = getLineFromInput();
		if (!inputString) {
			break;
		}
		if (isTargetInString(inputString, strlen(inputString), targetString, strlen(targetString))) {
			std::cout << inputString << std::endl;
		}
		delete[] inputString;
	}

	std::cout << "releasing" << std::endl;
	releaseColorStrings();			// TODO: I think these might not get hit on every necessary failure. Make that not be the case.
	releaseFormatStrings();
}
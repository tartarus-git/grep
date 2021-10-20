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

#define ANSI_ESCAPE_CODE_PREFIX "\033["
#define ANSI_ESCAPE_CODE_SUFFIX "m"
#define ANSI_ESCAPE_CODE_MIN_SIZE ((sizeof(ANSI_ESCAPE_CODE_PREFIX) - 1) + (sizeof(ANSI_ESCAPE_CODE_SUFFIX) - 1))

// POSIX functions
#ifdef PLATFORM_WINDOWS
#define isatty(x) _isatty(x)
#define fileno(x) _fileno(x)
#endif

namespace color {
	char* red;

	//const char* red = ANSI_ESCAPE_CODE_PREFIX "31" ANSI_ESCAPE_CODE_SUFFIX;
	//const char* green  = ANSI_ESCAPE_CODE_PREFIX "32" ANSI_ESCAPE_CODE_SUFFIX;
	//const char* reset = ANSI_ESCAPE_CODE_PREFIX "0" ANSI_ESCAPE_CODE_SUFFIX;
}

void manageArgs(int argc, char** argv) {
	switch (argc) {
	case 1:
		// Too little
		return;
	case 2:
		// Just right
		return;
	default:
		// Too much
		return;
	}
}

int main(int argc, char** argv) {
#ifdef PLATFORM_WINDOWS
	HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { exit(EXIT_FAILURE); }
	DWORD mode;
	if (!GetConsoleMode(consoleOutputHandle, &mode)) { exit(EXIT_FAILURE); }
	if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { exit(EXIT_FAILURE); }
#endif

	// Check if stdout is pipe or not.
	if (isatty(fileno(stdout))) { color::red = new char[ANSI_ESCAPE_CODE_MIN_SIZE + 2 + 1]; memcpy(color::red, ANSI_ESCAPE_CODE_PREFIX "31" ANSI_ESCAPE_CODE_SUFFIX, ANSI_ESCAPE_CODE_MIN_SIZE + 2 + 1); }
	else { color::red = new char[1]; color::red[0] = '\0'; }

	//std::cout << color::red << "ERROR: bruuuuh" << std::endl;

	//std::string hi;
	//std::cin >> hi;
	//std::cout << hi << std::endl;
	//std::cout << "hi" << std::endl;
	char thing[100];
	fgets(thing, 100, stdin);
	printf(thing);
	printf("hi there");

	delete[] color::red;
}
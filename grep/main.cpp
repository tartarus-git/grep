#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

// TODO: Make sure the preprocessor defs in the setup menu are ordered, looks kind of weird right now.

#ifdef PLATFORM_WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif

#include <csignal>

#include <iostream>
#include <string>
#include <regex>

#define ANSI_ESC_CODE_PREFIX "\033["
#define ANSI_ESC_CODE_SUFFIX "m"
#define ANSI_ESC_CODE_MIN_SIZE ((sizeof(ANSI_ESC_CODE_PREFIX) - 1) + (sizeof(ANSI_ESC_CODE_SUFFIX) - 1))

// POSIX functions
#ifdef PLATFORM_WINDOWS
#define isatty(x) _isatty(x)
#define fileno(x) _fileno(x)
#endif

const char* helpText = "grep accepts text as input and outputs the lines from the input that have the specified keyphrase in them\n" \
					   "\n" \
					   "usage: grep [-c] <search string>\n" \
					   "\n" \
					   "arguments:\n" \
							"\t-c		-->			be case sensitive when matching";

bool isOutputColored;

namespace color {
	char* red;
	void initRed() {
		if (isOutputColored) {
			red = new char[ANSI_ESC_CODE_MIN_SIZE + 2 + 1];
			memcpy(red, ANSI_ESC_CODE_PREFIX "31" ANSI_ESC_CODE_SUFFIX, ANSI_ESC_CODE_MIN_SIZE + 2 + 1);
			return;
		}
		red = new char;
		*red = '\0';
	}

	char* reset;
	void initReset() {
		if (isOutputColored) {
			reset = new char[ANSI_ESC_CODE_MIN_SIZE + 1 + 1];
			memcpy(reset, ANSI_ESC_CODE_PREFIX "0" ANSI_ESC_CODE_SUFFIX, ANSI_ESC_CODE_MIN_SIZE + 1 + 1);
			return;
		}
		reset = new char;
		*reset = '\0';
	}

	void release() {
		delete[] color::red;
		delete[] color::reset;
	}
}

namespace format {
	char* error;
	void initError() {
		if (isOutputColored) {
			error = new char[ANSI_ESC_CODE_MIN_SIZE + 2 + 7 + 1];
			memcpy(error, color::red, ANSI_ESC_CODE_MIN_SIZE + 2);
			memcpy(error + ANSI_ESC_CODE_MIN_SIZE + 2, "ERROR: ", 7 + 1);
			return;
		}
		error = new char[7 + 1];
		memcpy(error, "ERROR: ", 7 + 1);
	}

	char* endl;
	void initEndl() {
		if (isOutputColored) {
			endl = new char[ANSI_ESC_CODE_MIN_SIZE + 1 + 1 + 1];
			memcpy(endl, color::reset, ANSI_ESC_CODE_MIN_SIZE + 1);
			memcpy(endl + ANSI_ESC_CODE_MIN_SIZE + 1, "\n", 1 + 1);
			return;
		}
		endl = new char[1 + 1];
		memcpy(endl, "\n", 1 + 1);
	}

	void release() {
		delete[] format::error;
		delete[] format::endl;
	}
}

void releaseOutputStyling() {
	format::release();
	color::release();
}

bool shouldLoopRun = true;
void signalHandler(int signum) { shouldLoopRun = false; }

namespace flags {
	bool caseSensitive = false;
}

void parseFlagGroup(char* arg) {
	for (int i = 0; ; i++) {
		switch (arg[i]) {
		case 'c':
			flags::caseSensitive = true;
			break;
		case '\0': return;
		default:
			color::initRed();
			format::initError();
			color::initReset();
			format::initEndl();
			std::cout << format::error << "one or more flag arguments is invalid" << format::endl;
			releaseOutputStyling();
			exit(EXIT_FAILURE);
		}
	}
}

unsigned int parseFlags(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') { parseFlagGroup(argv[i] + 1); continue; }
		return i;
	}
	return argc;				// No argument was found that wasn't a flag group.
}

void showHelp() {
	std::cout << helpText << std::endl;
	releaseOutputStyling();
	exit(EXIT_SUCCESS);
}

std::regex keyphraseRegex;

void manageArgs(int argc, char** argv) {
	unsigned int keyphraseArgIndex = parseFlags(argc, argv);
	switch (argc - keyphraseArgIndex) {
	case 0:
		color::initRed();
		format::initError();
		color::initReset();
		format::initEndl();
		std::cout << format::error << "too few arguments" << format::endl;
		if (isOutputColored) { showHelp(); }
		releaseOutputStyling();
		exit(EXIT_SUCCESS);
	case 1:
		{
			std::regex_constants::syntax_option_type regexFlags = std::regex_constants::grep;
			if (!flags::caseSensitive) { regexFlags |= std::regex_constants::icase; }
			keyphraseRegex = std::regex(argv[keyphraseArgIndex], regexFlags);
		}
		return;
	default:
		color::initRed();
		format::initError();
		color::initReset();
		format::initEndl();
		std::cout << format::error << "too many arguments" << format::endl;
		releaseOutputStyling();
		exit(EXIT_SUCCESS);
	}
}

int main(int argc, char** argv) {
	signal(SIGINT, signalHandler);

	if (isatty(fileno(stdout))) {					// Only enable colors if stdout is a tty to make piping output easier.
#ifdef PLATFORM_WINDOWS			// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping.
		// If we pipe, we need it disabled and we don't need ANSI key codes, so only do this when we don't pipe.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { return EXIT_FAILURE; }
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { return EXIT_FAILURE; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { return EXIT_FAILURE; }
#endif
		isOutputColored = true;
	}
	else { isOutputColored = false; }

	manageArgs(argc, argv);

	// Main loop for searching through each line as it comes.
	while (shouldLoopRun) {
		std::string line;
		std::getline(std::cin, line);
		if (std::cin.eof()) { break; }
		if (std::regex_search(line, keyphraseRegex)) { std::cout << line << std::endl; }
	}
}
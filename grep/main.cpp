// Memory usage.
#define INPUT_STREAM_BUFFER_MAX_SIZE 1024 * 1024																		// The maximum size to resize the stream buffer to. This prevents endless amounts of RAM from being used.
#define INPUT_STREAM_BUFFER_START_SIZE 256																				// The starting size for the stream buffer.
#define INPUT_STREAM_BUFFER_SIZE_STEP 256																				// How much more memory to reallocate with if the bounds of the previous memory were hit.

#include <csignal>

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN																								// Include Windows.h to get access to the few winapi functions that we need, such as the ones for getting console input handle and setting ANSI escape code support.
#include <Windows.h>
#endif

#include <cstdlib>																										// Needed for realloc function.

#ifdef PLATFORM_WINDOWS
#include <io.h>																											// Needed for _isatty function.
#define isatty(x) _isatty(x)																							// Renaming _isatty to isatty so it's the same as the function call in Linux.
#else
#include <errno.h>																										// Gives us access to errno global variable for reading errors from certain functions.
#include <unistd.h>																										// Linux isatty function is in here as well as some other useful stuff for this program as well I think.
#include <fcntl.h>																										// File control function used in InputStream to set console input to non-blocking.
#endif

#include <thread>																										// For access to std::this_thread::yield().

#include <stdio.h>
#ifdef PLATFORM_WINDOWS
#define fileno(x) _fileno(x)																							// Renaming for compatibility with the Linux version of the function call.
#endif
#include <iostream>
#include <string>
#include <regex>

#ifdef PLATFORM_WINDOWS																									// This define is already defined in one of the Linux-only headers, but for Windows, we need to explicitly do it.
#define STDIN_FILENO 0
#endif

// ANSI escape code helpers.
#define ANSI_ESC_CODE_PREFIX "\033["
#define ANSI_ESC_CODE_SUFFIX "m"
#define ANSI_ESC_CODE_MIN_SIZE ((sizeof(ANSI_ESC_CODE_PREFIX) - 1) + (sizeof(ANSI_ESC_CODE_SUFFIX) - 1))

// Help text.
const char* helpText = "grep accepts text as input and outputs the lines from the input that have the specified keyphrase in them\n" \
					   "\n" \
					   "usage: grep [-c] <regex keyphrase>\n" \
					   "       grep <--help|--h>            -->            shows help text\n" \
					   "\n" \
					   "arguments:\n" \
							"\t-c        -->        be case sensitive when matching";

// Flag to keep track of whether we should color output or not. Can also be used for testing if output is a TTY or not.
bool isOutputColored;

// Output coloring.
namespace color {
	char* red;
	void unsafeInitRed() {
		red = new char[ANSI_ESC_CODE_MIN_SIZE + 2 + 1];
		memcpy(red, ANSI_ESC_CODE_PREFIX "31" ANSI_ESC_CODE_SUFFIX, ANSI_ESC_CODE_MIN_SIZE + 2 + 1);
	}
	void unsafeInitPipedRed() {
		red = new char;
		*red = '\0';
	}
	void initRed() {
		if (isOutputColored) { unsafeInitRed(); return; }
		unsafeInitPipedRed();
	}

	char* reset;
	void unsafeInitReset() {
		reset = new char[ANSI_ESC_CODE_MIN_SIZE + 1 + 1];
		memcpy(reset, ANSI_ESC_CODE_PREFIX "0" ANSI_ESC_CODE_SUFFIX, ANSI_ESC_CODE_MIN_SIZE + 1 + 1);
	}
	void unsafeInitPipedReset() {
		reset = new char;
		*reset = '\0';
	}
	void initReset() {
		if (isOutputColored) { unsafeInitReset(); return; }
		unsafeInitPipedReset();
	}

	void release() {
		delete[] color::red;
		delete[] color::reset;
	}
}

// Output formatting.
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

// Helper function to initialize all output coloring and formatting. Basically gets us into a mode where we can output colored error messages.
void initOutputStyling() {
	color::initRed();
	format::initError();
	color::initReset();
	format::initEndl();
}

// Helper function to release all output coloring and formatting.
void releaseOutputStyling() {
	format::release();
	color::release();
}

// Flag so the main loop knows when to quit because of a SIGINT.
bool shouldLoopRun = true;
void signalHandler(int signum) { shouldLoopRun = false; }																		// Gets called on SIGINT.

// Collection of flags. These correspond to command-line flags you can set for the program.
namespace flags {
	bool caseSensitive = false;
}

// Parse a single flag group. A flag group is made out of a bunch of single letter flags.
void parseFlagGroup(char* arg) {
	for (int i = 0; ; i++) {
		switch (arg[i]) {
		case 'c':
			flags::caseSensitive = true;
			break;
		case '\0': return;
		default:
			initOutputStyling();
			std::cout << format::error << "one or more flag arguments is invalid" << format::endl;
			releaseOutputStyling();
			exit(EXIT_SUCCESS);
		}
	}
}

// Show help, but only if our output is connected to a TTY. This is simply to be courteous to any programs that might be receiving our stdout through a pipe.
void showHelp() { if (isOutputColored) { std::cout << helpText << std::endl; } }

// Parse flags at the argument level. Calls parseFlagGroup if it encounters flag groups and handles word flags (those with -- in front) separately.
unsigned int parseFlags(int argc, char** argv) {
	for (int i = 1; i < argc; i++) {
		const char* arg = argv[i];
		if (arg[0] == '-') {
			if (arg[1] == '-') {
				const char* flagTextStart = arg + 2;
				if (*flagTextStart == '\0') { continue; }
				if (!strcmp(flagTextStart, "help")) { showHelp(); exit(EXIT_SUCCESS); }
				if (!strcmp(flagTextStart, "h")) { showHelp(); exit(EXIT_SUCCESS); }
				initOutputStyling();
				std::cout << format::error << "one or more flag arguments is invalid" << format::endl;
				releaseOutputStyling();
				exit(EXIT_SUCCESS);
			}
			parseFlagGroup(argv[i] + 1);
			continue;
		}
		return i;													// Return index of first arg that isn't flag arg. Helps calling code parse args.
	}
	return argc;													// No non-flag argument was found. Return argc because it works nicely with calling code.
}

std::regex keyphraseRegex;

// Arg management function. Handles some argument parsing but pushes flag parsing to parseFlags.
void manageArgs(int argc, char** argv) {
	unsigned int keyphraseArgIndex = parseFlags(argc, argv);					// Parse flags before doing anything else.
	switch (argc - keyphraseArgIndex) {
	case 0:																		// If no non-flag args were found, throw error.
		initOutputStyling();
		std::cout << format::error << "too few arguments" << format::endl;
		showHelp();
		releaseOutputStyling();
		exit(EXIT_SUCCESS);
	case 1:																		// If exactly 1 non-flag argument exists, go ahead with program.
		{	// Unnamed namespace because without it one can't create variables inside switch cases.
			std::regex_constants::syntax_option_type regexFlags = std::regex_constants::grep | std::regex_constants::nosubs | std::regex_constants::optimize;
			if (!flags::caseSensitive) { regexFlags |= std::regex_constants::icase; }
			try { keyphraseRegex = std::regex(argv[keyphraseArgIndex], regexFlags); }									// Parse regex keyphrase.
			catch (const std::regex_error& err) {																		// Catch any errors relating to keyphrase regex syntax and report them.
				initOutputStyling();
				std::cout << format::error << "regex error: " << err.what() << format::endl;
				releaseOutputStyling();
				exit(EXIT_SUCCESS);
			}
		}
		return;
	default:																	// If more than 1 non-flag argument exists (includes flags after first non-flag arg), throw error.
		initOutputStyling();
		std::cout << format::error << "too many arguments" << format::endl;
		releaseOutputStyling();
		exit(EXIT_SUCCESS);
	}
}

class InputStream {
#ifdef PLATFORM_WINDOWS
public:

	static void init() { }
#else
	static char* buffer;
	static size_t bufferSize;

	static ssize_t bytesRead;
	static ssize_t bytesReceived;

public:
	static bool eof;

	static void init() {
		int result = fcntl(STDIN_FILENO, F_GETFL);																			// This code block sets file operations on the stdin file to non-blocking.
		if (result == -1) { return; }																						// If can't get necessary data through fcntl, return and just settle for blocking.
		if (fcntl(STDIN_FILENO, F_SETFL, result | O_NONBLOCK) == -1) { return; }											// If can't set necessary data through fcntl, return and just settle for blocking.

		buffer = new char[bufferSize];																						// Initialize buffer with the starting amount of RAM space.
	}
#endif

	static bool readLine(std::string& line) {																				// Returns false if no error, true if EOF or nothing to read. EOF is signaled by eof member variable in Linux. In Windows nothing to read state doesn't exist.
#ifdef PLATFORM_WINDOWS
		if (std::cin.eof()) { return true; }
		std::getline(std::cin, line);
		return false;
#else
		while (true) {
			for (; bytesRead < bytesReceived; bytesRead++) {																// Try reading as much as possible from buffer.
				char character = buffer[bytesRead];
				if (character == '\n') { bytesRead += 1; return false; }
				line += character;
			}

			bytesReceived = read(STDIN_FILENO, buffer, bufferSize);															// If buffer is drained, read more data into buffer.
			if (bytesReceived == 0) { eof = true; return true; }															// EOF
			if (bytesReceived == -1) {																						// Either nonblocking nothing to read or error, which is also reported to caller as EOF.
				if (errno == EAGAIN) { return true; }
				format::initError();																						// Assumes the colors are already set up because this is only triggered inside the main loop.
				format::initEndl();
				std::cout << format::error << "failed to read from stdin" << format::endl;
				format::release();
				eof = true;
				return true;

			}
			bytesRead = 0;

			if (bytesReceived == bufferSize) {																				// Make buffer bigger if it is filled with one read syscall. This minimizes amount of syscalls we have to do. Buffer doesn't have the ability to get smaller again, doesn't need to.
				size_t newBufferSize = bufferSize + INPUT_STREAM_BUFFER_SIZE_STEP;
				char* newBuffer = (char*)realloc(buffer, newBufferSize);
				if (newBuffer) { buffer = newBuffer; bufferSize = newBufferSize; }
			}
		}
#endif
	}

#ifdef PLATFORM_WINDOWS
	static void release() { }
#else
	static void release() { delete[] buffer; }
#endif
};

#ifndef PLATFORM_WINDOWS																														// Static members variables only need to be initialized in Linux because we don't have any in Windows.
char* InputStream::buffer;
size_t InputStream::bufferSize = INPUT_STREAM_BUFFER_START_SIZE;

ssize_t InputStream::bytesRead = 0;
ssize_t InputStream::bytesReceived = 0;

bool InputStream::eof = false;
#endif

// Program entry point
int main(int argc, char** argv) {
	signal(SIGINT, signalHandler);																												// Handling error here doesn't do any good because program should continue to operate regardless.
																																				// Reacting to errors here might poison stdout for programs on other ends of pipes, so just leave this be.

	// Only enable colors if stdout is a TTY to make reading piped output easier for other programs.
	if (isatty(fileno(stdout))) {
#ifdef PLATFORM_WINDOWS																															// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping. That's why this group is here.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { return EXIT_FAILURE; }
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { return EXIT_FAILURE; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { return EXIT_FAILURE; }
#endif
		isOutputColored = true;
	}
	else { isOutputColored = false; }

	manageArgs(argc, argv);																														// Manage the arguments so that we don't have to worry about it here.

	InputStream::init();																														// Initialize input streaming. Doesn't do anything on windows.

	std::string line;																															// Storage for the current line that the algorithm is working on.
	std::smatch matchData;																														// Storage for regex match data, which we use to color matches.

	// Branch based on if the output is colored or not. This is so that we don't check it over and over again inside the loop, which is terrible.
	if (isOutputColored) {																														// If output is colored, activate colors and do the more complex matching algorithm
		color::unsafeInitRed();
		color::unsafeInitReset();

		while (shouldLoopRun) {
			if (InputStream::readLine(line)) {
#ifdef PLATFORM_WINDOWS																															// InputStream::readLine is blocking on windows, so EOF is signaled with a true return, so break out here.
				break;
#else
				if (InputStream::eof) { break; }																								// EOF is signaled with InputStream::eof on Linux, so break out when that is encountered.
				std::this_thread::yield();																										// Give all other same-priority threads precedence over this one so we don't slow down the system.
				continue;																														// Line isn't complete, continue to build line.
#endif
			}

			if (std::regex_search(line, matchData, keyphraseRegex)) {																			// Highlighted regex search algorithm.
				do {
					ptrdiff_t matchPosition = matchData.position();
					std::cout.write(line.c_str(), matchPosition);
					std::cout << color::red;
					std::cout.write(line.c_str() + matchPosition, matchData.length());
					std::cout << color::reset;
					line = matchData.suffix();
				} while (std::regex_search(line, matchData, keyphraseRegex));
				std::cout << line << std::endl;																									// Print the rest of the line, where no match was found. The std::endl is important here.
			}

			line.clear();																														// Clear line buffer so we can use it again.
		}

		color::release();
		InputStream::release();																													// This doesn't do anything on Windows.
		return EXIT_SUCCESS;
	}

	color::unsafeInitPipedRed();																												// If output isn't colored, don't activate colors and do the simple matching algorithm.
	color::unsafeInitPipedReset();

	while (shouldLoopRun) {
		if (InputStream::readLine(line)) {
#ifdef PLATFORM_WINDOWS
			break;
#else
			if (InputStream::eof) { break; }
			std::this_thread::yield();
			continue;
#endif
		}

		if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << line << std::endl; }												// Print lines that match the regex.

		line.clear();
	}

	color::release();
	InputStream::release();
}

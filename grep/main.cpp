#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#ifdef PLATFORM_WINDOWS
#include <io.h>
#define isatty(x) _isatty(x)
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <csignal>

#include <stdio.h>
#ifdef PLATFORM_WINDOWS
#define fileno(x) _fileno(x)
#endif
#include <iostream>
#include <string>
#include <regex>

#ifdef PLATFORM_WINDOWS
#define STDIN_FILENO 0
#endif

// ANSI escape code helpers.
#define ANSI_ESC_CODE_PREFIX "\033["
#define ANSI_ESC_CODE_SUFFIX "m"
#define ANSI_ESC_CODE_MIN_SIZE ((sizeof(ANSI_ESC_CODE_PREFIX) - 1) + (sizeof(ANSI_ESC_CODE_SUFFIX) - 1))

// Help text.
const char* helpText = "grep accepts text as input and outputs the lines from the input that have the specified keyphrase in them\n" \
					   "\n" \
					   "usage: grep [-c] <search string>\n" \
					   "\n" \
					   "arguments:\n" \
							"\t-c		-->			be case sensitive when matching";

// Flag to keep track of whether we should color output or not. Can also be used for testing if output is a TTY or not.
bool isOutputColored;

// Output coloring.
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
void signalHandler(int signum) { shouldLoopRun = false; }						// Gets called on SIGINT.

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
			try { keyphraseRegex = std::regex(argv[keyphraseArgIndex], regexFlags); }
			catch (const std::regex_error& err) {
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
#define BUFFER_SIZE 256
	static char buffer[BUFFER_SIZE];
	static size_t eaten;
	static size_t created;

public:
	static bool eof;
	static void init() {
		int result = fcntl(STDIN_FILENO, F_GETFL);					// Read the prev data from the thing so I don't overwrite anything.
		if (result == -1) {
			std::cout << "failed to get file status flags data from stdin" << std::endl;
			return;
		}
		std::cout << result << std::endl;
		if (fcntl(STDIN_FILENO, F_SETFL, result | O_NONBLOCK) == -1) {
			// TODO: Implement logic for when the async setup fails.
			std::cout << "failed to set stdin to non-blocking in init()" << std::endl;
		}
	}

	static bool readLine(std::string& buffer) {
		while (true) {
			for (eaten; eaten < created; eaten++) {
				if (InputStream::buffer[eaten] == '\n') { eaten += 1; return 1; }
				buffer += InputStream::buffer[eaten];
			}
			if (eaten == created) {
				int bytesRead = read(STDIN_FILENO, InputStream::buffer, BUFFER_SIZE);
				if (bytesRead == 0) {
					eof = true;
					return false;
				}
				if (bytesRead == -1) {
					if (errno == EAGAIN) {
						return false;
					}
					return false;
				}
				created = bytesRead;
				eaten = 0;
			}
		}
	}
};

char InputStream::buffer[256];
size_t InputStream::eaten = 0;
size_t InputStream::created = 0;
bool InputStream::eof = false;

// Program entry point
int main(int argc, char** argv) {
	InputStream::init();
	signal(SIGINT, signalHandler);			// Handling error here doesn't do any good because program should continue to operate regardless.
											// Reacting to errors here might poison stdout for programs on other ends of pipes, so just leave this be.

	// Only enable colors if stdout is a TTY to make reading piped output easier for other programs.
	if (isatty(fileno(stdout))) {
#ifdef PLATFORM_WINDOWS			// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping. That's why this group is here.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { return EXIT_FAILURE; }
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { return EXIT_FAILURE; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { return EXIT_FAILURE; }
#endif
		isOutputColored = true;
	}
	else { isOutputColored = false; }

	manageArgs(argc, argv);									// Manage the arguments so that we don't have to worry about it here.

	std::string line;										// Storage for the current line that the algorithm is working on.
	std::smatch matchData;									// Storage for regex match data, which we use to color matches.
	color::initRed();										// Initialize coloring. This is for highlighting matches.
	color::initReset();
	while (shouldLoopRun) {																	// Run loop until shouldLoopRun says its time to quit.
		bool succeeded = InputStream::readLine(line);
		if (!succeeded) {
			if (InputStream::eof == true) {
				break;
			}
			continue;
		}


		if (isOutputColored) {																// Line output algorithm is different with colors because highlighting.
			if (std::regex_search(line, matchData, keyphraseRegex)) {
				do {
					ptrdiff_t matchPosition = matchData.position();
					std::cout.write(line.c_str(), matchPosition);
					std::cout << color::red;
					std::cout.write(line.c_str() + matchPosition, matchData.length());
					std::cout << color::reset;
					line = matchData.suffix();
				} while (std::regex_search(line, matchData, keyphraseRegex));
				std::cout << line << std::endl;			// Print the rest of the line, where no match was found. The std::endl is important here.
			}
		}
		else { if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << line << std::endl; } }	// Just output whole line for without colors.

		line.clear();													// Clear the line buffer so we can use it again.
	}
	color::release();													// Only release the colors if we've gotten to this point, because that's all we used.
}

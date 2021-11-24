// Memory usage.
#define INPUT_STREAM_BUFFER_MAX_SIZE 1024 * 1024																		// The maximum size to resize the stream buffer to. This prevents endless amounts of RAM from being used.
#define INPUT_STREAM_BUFFER_START_SIZE 256																				// The starting size for the stream buffer.
#define INPUT_STREAM_BUFFER_SIZE_STEP 256																				// How much more memory to reallocate with if the bounds of the previous memory were hit.

#include <csignal>
#ifndef PLATFORM_WINDOWS
#include <sys/signalfd.h>
#include <poll.h>
#endif

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

#include <chrono>																										// For access to time durations for use with sleep_for().
#include <thread>																										// For access to std::this_thread::sleep_for() and std::this_thread::yield().

#include <stdio.h>
#include <iostream>
#include <string>
#include <regex>

#ifdef PLATFORM_WINDOWS																									// This define is already defined in one of the Linux-only headers, but for Windows, we need to explicitly do it.
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
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

#ifdef PLATFORM_WINDOWS																// Only needed on Windows because we signal for the main loop to stop in Linux via artifical EOF signal.
// Flag so the main loop knows when to quit because of a SIGINT.
bool shouldLoopRun = true;
void signalHandler(int signum) { shouldLoopRun = false; }							// Gets called on SIGINT.
#endif

// Collection of flags. These correspond to command-line flags you can set for the program.
namespace flags {
	bool caseSensitive = false;
	bool allLines = false;
}

// Parse a single flag group. A flag group is made out of a bunch of single letter flags.
void parseFlagGroup(char* arg) {
	for (int i = 0; ; i++) {
		switch (arg[i]) {
		case 'c': flags::caseSensitive = true; break;
		case 'a': flags::allLines = true; break;
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

// Handles input in a buffered way.
class InputStream {
#ifdef PLATFORM_WINDOWS
public:
#else
	static pollfd fds[2];				// File descriptors to poll. One for stdin and one for a signal fd.
	static sigset_t sigmask;			// Signals to handle with poll.

	static char* buffer;
	static size_t bufferSize;

	static ssize_t bytesRead;				// Position of read head in buffer.
	static ssize_t bytesReceived;			// Position of write head in buffer.

public:
	static void init() {
		buffer = new char[bufferSize];								// Initialize buffer with the starting amount of RAM space.
		
		// Add SIGINT to set of tracked signals.
		if (sigemptyset(&sigmask) == -1) { goto errorBranch; }
		if (sigaddset(&sigmask, SIGINT) == -1) { goto errorBranch; }

		// Block the set of tracked signals from being tracked by default because obviously we're tracking them.
		if (sigprocmask(SIG_BLOCK, &sigmask, nullptr) == -1) { goto errorBranch; }

		// Create new file descriptor. Makes a read available when one of the tracked signals is caught.
		{
			int sigfd = signalfd(-1, &sigmask, 0);
			if (sigfd == -1) { goto errorBranch; }

			fds[1].fd = sigfd;				// Add sigfd to list of to be polled file descriptors, so we can be notified if we get a signal from poll.
			return;
		}

errorBranch:	fds[1].fd = -1;					// Tell poll to ignore this entry in the fds array.
	}
#endif

	static bool readLine(std::string& line) {						// Returns false on success. Returns true on EOF in Windows. Returns true on EOF or SIGINT or error on Linux.
#ifdef PLATFORM_WINDOWS
		if (std::cin.eof()) { return true; }
		std::getline(std::cin, line);
		return false;
#else
		while (true) {
			for (; bytesRead < bytesReceived; bytesRead++) {							// Read all the data in the buffer.
				char character = buffer[bytesRead];
				if (character == '\n') { bytesRead += 1; return false; }
				line += character;
			}

			// When no more data left in buffer, try get more.
			int result = poll(fds, 2, -1);			// Block until we either get some input on stdin or get a SIGINT.

			if (fds[1].revents) { return true; }		// Signal EOF if we got a SIGINT.

			bytesReceived = read(STDIN_FILENO, buffer, bufferSize);		// If input available on stdin, read as much as we can fit into the buffer.

			if (bytesReceived == 0) { return true; }		// In case of actual EOF, signal EOF.
			if (bytesReceived == -1) {					// In case of error, log and signal EOF for graceful exit of calling code.
				format::initError();					// Assumes the colors are already set up because this is only triggered inside the main loop.
				format::initEndl();
				std::cout << format::error << "failed to read from stdin" << format::endl;
				format::release();
				return true;

			}
			bytesRead = 0;							// If new data is read, the read head needs to be reset to the beginning of the buffer.

			if (bytesReceived == bufferSize) {	// Make buffer bigger if it is filled with one read syscall. This minimizes amount of syscalls we have to do. Buffer doesn't have the ability to get smaller again, doesn't need to.
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

#ifndef PLATFORM_WINDOWS										// Static members variables only need to be initialized in Linux because we don't have any in Windows.
pollfd InputStream::fds[] = { STDIN_FILENO, POLLIN, 0, 0, POLLIN, 0 };			// Parts of this data get changed later in runtime.
sigset_t InputStream::sigmask;

char* InputStream::buffer;
size_t InputStream::bufferSize = INPUT_STREAM_BUFFER_START_SIZE;

ssize_t InputStream::bytesRead = 0;
ssize_t InputStream::bytesReceived = 0;
#endif

void highlightMatches(std::string& line, std::smatch matchData) {
	do {
		ptrdiff_t matchPosition = matchData.position();
		std::cout.write(line.c_str(), matchPosition);
		std::cout << color::red;
		std::cout.write(line.c_str() + matchPosition, matchData.length());
		std::cout << color::reset;
		line = matchData.suffix();
	} while (std::regex_search(line, matchData, keyphraseRegex));
}

// Program entry point
int main(int argc, char** argv) {
#ifdef PLATFORM_WINDOWS
	signal(SIGINT, signalHandler);			// Handling error here doesn't do any good because program should continue to operate regardless.
											// Reacting to errors here might poison stdout for programs on other ends of pipes, so just leave this be.
#else
	InputStream::init();				// Initialise InputStream, doesn't do anything on Windows, so only necessary on Linux.
#endif
	// Only enable colors if stdout is a TTY to make reading piped output easier for other programs.
	if (isatty(STDOUT_FILENO)) {
#ifdef PLATFORM_WINDOWS					// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping. That's why this group is here.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { return EXIT_FAILURE; }
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { return EXIT_FAILURE; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { return EXIT_FAILURE; }
#endif
		isOutputColored = true;
	}
	else { isOutputColored = false; }

	manageArgs(argc, argv);				// Manage the arguments so that we don't have to worry about it here.

	std::string line;				// Storage for the current line that the algorithm is working on.
	std::smatch matchData;				// Storage for regex match data, which we use to color matches.

	// Branch based on if the output is colored or not. This is so that we don't check it over and over again inside the loop, which is terrible.
	if (isOutputColored) {				// If output is colored, activate colors and do the more complex matching algorithm
		color::unsafeInitRed();
		color::unsafeInitReset();

		if (flags::allLines) {
#ifdef PLATFORM_WINDOWS
			while (shouldLoopRun) {
#else
			while (true) {
#endif
				if (InputStream::readLine(line)) { break; }				// Break out of loop on EOF.
				if (std::regex_search(line, matchData, keyphraseRegex)) { highlightMatches(line, matchData); }
				std::cout << line << std::endl;							// Print the rest of the line where no match was found. If whole line is matchless, prints the whole line because flags::allLines.
				line.clear();											// Clear line buffer so readLine doesn't add next line onto the end.
			}
			goto releaseAndExit;
		}

#ifdef PLATFORM_WINDOWS
		while (shouldLoopRun) {
#else
		while (true) {
#endif
			if (InputStream::readLine(line)) { break; }					// Break out of loop on EOF.
			if (std::regex_search(line, matchData, keyphraseRegex)) { highlightMatches(line, matchData); std::cout << line << std::endl; }
			line.clear();												// Clear line buffer so we can use it again.
		}
		goto releaseAndExit;
	}

	color::unsafeInitPipedRed();					// If output isn't colored, don't activate colors.
	color::unsafeInitPipedReset();

	if (flags::allLines) {
#ifdef PLATFORM_WINDOWS
		while (shouldLoopRun) {
#else
		while (true) {
#endif
			if (InputStream::readLine(line)) { break; }
			std::cout << line << std::endl;						// If flags::allLines, output every line from input.
			line.clear();
		}
		goto releaseAndExit;
	}
	
#ifdef PLATFORM_WINDOWS
	while (shouldLoopRun) {
#else
	while (true) {
#endif
		if (InputStream::readLine(line)) { break; }
		if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << line << std::endl; }					// Print lines that match the regex.
		line.clear();
	}

releaseAndExit:
	color::release();
	InputStream::release();										// This doesn't do anything on Windows.
	return EXIT_SUCCESS;
}

/*

NOTE:

The Windows implementation relies on EOF being sent on SIGINT, but still uses the shouldLoopRun variable in case things don't go as planned, in order to still be able to exit eventually.
The problem with this approach is that in pipes where the first program doesn't listen to SIGINT and doesn't signal EOF to this grep program, this grep program wants to exit, but doesn't get to until a line is sent to it's stdin.
This problem doesn't exist in the Linux version because we intercept SIGINT in grep through polling a sig fd, which makes it instant, regardless of if data is available on stdin or not.
In order to do this in the Windows version (where such nice things as signalfd don't exist to my knowledge), I would need to do a whole lot of research and potentially some weird stuff.
Honestly, I'm too lazy for that right now and we're talking about something that, in all likelyhood, will never become a problem.
If it does become a problem, I'll just use the Windows findstr for that case.
For now, I'm just going to leave it like this.

*/

/*

NOTE:

There seems to be a bug in std::regex when using keyphrase ".*". When just doing match over the whole line and not highlighting, everything is fine, but for some reason, it gets stuck on match 0 and just keeps looping over and over.
You could say that it has to happen this way because, while it is strange, that behaviour makes the most sense based on the code above, but why is everything fine for keyphrase ".*k" then. Shouldn't the same problem apply there?

*/

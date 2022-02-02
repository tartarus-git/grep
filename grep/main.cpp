// Memory usage.
#define INPUT_STREAM_BUFFER_MAX_SIZE 1024 * 1024																		// The maximum size to resize the stream buffer to. This prevents endless amounts of RAM from being used.
#define INPUT_STREAM_BUFFER_START_SIZE 256																				// The starting size for the stream buffer.
#define INPUT_STREAM_BUFFER_SIZE_STEP 256																				// How much more memory to reallocate with if the bounds of the previous memory were hit.

#define HISTORY_BUFFER_MAX_LINE_COUNT 63																				// Maximum num of lines storable in history buffer when using --context x flag. Note that the count of history buffer is HISTORY_BUFFER_MAX_LINE_COUNT + 1 because of circular buffer.

// TODO: Make absolutely sure that I'm not including too much.

#include <csignal>																										// Signalling and polling things.
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
#include <unistd.h>																										// Linux isatty function is in here as well as some other useful stuff for this program as well I think.
#endif

#include <chrono>																										// For access to time durations for use with sleep_for().
#include <thread>																										// For access to std::this_thread::sleep_for() and std::this_thread::yield().

#include <cstdio>																										// NOTE: Use "c___" headers instead of "____.h" headers whenever possible. The "c___" counterparts minimize global scope pollution by putting a lot of things in namespaces and replacing some #defines with functions. It's more "correct".
#include <iostream>
#include <string>
#include <regex>

#ifdef PLATFORM_WINDOWS																									// These #defines are already defined in one of the Linux-only headers, but for Windows, we need to explicitly do it.
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#endif

// ANSI escape code helpers.
#define ANSI_ESC_CODE_PREFIX "\033["
#define ANSI_ESC_CODE_SUFFIX "m"
#define ANSI_ESC_CODE_MIN_SIZE ((sizeof(ANSI_ESC_CODE_PREFIX) - 1) + (sizeof(ANSI_ESC_CODE_SUFFIX) - 1))

const char* helpText = "grep accepts text as input and outputs the lines from the input that have the specified keyphrase in them\n" \
					   "\n" \
					   "usage: grep [-calv] [--context <amount> || --only-line-nums] <regex keyphrase>\n" \
					   "       grep <--help|--h>            -->            shows help text\n" \
					   "\n" \
					   "arguments:\n" \
							"\t-c                           -->         be case sensitive when matching\n" \
							"\t-a                           -->         print all lines from input but still color matches\n" \
							"\t-l                           -->         print line numbers\n" \
							"\t-v                           -->         invert output - print lines that would normally be omitted and omit lines that would normally be printed\n" \
							"\t--context <amount>           -->         print the specified amount of context (in lines) around each matched line\n" \
							"\t--only-line-nums             -->         print only the line numbers, not the actual lines\n";

// Flag to keep track of whether we should color output or not. Can also be used for testing if output is a TTY or not.
bool isOutputColored;						// TODO: In order to make testing the command programmatically easier, add a command line flag that overrides the default choice for isOutputColored and lets the user decide.

// Output coloring.
namespace color {
	char* red;
	void unsafeInitRed() { red = new char[ANSI_ESC_CODE_MIN_SIZE + 2 + 1]; memcpy(red, ANSI_ESC_CODE_PREFIX "31" ANSI_ESC_CODE_SUFFIX, ANSI_ESC_CODE_MIN_SIZE + 2 + 1); }
	void unsafeInitPipedRed() { red = new char; *red = '\0'; }
	void initRed() { if (isOutputColored) { unsafeInitRed(); return; } unsafeInitPipedRed(); }

	char* reset;
	void unsafeInitReset() { reset = new char[ANSI_ESC_CODE_MIN_SIZE + 1 + 1]; memcpy(reset, ANSI_ESC_CODE_PREFIX "0" ANSI_ESC_CODE_SUFFIX, ANSI_ESC_CODE_MIN_SIZE + 1 + 1); }
	void unsafeInitPipedReset() { reset = new char; *reset = '\0'; }
	void initReset() { if (isOutputColored) { unsafeInitReset(); return; } unsafeInitPipedReset(); }

	void release() { delete[] color::red; delete[] color::reset; }
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
bool shouldLoopRun = true;
void signalHandler(int signum) { shouldLoopRun = false; }
#endif

// Collection of flags. These correspond to command-line flags you can set for the program.
namespace flags {
	bool caseSensitive = false;
	bool allLines = false;
	bool lineNums = false;
	bool inverted = false;
	bool context = false;
	bool only_line_nums = false;
}

// Keeps track of the current line so a variety of helper functions can do things with it.
size_t lineCounter = 1;

class HistoryBuffer {
	static std::string* buffer;
	static unsigned int buffer_len;
	static unsigned int index;
	static unsigned int beginIndex;

public:
	static unsigned int buffer_lastIndex;
	static unsigned int amountFilled;																	// This variable isn't always used, only for specific modes where keeping track of amount of filled slots is the most efficient way.

	static void incrementAmountFilled() { if (amountFilled != buffer_lastIndex) { amountFilled++; } }

	static void purgeAmountFilled() { amountFilled = 0; }

	static void init() {
		buffer_len = buffer_lastIndex + 1;
		buffer = new std::string[buffer_len];
	}

	static void incrementHead() {
		if (index == beginIndex - 1) {
			if (beginIndex == buffer_lastIndex) { index++; beginIndex = 0; return; }
			index++; beginIndex++; return;
		}
		if (index == buffer_lastIndex) { if (beginIndex == 0) { beginIndex = 1; } index = 0; return; }
		index++;
	}

	static void push(const std::string& line) { buffer[index] = line; incrementHead(); }

	static void pushWithAmountInc(const std::string& line) { push(line); if (amountFilled != buffer_lastIndex) { amountFilled++; } }

	static void purge() { index = beginIndex; }

	static void purgeWithAmountSet() { purge(); purgeAmountFilled(); }

	static void print() { for ( ; beginIndex != index; beginIndex = (beginIndex == buffer_lastIndex ? 0 : beginIndex + 1)) { std::cout << buffer[beginIndex] << std::endl; } }

	// NOTE: We could store the amount filled and edit the variable with every call to push(), but I doubt that would be efficient since push is called so many more times than print is (at least normally).
	// Doing that would cause amountFilled to be added to over and over with little return on the investment. That's why we're just calculating amountFilled every time we need it, that way push() doesn't have to waste it's time incrementing anything.
	// NOTE: The exceptions to that rule are the peek functions. They're used for flags::inverted, where it's reasonable to cache amountFilled, so we do it there. Not for normal prints though.
	// NOTE: Another exception to that rule is flags::only_line_nums. There, the circular buffer isn't used and amountFilled is also the most efficient way of handling things. This is the case for printLineNums() and a couple functions at the top of the class.
	static unsigned int calculateAmountFilled() { return index > beginIndex ? index - beginIndex : buffer_len - beginIndex + index; }

	static void printLinesWithLineNums() {
		if (index == beginIndex) { return; }
		lineCounter -= calculateAmountFilled();
		std::cout << lineCounter << ' ' << buffer[beginIndex] << std::endl;
		lineCounter++;
		beginIndex = (beginIndex == buffer_lastIndex ? 0 : beginIndex + 1);
		for ( ; beginIndex != index; beginIndex = (beginIndex == buffer_lastIndex ? 0 : beginIndex + 1), lineCounter++) { std::cout << lineCounter << ' ' << buffer[beginIndex] << std::endl; }
	}

	static void lastPrintLineNums() { for (size_t historyLine = lineCounter - amountFilled; historyLine < lineCounter; historyLine++) { std::cout << historyLine << std::endl; } }
	static void printLineNums() { lastPrintLineNums(); purgeAmountFilled(); }

	static bool peekSafestLine(std::string& safestLine) { if (amountFilled == buffer_lastIndex) { safestLine = buffer[beginIndex]; return true; } return false; }

	static bool peekSafestLineNum(size_t& safestNum) { if (amountFilled == buffer_lastIndex) { safestNum = lineCounter - buffer_lastIndex; return true; } return false; }

	static void release() { delete[] buffer; }
};

std::string* HistoryBuffer::buffer;
unsigned int HistoryBuffer::buffer_len;
unsigned int HistoryBuffer::index = 0;
unsigned int HistoryBuffer::beginIndex = 0;
unsigned int HistoryBuffer::buffer_lastIndex;
unsigned int HistoryBuffer::amountFilled = 0;

// Parse a single flag group. A flag group is made out of a bunch of single letter flags.
void parseFlagGroup(char* arg) {
	for (int i = 0; ; i++) {
		switch (arg[i]) {
		case 'c': flags::caseSensitive = true; break;
		case 'a': flags::allLines = true; break;
		case 'l': flags::lineNums = true; break;
		case 'v': flags::inverted = true; break;
		case '\0': return;
		default:
			initOutputStyling();
			std::cout << format::error << "one or more flag arguments are invalid" << format::endl;
			releaseOutputStyling();
			exit(EXIT_SUCCESS);
		}
	}
}

// Adds a character to the end of an unsigned integer.
bool addToUInt(unsigned int& value, char character) {
	if (character < '0' || character > '9' ) { return false; }
	value = value * 10 + (character - '0');
	return true;
}

unsigned int parseUInt(char* string) {				// TODO: A future improvement would be to offset the line from the line number differently based on how many digits the number has in the console. So that lines don't start nudging to the right when going from line 99 to line 100. 
	unsigned int result = 0;
	if (string[0] != '\0') {
		if (addToUInt(result, string[0])) {
			for (int i = 1; ; i++) {
				if (addToUInt(result, string[i])) { continue; }
				if (string[i] == '\0' && result <= HISTORY_BUFFER_MAX_LINE_COUNT) { return result; }
				break;
			}
		}
	}
	initOutputStyling();
	std::cout << format::error << "invalid value for --context flag" << format::endl;
	releaseOutputStyling();
	exit(EXIT_SUCCESS);
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
				if (!strcmp(flagTextStart, "context")) {
					i++;
					if (i == argc) {
						initOutputStyling();
						std::cout << format::error << "the --context flag was not supplied with a value" << format::endl;
						releaseOutputStyling();
						exit(EXIT_SUCCESS);
					}
					HistoryBuffer::buffer_lastIndex = parseUInt(argv[i]);
					if (HistoryBuffer::buffer_lastIndex == 0) { continue; }															// Context value 0 is the same as no context, so don't bother setting context up.
					flags::context = true;
					continue;
				}
				if (!strcmp(flagTextStart, "only-line-nums")) { flags::only_line_nums = true; continue; }
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
		return i;																									// Return index of first arg that isn't flag arg. Helps calling code parse args.
	}
	return argc;																									// No non-flag argument was found. Return argc because it works nicely with calling code.
}

std::regex keyphraseRegex;

// Arg management function. Handles some argument parsing but pushes flag parsing to parseFlags.
void manageArgs(int argc, char** argv) {
	unsigned int keyphraseArgIndex = parseFlags(argc, argv);														// Parse flags before doing anything else.
	switch (argc - keyphraseArgIndex) {
	case 0:
		initOutputStyling();
		std::cout << format::error << "too few arguments" << format::endl;
		showHelp();
		releaseOutputStyling();
		exit(EXIT_SUCCESS);
	case 1:
		{																											// Unnamed namespace because we can't create variables inside switch cases otherwise.
			std::regex_constants::syntax_option_type regexFlags = std::regex_constants::grep | std::regex_constants::nosubs | std::regex_constants::optimize;
			if (!flags::caseSensitive) { regexFlags |= std::regex_constants::icase; }
			try { keyphraseRegex = std::regex(argv[keyphraseArgIndex], regexFlags); }								// Parse regex keyphrase.
			catch (const std::regex_error& err) {																	// Catch any errors relating to keyphrase regex syntax and report them.
				initOutputStyling();
				std::cout << format::error << "regex error: " << err.what() << format::endl;
				releaseOutputStyling();
				exit(EXIT_SUCCESS);
			}
		}
		return;
	default:																										// If more than 1 non-flag argument exists (includes flags after first non-flag arg), throw error.
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
	static void init() { std::cin.sync_with_stdio(false); }															// Unsynchronize C++ input buffer with C input buffer. This often makes things faster because implementors often don't bother buffering input until this is set to false (where they can then safely buffer input).
#else
	static pollfd fds[2];																							// File descriptors to poll. One for stdin and one for a signal fd.
	static sigset_t sigmask;																						// Signals to handle with poll.

	static char* buffer;
	static size_t bufferSize;

	static ssize_t bytesRead;																						// Position of read head in buffer.
	static ssize_t bytesReceived;																					// Position of write head in buffer.

public:
	static void init() {
		buffer = new char[bufferSize];																				// Initialize buffer with the starting amount of RAM space.
		
		// Add SIGINT and SIGTERM to set of tracked signals.
		if (sigemptyset(&sigmask) == -1) { goto errorBranch; }
		if (sigaddset(&sigmask, SIGINT) == -1) { goto errorBranch; }
		if (sigaddset(&sigmask, SIGTERM) == -1) { goto errorBranch; }

		// Block the set of tracked signals from being handled by default by thread because obviously we're handling them.
		if (sigprocmask(SIG_BLOCK, &sigmask, nullptr) == -1) { goto errorBranch; }

		// Create new file descriptor. Makes a read available when one of the tracked signals is caught.
		{
			int sigfd = signalfd(-1, &sigmask, 0);
			fds[1].fd = sigfd;																						// Add sigfd to list of to be polled file descriptors, so we can be notified if we get a signal from poll. It doesn't matter if it's -1 because on failure we set it to that anyway.
			return;
		}

errorBranch:	fds[1].fd = -1;																						// Tell poll to ignore the now unused entry in fds because some error prevented us from setting it up correctly.
	}

	// NOTE: I think we probably could have just used the standard input buffering mechanisms (while retaining the ability to adjust buffer size) instead of building our own.
	// NOTE: That would probably be faster since the standard is optimized for each platform. I'm too proud of my mechanism to remove it right now, so I'm leaving it in for the forseeable future.
	// NOTE: Plus, I haven't done any benchmarks. Maybe this is faster than the standard because the standard might do a bunch of safety checks and unnecessary stuff.

	static bool refillBuffer() {
		// When no more data left in buffer, try get more.
		if (poll(fds, 2, -1) == -1) {																				// Block until we either get some input on stdin or get either a SIGINT or a SIGTERM.
			format::initError();
			format::initEndl();
			std::cout << format::error << "failed to poll stdin, SIGINT and SIGTERM" << format::endl;
			format::release();
			return false;
		}

		if (fds[1].revents) { return false; }																		// Signal EOF if we caught a signal.

		bytesReceived = read(STDIN_FILENO, buffer, bufferSize);														// Read as much as we can fit into the buffer.

		if (bytesReceived == 0) { return false; }																	// In case of actual EOF, signal EOF.
		if (bytesReceived == -1) {																					// In case of error, log and signal EOF.
			format::initError();																					// Assumes the colors are already set up because this is only triggered inside the main loop.
			format::initEndl();
			std::cout << format::error << "failed to read from stdin" << format::endl;
			format::release();
			return false;

		}
		bytesRead = 0;																								// If new data is read, the read head needs to be reset to the beginning of the buffer.

		// TODO: The below comparison gives warnings on gcc. Makes sense, can we stop comparing two different types here?
		if (bytesReceived == bufferSize) {																			// Make buffer bigger if it is filled with one read syscall. This minimizes amount of syscalls we have to do. Buffer doesn't have the ability to get smaller again, doesn't need to.
			size_t newBufferSize = bufferSize + INPUT_STREAM_BUFFER_SIZE_STEP;
			char* newBuffer = (char*)realloc(buffer, newBufferSize);
			if (newBuffer) { buffer = newBuffer; bufferSize = newBufferSize; }
		}

		return true;
	}
#endif

	// TODO: Do those TODO's from the diff source code in here.

	static bool readLine(std::string& line) {																		// Returns true on success. Returns false on EOF or error in Windows. Returns false on EOF or SIGINT or SIGTERM or error on Linux.
#ifdef PLATFORM_WINDOWS
		if (std::getline(std::cin, line)) { return true; }															// Get line. Technically, eofbit gets set if EOF terminates the line, but we don't worry about that because in that case we have to return true as well.
		// NOTE: Technically, one could put the below line above std::getline, but that would do an unnecessary branch for every readLine in the file. This way, the branch is only tested when it has to be, which induces small overhead at EOF but saves a bunch of overhead in the loops.
		if (std::cin.eof()) { return false; }																		// If getline fails because we're trying to read at the EOF position (in which case eofbit will be set), return false without doing error reporting.
		format::initError();																						// Otherwise, some error occurred and we need to report it and return false.
		format::initEndl();
		std::cout << format::error << "failed to read from stdin" << format::endl;
		format::release();
		return false;

#else
		while (true) {
			for (; bytesRead < bytesReceived; bytesRead++) {														// Read all the data in the buffer.
				if (buffer[bytesRead] == '\n') { bytesRead += 1; return true; }										// Stop when we encounter the end of the current line.
				line += buffer[bytesRead];
			}
			if (refillBuffer()) { continue; }																		// If we never encounter the end of the line in the current buffer, fetch more data.
			return false;																							// If something went wrong while refilling buffer, return false.
		}
#endif
	}

	static bool discardLine() {																						// Reads the next line but doesn't store it anywhere since we're only reading it to advance the read position. Returns true on success. Returns false on EOF or error in Windows. Returns false on EOF, SIGINT, SIGTERM or error on Linux.
#ifdef PLATFORM_WINDOWS
		char character;
		while (true) {																								// Not going to check for shouldLoopRun here because interrupting inside of a line isn't something that readLine offers either and because console is line-buffered anyway, so it wouldn't actually enable signalling while input is pending.
			character = std::cin.get();																				// Processing characters once they've been submitted by user is super fast, so the check would be pretty much unnecessary unless the lines are super super super long, which doesn't happen often.
			if (character == EOF) { return false; }																	// Even if the lines are long, all you'll have to do is press Ctrl+C and wait for the line to be over for grep to quit. This is all so unlikely, that I'm not going to waste a branch checking for it.
			if (std::cin.fail()) {
				format::initError();
				format::initEndl();
				std::cout << format::error << "failed to read from stdin" << format::endl;
				format::release();
				return false;
			}
			if (character == '\n') { return true; }
		}
#else
		while (true) {
			for ( ; bytesRead < bytesReceived; bytesRead++) { if (buffer[bytesRead] == '\n') { bytesRead += 1; return true; } }						// Read all the data in current buffer and exit as soon as we've discarded an entire line.
			if (refillBuffer()) { continue; }
			return false;
		}
#endif
	}

#ifndef PLATFORM_WINDOWS
	static void release() { delete[] buffer; }
#endif
};

#ifndef PLATFORM_WINDOWS																							// Static members variables only need to be initialized in Linux because we don't have any in Windows.
pollfd InputStream::fds[] = { STDIN_FILENO, POLLIN, 0, 0, POLLIN, 0 };												// Parts of this data get changed later in runtime.
sigset_t InputStream::sigmask;

char* InputStream::buffer;
size_t InputStream::bufferSize = INPUT_STREAM_BUFFER_START_SIZE;

ssize_t InputStream::bytesRead = 0;
ssize_t InputStream::bytesReceived = 0;
#endif

std::string line;
std::smatch matchData;

void highlightMatches() {																							// I assume this will be inlined. Probably not in debug mode, but almost definitely in release mode.
	do {
		ptrdiff_t matchPosition = matchData.position();
		std::cout.write(line.c_str(), matchPosition);
		std::cout << color::red;
		std::cout.write(line.c_str() + matchPosition, matchData.length());
		std::cout << color::reset;
		line = matchData.suffix();
	} while (std::regex_search(line, matchData, keyphraseRegex));
}

// A couple of #defines to help reduce code bloat in the coming sections of the program.

#ifdef PLATFORM_WINDOWS
#define MAIN_WHILE InputStream::init(); while (shouldLoopRun)
#else
#define MAIN_WHILE InputStream::init(); while (true)
#endif

#define LINE_WHILE_START MAIN_WHILE { if (!InputStream::readLine(line)) { break; }
#ifdef PLATFORM_WINDOWS
#define LINE_WHILE_END_INNER }
#else
#define LINE_WHILE_END_INNER line.clear(); } InputStream::release();
#endif
#define LINE_WHILE_END LINE_WHILE_END_INNER return 0;

#define COLORED_LINE_WHILE_START color::unsafeInitRed(); color::unsafeInitReset(); LINE_WHILE_START
#define COLORED_LINE_WHILE_END_INNER LINE_WHILE_END_INNER color::release();
#define COLORED_LINE_WHILE_END COLORED_LINE_WHILE_END_INNER return 0;
#define COLORED_RED_ONLY_LINE_WHILE_START color::unsafeInitRed(); color::unsafeInitReset(); std::cout << color::red; LINE_WHILE_START
#define COLORED_RED_ONLY_LINE_WHILE_END LINE_WHILE_END_INNER std::cout << color::reset; color::release(); return 0;

#ifdef PLATFORM_WINDOWS
#define LINE_WHILE_CONTINUE continue;
#else
#define LINE_WHILE_CONTINUE line.clear(); continue;
#endif

#ifdef PLATFORM_WINDOWS
#define INNER_WINDOWS_SIGNAL_CHECK_START if (shouldLoopRun) {
#define INNER_WINDOWS_SIGNAL_CHECK_END(releasingCode) } else { releasingCode }
#else
#define INNER_WINDOWS_SIGNAL_CHECK_START
#define INNER_WINDOWS_SIGNAL_CHECK_END(releasingCode)
#endif

#ifdef PLATFORM_WINDOWS
#define INNER_INPUT_STREAM_READ_LINE(releasingCode) if (!InputStream::readLine(line)) { releasingCode }
#define INNER_INPUT_STREAM_DISCARD_LINE(releasingCode) if (!InputStream::discardLine()) { releasingCode }
#else
#define INNER_INPUT_STREAM_READ_LINE(releasingCode) if (!InputStream::readLine(line)) { InputStream::release(); releasingCode }
#define INNER_INPUT_STREAM_DISCARD_LINE(releasingCode) if (!InputStream::discardLine()) { InputStream::release(); releasingCode }
#endif

// TODO: Learn about SFINAE and std::enable_if.

// Program entry point
int main(int argc, char** argv) {
	std::cout.sync_with_stdio(false);		// Unsynchronize C++ output buffer with C output buffer. Normally set to true to avoid output getting mixed around when mixing C++ and C function calls in source code. Setting to false yields performance improvements for same reason as doing this for input buffers above.
											// IMPORTANT: As a result of doing this unsync stuff, we have to choose either C or C++ style and stick with it for the whole program. It is still possible to use C-style input and C++-style output at same time, the limitation only applies within the boundaries of input and output.
#ifdef PLATFORM_WINDOWS
	signal(SIGINT, signalHandler);			// Handling error here doesn't do any good because program should continue to operate regardless.
	signal(SIGTERM, signalHandler);			// Reacting to errors here might poison stdout for programs on other ends of pipes, so just leave this be.
#endif

	// Only enable colors if stdout is a TTY to make reading piped output easier for other programs.
	if (isatty(STDOUT_FILENO)) {
#ifdef PLATFORM_WINDOWS						// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping. That's why this group is here.
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

	// NOTE: As much of the branching is done outside of the loops as possible so as to avoid checking data over and over even though it'll never change it's value.
	// The compiler is really good at doing this and technically would do it for me, but this way it's a safe bet and I can rest easy. Also, the main reason is that this way is more organized.
	// Trying to branch inside the loops and to condense everything down to one single loop with a bunch of conditionals inside gets ugly really fast. I prefer this layout more.
	if (isOutputColored) {					// If output is colored, activate colors before going into each loop and do the more complex matching algorithm
		if (flags::allLines) {
			if (flags::inverted) { return 0; }
			if (flags::only_line_nums) {
				COLORED_LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << color::red << lineCounter << color::reset << std::endl; lineCounter++; LINE_WHILE_CONTINUE; }
					std::cout << lineCounter << std::endl; lineCounter++;
				COLORED_LINE_WHILE_END
			}
			if (flags::lineNums) {
				COLORED_LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); std::cout << line << std::endl; lineCounter++; LINE_WHILE_CONTINUE; }
					std::cout << lineCounter << ' ' << line << std::endl; lineCounter++;
				COLORED_LINE_WHILE_END
			}
			COLORED_LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { highlightMatches(); } std::cout << line << std::endl; COLORED_LINE_WHILE_END
		}

		if (flags::context) {
			if (flags::only_line_nums) {
				if (flags::inverted) {
					isOutputColored = false;																												// Necessary so that error reporting doesn't assume that we have colors set up.
					LINE_WHILE_START
						if (std::regex_search(line, matchData, keyphraseRegex)) {
							HistoryBuffer::purgeAmountFilled();
							lineCounter++;
							for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
								INNER_WINDOWS_SIGNAL_CHECK_START
									line.clear();
									INNER_INPUT_STREAM_READ_LINE(return 0;)
									if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
									lineCounter++;
								INNER_WINDOWS_SIGNAL_CHECK_END(return 0;)
							}
							LINE_WHILE_CONTINUE;
						}
						size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { std::cout << safestLineNum << std::endl; }
						HistoryBuffer::incrementAmountFilled();
						lineCounter++;
					LINE_WHILE_END_INNER HistoryBuffer::lastPrintLineNums(); return 0;
				}
				COLORED_LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) {
						HistoryBuffer::printLineNums();
						std::cout << color::red << lineCounter << color::reset << std::endl;
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {			// SIDE-NOTE: Technically, for a lot of my use-cases, a do-while loop would be more efficient than a for loop since the initial check is garanteed to return true.
							INNER_WINDOWS_SIGNAL_CHECK_START																									// SIDE-NOTE: Even so, I like the way for loops look because you see the information for the loop at the top and not all the way at the bottom, plus, compiler optimizes.
								line.clear();
								INNER_INPUT_STREAM_READ_LINE(color::release(); return 0;)
								if (std::regex_search(line, matchData, keyphraseRegex)) {
									std::cout << color::red << lineCounter << color::reset << std::endl;
									lineCounter++;
									afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex;
									continue;
								}
								std::cout << lineCounter << std::endl;
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END(color::release(); return 0;)
						}
						LINE_WHILE_CONTINUE;
					}
					HistoryBuffer::incrementAmountFilled();
					lineCounter++;
				COLORED_LINE_WHILE_END
			}
			if (flags::lineNums) {
				if (flags::inverted) {
					isOutputColored = false;
					HistoryBuffer::init();
					LINE_WHILE_START
						if (std::regex_search(line, matchData, keyphraseRegex)) {
							HistoryBuffer::purgeWithAmountSet();
							lineCounter++;
							for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
								INNER_WINDOWS_SIGNAL_CHECK_START
									line.clear();
									INNER_INPUT_STREAM_READ_LINE(HistoryBuffer::release(); return 0;)
									if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
									lineCounter++;
								INNER_WINDOWS_SIGNAL_CHECK_END(HistoryBuffer::release(); return 0;)
							}
							LINE_WHILE_CONTINUE;
						}
						std::string safestLine;
						if (HistoryBuffer::peekSafestLine(safestLine)) {
							size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { std::cout << safestLineNum << ' ' << safestLine << std::endl; }
						}
						HistoryBuffer::pushWithAmountInc(line);
						lineCounter++;
					LINE_WHILE_END_INNER HistoryBuffer::printLinesWithLineNums(); HistoryBuffer::release(); return 0;
				}
				HistoryBuffer::init();
				COLORED_LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) {
						HistoryBuffer::printLinesWithLineNums();
						std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); std::cout << line << std::endl;
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								line.clear();
								INNER_INPUT_STREAM_READ_LINE(color::release(); HistoryBuffer::release(); return 0;)
								if (std::regex_search(line, matchData, keyphraseRegex)) {
									std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); std::cout << line << std::endl;
									lineCounter++;
									afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex;
									continue;
								}
								std::cout << lineCounter << ' ' << line << std::endl;
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END(color::release(); HistoryBuffer::release(); return 0;)
						}
						LINE_WHILE_CONTINUE;
					}
					HistoryBuffer::push(line);
					lineCounter++;
				COLORED_LINE_WHILE_END_INNER HistoryBuffer::release(); return 0;
			}
			if (flags::inverted) {
				isOutputColored = false;
				HistoryBuffer::init();
				LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) {
						HistoryBuffer::purgeWithAmountSet();
						for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								line.clear();
								INNER_INPUT_STREAM_READ_LINE(HistoryBuffer::release(); return 0;)
								if (std::regex_search(line, matchData, keyphraseRegex)) { padding = HistoryBuffer::buffer_lastIndex; continue; }
								padding--;
							INNER_WINDOWS_SIGNAL_CHECK_END(HistoryBuffer::release(); return 0;)
						}
						LINE_WHILE_CONTINUE;
					}
					std::string safestLine; if (HistoryBuffer::peekSafestLine(safestLine)) { std::cout << safestLine << std::endl; }
					HistoryBuffer::pushWithAmountInc(line);
				LINE_WHILE_END_INNER HistoryBuffer::print(); HistoryBuffer::release(); return 0;
			}
			HistoryBuffer::init();
			COLORED_LINE_WHILE_START
				if (std::regex_search(line, matchData, keyphraseRegex)) {
					HistoryBuffer::print();
					highlightMatches(); std::cout << line << std::endl;
					for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							line.clear();
							INNER_INPUT_STREAM_READ_LINE(color::release(); HistoryBuffer::release(); return 0;)
							if (std::regex_search(line, matchData, keyphraseRegex)) {
								highlightMatches(); std::cout << line << std::endl;
								padding = HistoryBuffer::buffer_lastIndex;
								continue;
							}
							std::cout << line << std::endl;
							padding--;
						INNER_WINDOWS_SIGNAL_CHECK_END(color::release(); HistoryBuffer::release(); return 0;)
					}
					LINE_WHILE_CONTINUE;
				}
				HistoryBuffer::push(line);
			COLORED_LINE_WHILE_END_INNER HistoryBuffer::release(); return 0;
		}

		// SIDE-NOTE: Storing these command-line flags in a bit field and switching on it's value might have been a better way to do this in theory, because it's more efficient and might look better, but in practice, it might not have been the way to go.
		// It's not very scalable because you have to write out every single case (or use default, but that isn't applicable when you have complex relationships between cases and such). As soon as you have anywhere near something like 16 flags, your code bloat starts to become insane.
		// So I think the if statement approach is a good one, even though it's technically a very small bit less performant than the switch case approach.

		if (flags::inverted) {
			isOutputColored = false;
			if (flags::only_line_nums) { LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; LINE_WHILE_CONTINUE; } std::cout << lineCounter << std::endl; lineCounter++; LINE_WHILE_END }
			if (flags::lineNums) { LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; LINE_WHILE_CONTINUE; } std::cout << lineCounter << ' ' << line << std::endl; lineCounter++; LINE_WHILE_END }
			LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { LINE_WHILE_CONTINUE } std::cout << line << std::endl; LINE_WHILE_END
		}
		if (flags::only_line_nums) { COLORED_RED_ONLY_LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << lineCounter << std::endl; } lineCounter++; COLORED_RED_ONLY_LINE_WHILE_END }
		if (flags::lineNums) { COLORED_LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); std::cout << line << std::endl; } lineCounter++; COLORED_LINE_WHILE_END }
		COLORED_LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { highlightMatches(); std::cout << line << std::endl; } COLORED_LINE_WHILE_END
	}

	if (flags::allLines) {
		if (flags::inverted) { return 0; }
		if (flags::only_line_nums) { MAIN_WHILE { INNER_INPUT_STREAM_DISCARD_LINE(return 0;) std::cout << lineCounter << std::endl; lineCounter++; } }				// TODO: You didn't release the InputStream here. Memory leak. Make this right.
		if (flags::lineNums) { LINE_WHILE_START std::cout << lineCounter << ' ' << line << std::endl; lineCounter++; LINE_WHILE_END }
		LINE_WHILE_START std::cout << line << std::endl; LINE_WHILE_END
	}

	// TODO: NOTE: I can't figure out why the one line at the end is gone (the correct bahaviour) when you run the master branch and but it's there in the temp branch. The bahaviour should be the same just from looking at the code, what am I missing?

	if (flags::context) {
		if (flags::only_line_nums) {
			if (flags::inverted) {
				LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) {
						HistoryBuffer::purgeAmountFilled();
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								line.clear();
								INNER_INPUT_STREAM_READ_LINE(return 0;)
								if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END(return 0;)
						}
						LINE_WHILE_CONTINUE;
					}
					size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { std::cout << safestLineNum << std::endl; }
					HistoryBuffer::incrementAmountFilled();
					lineCounter++;
				LINE_WHILE_END_INNER HistoryBuffer::lastPrintLineNums(); return 0;
			}
			LINE_WHILE_START
				if (std::regex_search(line, matchData, keyphraseRegex)) {
					HistoryBuffer::printLineNums();
					std::cout << lineCounter << std::endl;
					lineCounter++;
					for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							line.clear();
							INNER_INPUT_STREAM_READ_LINE(return 0;)
							std::cout << lineCounter << std::endl;
							lineCounter++;
							if (std::regex_search(line, matchData, keyphraseRegex)) { afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; }
						INNER_WINDOWS_SIGNAL_CHECK_END(return 0;)
					}
					LINE_WHILE_CONTINUE;
				}
				HistoryBuffer::incrementAmountFilled();
				lineCounter++;
			LINE_WHILE_END
		}
		if (flags::lineNums) {
			if (flags::inverted) {
				HistoryBuffer::init();
				LINE_WHILE_START
					if (std::regex_search(line, matchData, keyphraseRegex)) {
						HistoryBuffer::purgeWithAmountSet();
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								line.clear();
								INNER_INPUT_STREAM_READ_LINE(HistoryBuffer::release(); return 0;)
								if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END(HistoryBuffer::release(); return 0;)
						}
						LINE_WHILE_CONTINUE;
					}
					std::string safestLine;
					if (HistoryBuffer::peekSafestLine(safestLine)) {
						size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { std::cout << safestLineNum << ' ' << safestLine << std::endl; }
					}
					HistoryBuffer::pushWithAmountInc(line);
					lineCounter++;
				LINE_WHILE_END_INNER HistoryBuffer::printLinesWithLineNums(); HistoryBuffer::release(); return 0;
			}
			HistoryBuffer::init();
			LINE_WHILE_START
				if (std::regex_search(line, matchData, keyphraseRegex)) {
					HistoryBuffer::printLinesWithLineNums();
					std::cout << lineCounter << ' ' << line << std::endl;
					lineCounter++;
					for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							line.clear();
							INNER_INPUT_STREAM_READ_LINE(HistoryBuffer::release(); return 0;)
							std::cout << lineCounter << ' ' << line << std::endl;
							lineCounter++;
							if (std::regex_search(line, matchData, keyphraseRegex)) { afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; }
						INNER_WINDOWS_SIGNAL_CHECK_END(HistoryBuffer::release(); return 0;)
					}
					LINE_WHILE_CONTINUE;
				}
				HistoryBuffer::push(line);
				lineCounter++;
			LINE_WHILE_END_INNER HistoryBuffer::release(); return 0;
		}
		if (flags::inverted) {
			HistoryBuffer::init();
			LINE_WHILE_START
				if (std::regex_search(line, matchData, keyphraseRegex)) {
					HistoryBuffer::purgeWithAmountSet();
					for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							line.clear();
							INNER_INPUT_STREAM_READ_LINE(HistoryBuffer::release(); return 0;)
							if (std::regex_search(line, matchData, keyphraseRegex)) { padding = HistoryBuffer::buffer_lastIndex; continue; }
							padding--;
						INNER_WINDOWS_SIGNAL_CHECK_END(HistoryBuffer::release(); return 0;)
					}
					LINE_WHILE_CONTINUE;
				}
				std::string safestLine; if (HistoryBuffer::peekSafestLine(safestLine)) { std::cout << safestLine << std::endl; }
				HistoryBuffer::pushWithAmountInc(line);
			LINE_WHILE_END_INNER HistoryBuffer::print(); HistoryBuffer::release(); return 0;
		}
		HistoryBuffer::init();
		LINE_WHILE_START
			if (std::regex_search(line, matchData, keyphraseRegex)) {
				HistoryBuffer::print();
				std::cout << line << std::endl;
				for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
					INNER_WINDOWS_SIGNAL_CHECK_START
						line.clear();
						INNER_INPUT_STREAM_READ_LINE(HistoryBuffer::release(); return 0;)
						if (std::regex_search(line, matchData, keyphraseRegex)) {
							std::cout << line << std::endl;
							padding = HistoryBuffer::buffer_lastIndex;
							continue;
						}
						std::cout << line << std::endl;
						padding--;
					INNER_WINDOWS_SIGNAL_CHECK_END(HistoryBuffer::release(); return 0;)
				}
				LINE_WHILE_CONTINUE;
			}
			HistoryBuffer::push(line);
		LINE_WHILE_END_INNER HistoryBuffer::release(); return 0;
	}

	if (flags::inverted) {
		if (flags::only_line_nums) { LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; LINE_WHILE_CONTINUE; } std::cout << lineCounter << std::endl; lineCounter++; LINE_WHILE_END }
		if (flags::lineNums) { LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { lineCounter++; LINE_WHILE_CONTINUE; } std::cout << lineCounter << ' ' << line << std::endl; lineCounter++; LINE_WHILE_END }
		LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { LINE_WHILE_CONTINUE; } std::cout << line << std::endl; LINE_WHILE_END
	}
	if (flags::only_line_nums) { LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << lineCounter << std::endl; } lineCounter++; LINE_WHILE_END }
	if (flags::lineNums) { LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << lineCounter << ' ' << line << std::endl; } lineCounter++; LINE_WHILE_END }
	LINE_WHILE_START if (std::regex_search(line, matchData, keyphraseRegex)) { std::cout << line << std::endl; } LINE_WHILE_END
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

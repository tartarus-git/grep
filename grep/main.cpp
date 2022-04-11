// Memory usage.
#define INPUT_STREAM_BUFFER_MAX_SIZE (8192 * 1024)					// 8 MB		<-- ABSOLUTELY MUST be a multiple of INPUT_STREAM_BUFFER_SIZE_STEP, or else program will break in certain scenarios.
#define INPUT_STREAM_BUFFER_START_SIZE 8192							// NOTE: Popular file sys block sizes (aka. logical block sizes) are 4KB and 8KB. We start the buffer at 8KB and scale with 8KB until a max size of 8MB.
#define INPUT_STREAM_BUFFER_SIZE_STEP 8192							// NOTE: This is to try to get the buffer to be a multiple of file sys block size. This system works great for block sizes under 8KB too because the block sizes are in powers of two and 8KB covers the alignment for 4KB perfectly.

#define VECTOR_STRING_BUFFER_MAX_SIZE (8192 * 1024 * 4)				// 32 MB	<-- ABSOLUTELY MUST be a multiple of VECTOR_STRING_BUFFER_STEP_SIZE, or else program will break in certain scenarios.
#define VECTOR_STRING_BUFFER_START_SIZE 8192						// NOTE: There shouldn't be a reason for these sizes to coincide with file sys block size like those above, since these are purely in memory. Just in case (and because there is no downside in this case), we set them to the same ones as above.
#define VECTOR_STRING_BUFFER_STEP_SIZE 8192							// NOTE: I figure that, in case the OS needs to write some RAM to the swap space on the hard disk, this will make our memory recoverable in a more efficient way, possibly giving us a marginal speed-up. This theory might not even be a bad one, but I don't have proof.

// NOTE: AFAIK, the file system block size is a unit of work for the file system. File reads are done in blocks which are the size of the file system block size.
// If the buffer isn't a multiple of the file system block size, we could end up reading way more data from the file than we need (1 extra block), even if we're just a couple of bytes
// over the nearest block size multiple. We try to avoid that by tuning the values. It's essentially the same deal as with memory alignment in programming, we want to optimize it to avoid unnecessary reads.
// NOTE: The block size of the underlying device (HDD or SSD for example) is often 512 bytes, but the logical (file system) block size is often bigger (4 or 8 KB). It is often the same as the page size for virtual memory.

// TODO: Technically, it would be optimal to just straight up read the block size from the OS, you would just have to implement it two times (Windows and Linux). Shouldn't be too hard though, consider doing it in the future.

#define HISTORY_BUFFER_MAX_LINE_COUNT 63															// Maximum num of lines storable in history buffer when using --context x flag. Note that the count of history buffer is HISTORY_BUFFER_MAX_LINE_COUNT + 1 because of circular buffer.

#include <csignal>																					// This block is for signalling and polling things.
#ifndef PLATFORM_WINDOWS
#include <sys/signalfd.h>
#include <poll.h>
#endif

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN																			// Include Windows.h to get access to the few winapi functions that we need, such as the ones for getting console handles and setting ANSI escape code support.
#include <Windows.h>
#endif

#include <cstdlib>																					// Needed for malloc, free and realloc functions.

#ifdef PLATFORM_WINDOWS
#include <io.h>																						// Needed for _isatty function.
#define isatty(x) _isatty(x)																		// Renaming _isatty to isatty so it's the same as the function call in Linux.
#define crossplatform_read(...) _read(__VA_ARGS__)
#define crossplatform_write(...) _write(__VA_ARGS__)
#else
#include <unistd.h>																					// Linux isatty function is in here as well as some other useful stuff for this program as well I think.
#define crossplatform_read(...) read(__VA_ARGS__)
#define crossplatform_write(...) write(__VA_ARGS__)
#endif

// TODO: Why do we need cstdio?
#include <cstdio>																					// NOTE: Use "c___" headers instead of "____.h" headers whenever possible. The "c___" counterparts minimize global scope pollution by putting a lot of things in namespaces and replacing some #defines with functions. It's more "correct".
#include <iostream>																					// For std::cout. We don't use std::cin or std::cerr because we handle both of those pathways through raw syscalls and custom buffering.

#include <cstring>																					// For some weird reason, std::memcpy is located inside this header file, which means we should include it. We also use std::strlen, which is also in this header. We might use a couple other things from here, not sure.

#include <regex>																					// For regex support. We don't currently do the regex parsing and searching ourselves. Perhaps that will be a future improvement.

#include <new>																						// For std::nothrow, which we use in "new (std::nothrow) x" statements to avoid exceptions.

#ifdef PLATFORM_WINDOWS																				// These #defines are already defined in one of the Linux-only headers, but for Windows, we need to explicitly do it.
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

/*

NOTE: I've just learned about compiler intrinsics and SIMD and such and I'm going to write it here so I have a reference for later and don't forget:
So SIMD is often mentioned as a way to speed up computations and you should be aware of it so that you might use it in the future to speed up some of your algorithms.
IMPORTANT: A fair amount of SIMD instructions are already generated by the compiler when compiling normal code, because the compiler will optimize things that it can do better with SIMD. That means that you need not worry for a lot of calculations, because the optimizer has your back AFAIK.
SIMD stands for Single Instruction Multiple Data. It means that, rather than operating on one word or one unit of data per instruction, one instruction will alter a whole array (vector) of words or units in one go. This is awesome because it requires the same amount of time but can make your programs
a lot faster (potentially). That means, that for a lot of simple additions and multiplications and what not inside of functions, you're actually wasting processing power because you could be doing four at a time.
Obviously, that means that you should theoretically use SIMD everywhere, but that would be a major hassle and potentially screw up some of your cross platform-ness. For those simple optimizations, I would think it would be pretty safe to rely on the compiler to group and optimize certain calculations.
SIMD is super useful for stuff like going through arrays with loops. The practice of loop unrolling already exists to make this process more efficient (calculating four elements and then moving on to the next four for example), but this can be made even better by having those four elements be handled by a single SIMD
instruction.
IMPORTANT: If the heavy lifting inside of your algorithms is done using standard library functions, you can rest easy AFAIK, because they are most likely implemented as efficiently as possible and are tailored to your system. That means SIMD is most likely used to it's full potential in those functions.
There are different instructions sets that got added to CPU's over the years, each expanding the SIMD functionality of the CPU's. For example, SSE (Streaming SIMD Extentions) is one of those, it has different versions as well.
You can basically rely on the fact that CPU's are super backwards compatible and that if a SIMD instruction set was once implemented, it will still be there along with whatever new instruction sets have been added since.
IMPORTANT: SIMD is not cross platform technically, you have to specialize your code to use a specific SIMD instruction set, which depends on the architecture that you are using. In practice, this doesn't matter all too much because, if you choose an instruction set that is old enough (doesn't even have to be that old),
it'll be supported by all the CPU's that you would ever really want to run your code on.
To use a SIMD instruction set, you have to include the required header. This header will be filled with what are known as compiler intrinsics, which are, AFAIK, functions that are specific to the compiler and, in this case, translate directly into SIMD instructions.
There are different headers for different instruction sets, you have to google around to find the one you want to use. The recommended one for standard SIMD stuff is immintrin.h.
The header exists for both MSVC and gcc and includes all the other headers you would probably want, which means it includes basically all the other instruction sets. The header will be made available if the intrinsics are available for the system you are compiling for.
Since this header has so many different versions, it will always be available AFAIK, some intrinsics inside may not be available though because they might not be supported AFAIK.
The documentation for this stuff is lacking in my opinion, so researching is a slight pain, but it is very much worth it to implement SIMD instructions if you come across the right spot in code, because it could dramatically speed up your processing.

Heres an interesting way to look at things:
Normal CPU instructions --> super linear, theoretically not vectorized (they don't behave like SIMD instructions), although I'm not sure how many normal instructions make use of vectorization, it does seems to be the way CPU's are heading.
SIMD instructions --> useful, but relatively minimal vectorization (usually 4 or 8 words per instruction AFAIK)
GPU code with OpenCL for example --> A huge amount of computations at the same time, not because of instructions that manipulate multiple data, but because of the crazy amount of CPU cores.
The three types of programming can sort of be seen as in a heirarchy or a ladder or something. Why would you ever use SIMD over OpenCL? SIMD has so much less overhead than queueing kernels in OpenCL AFAIK. SIMD is super useful when you want to vectorize small amounts of things.
If you want to process a huge array in parallel, GPU computing is the way to go, instead of using SIMD.

Another concept that builds on SIMD: SWAR (SIMD Within A Register):
Very simple AFAIK. It is basically just more instructions, which also act like SIMD instructions, but they use data units that are smaller than normal for example (or registers that are larger than normal), to process multiple data within the one register.
Effectively does the same thing as SIMD, I'm not really sure why it warrants it's own acronym.

Beneifts of SWAR over SIMD: The benefits come only from the fact that, through sacrificing resolution of your values (moving from whole words to partial words), you can process more values in parallel AFAIK, which is faster.

*/

#define static_strlen(x) (sizeof(x) - 1)

// ANSI escape code helpers.
#define ANSI_ESC_CODE_PREFIX "\033["
#define ANSI_ESC_CODE_SUFFIX "m"

const char* helpText = "grep accepts text as input and outputs the lines from the input that have the specified keyphrase in them\n" \
					   "\n" \
					   "usage: grep [-calv] [--context <amount> || --only-line-nums || --stdout-color <auto|on|off> || --stderr-color <auto|on|off>] <regex keyphrase>\n" \
					   "       grep <--help || --h>            -->            shows help text\n" \
					   "\n" \
					   "arguments:\n" \
							"\t-c							   -->         be case sensitive when matching\n" \
							"\t-a                              -->         print all lines from input but still color matches\n" \
							"\t-l                              -->         print line numbers\n" \
							"\t-v                              -->         invert output - print lines that would normally be omitted and omit lines that would normally be printed\n" \
							"\t--context <amount>              -->         print the specified amount of context (in lines) around each matched line\n" \
							"\t--only-line-nums                -->         print only the line numbers, not the actual lines\n" \
							"\t--stdout-color <auto|on|off>    -->         force a specific stdout coloring behaviour, auto is default\n" \
							"\t--stderr-color <auto|on|off>    -->         force a specific stderr coloring behaviour, auto is default\n";

// Flag to keep track of whether we should color output or not.
bool isOutputColored;
bool isErrorColored;

// Output coloring.
namespace color {
	const char red[] = ANSI_ESC_CODE_PREFIX "31" ANSI_ESC_CODE_SUFFIX;
	const char reset[] = ANSI_ESC_CODE_PREFIX "0" ANSI_ESC_CODE_SUFFIX;
}

// SIDE-NOTE: const char* const instead of const char* doesn't always work. In the cases where you intend to change what the const char* pointer points to, const char* allows that while const char* const doesn't. In this case, const char* const is absolutely fine, but a lot of people still don't write it because personal preference and style.

// This function makes it easy to report errors. It handles the coloring for you, as well as the formatting of the error string.
template <size_t N>
void reportError(const char (&msg)[N]) {																		// NOTE: The reason we don't just use the std::cerr output buffer to format our string, and then flush it after we're done formatting, is because that would presumably be slower than this setup.
	if (isErrorColored) {
		// Construct a buffer to hold the finished error message.
		char buffer[static_strlen(color::red) + static_strlen("ERROR: ") + N - 1 + static_strlen(color::reset) + 1];

		// Copy in the ANSI code for red color.
		std::memcpy(buffer, color::red, static_strlen(color::red));												// NOTE: Technically, it would be more efficient to write "ERROR: " in every error message individually, but that probably means the executable is larger because of the extra .rodata data, which is undesirable.
																												// NOTE: We could also combine the color codes with the "ERROR: " string in order to have less copy operations and simpler code, but that has the same issue as above.
		// Copy in the ERROR tag.
		std::memcpy(buffer + static_strlen(color::red), "ERROR: ", static_strlen("ERROR: "));					// NOTE: memcpy is the C-style version of the function and std::memcpy is the C++-style version of the function. They're both literally the same function in every single way, but I'm going to use std because it's more "proper".

		// Copy in the actual error message.
		std::memcpy(buffer + static_strlen(color::red) + static_strlen("ERROR: "), msg, N - 1);					// SIDE-NOTE: You cannot combine const char[]'s like you can combine string literals ("hi" "there" <-- concatination), even though string literals explicitly have the type const char[]. It's better this way, it makes more sense.

		// Copy in the ANSI code for color reset.
		std::memcpy(buffer + static_strlen(color::red) + static_strlen("ERROR: ") + N - 1, color::reset, static_strlen(color::reset));

		// Add a newline to the end of the message.
		buffer[static_strlen(color::red) + static_strlen("ERROR: ") + N - 1 + static_strlen(color::reset)] = '\n';

		// Write the message to stdout.
		crossplatform_write(STDERR_FILENO, buffer, sizeof(buffer));
		// NOTE: We would technically have to buffer the error output if other buffered error output was present till this point (for temporal consistency). Since we only ever write unbuffered error output, this doesn't concern us.
		// NOTE: It might be theoretically better to do buffered error output because buffered stdout output could have happened before this (to ensure temporal consistency with the stdout output, which should get outputted earlier than us), but I don't think this consistency matters, especially when error gets piped somewhere else.

		// NOTE: No need to handle the errors that could happen here. First of all, we're already reporting an error, so we're exiting ASAP anyway. Second of all, if error reporting throws an error, how are we supposed to report that anyway? Doing nothing here is the most efficient at no real cost to the experience.
		return;
	}

	// Uncolored version of the error message generation.
	char buffer[static_strlen("ERROR: ") + N - 1 + 1];

	std::memcpy(buffer, "ERROR: ", static_strlen("ERROR: "));

	std::memcpy(buffer + static_strlen("ERROR: "), msg, N - 1);

	buffer[static_strlen("ERROR: ") + N - 1] = '\n';

	crossplatform_write(STDERR_FILENO, buffer, sizeof(buffer));									// NOTE: Intellisense is complaining about this line, even though PVS-Studio has nothing against it. I can't find any issue either, so I'm just going to leave it like this.
}

// Another version of the above function specifically for handling regex errors. This is necessary because the length of the regex errors isn't known at compile-time.
void reportRegexError(const char* msg) {
	size_t msgLength = std::strlen(msg);

	if (isErrorColored) {
		size_t bufferSize = static_strlen(color::red) + static_strlen("ERROR: regex error: ") + msgLength + static_strlen(color::reset) + 1;
		char* buffer = new char[bufferSize];

		std::memcpy(buffer, color::red, static_strlen(color::red));

		std::memcpy(buffer + static_strlen(color::red), "ERROR: regex error: ", static_strlen("ERROR: regex error: "));

		std::memcpy(buffer + static_strlen(color::red) + static_strlen("ERROR: regex error: "), msg, msgLength);

		std::memcpy(buffer + static_strlen(color::red) + static_strlen("ERROR: regex error: ") + msgLength, color::reset, static_strlen(color::reset));

		buffer[static_strlen(color::red) + static_strlen("ERROR: regex error: ") + msgLength + static_strlen(color::reset)] = '\n';

		crossplatform_write(STDERR_FILENO, buffer, bufferSize);

		delete[] buffer;
		return;
	}

	// Uncolored version of the error message generation.
	size_t bufferSize = static_strlen("ERROR: regex error: ") + msgLength + 1;
	char* buffer = new char[bufferSize];

	std::memcpy(buffer, "ERROR: regex error: ", static_strlen("ERROR: regex error: "));				// NOTE: Intellisense doesn't appreciate this line, but PVS-Studio doesn't have anything against it and I can't see any errors, so I'm leaving it like this.

	std::memcpy(buffer + static_strlen("ERROR: regex error: "), msg, msgLength);

	buffer[static_strlen("ERROR: regex error: ") + msgLength] = '\n';

	crossplatform_write(STDERR_FILENO, buffer, bufferSize);

	delete[] buffer;
}

// Signal handling.
#ifdef PLATFORM_WINDOWS																// Only needed on Windows because we signal for the main loop to stop in Linux via artifical EOF signal.
bool shouldLoopRun = true;
void signalHandler(int signum) { shouldLoopRun = false; }							// NOTE: SIGINT exists on Windows, but SIGTERM doesn't. SIGTERM is replaced by SIGBREAK on Windows. Also, SIGINT signal handler gets run on a separate thread, unlike the signal handlers for all the other signals. Why? I have no idea.
#endif

// Collection of flags. These correspond to a subset of the command-line flags you can set for the program. The other flags that aren't in the following namespace are handled differently and don't need any global variables.
namespace flags {
	bool caseSensitive = false;
	bool allLines = false;
	bool lineNums = false;
	bool inverted = false;
	bool context = false;
	bool only_line_nums = false;
}

// Keeps track of the current line number.
size_t lineCounter = 1;

// This class is a replacement for std::string. We use it because it has (AFAIK) more efficient character addition functions.
class VectorString {
public:
	char* buffer;
	size_t bufferSize = VECTOR_STRING_BUFFER_START_SIZE;
	size_t bufferWriteHead = 0;						// TODO: Change buffer to data and rework the variable naming a little. Also, redo the defines at the top of the file if necessary and check other names that need to be checked as well.

	// Initialize the VectorString. This consists solely of allocating space on the heap for the data array.
	bool init() {
		buffer = (char*)malloc(VECTOR_STRING_BUFFER_START_SIZE);						// SUPER-IMPORTANT-NOTE: We are forced to use malloc here since we use realloc in the following bit of code. Mixing C-style and C++-style allocations is UB (data structures and/or the heap location could be different, etc...). This is the only way.
		if (!buffer) {
			reportError("failed to allocate a vector string buffer");
			return false;
		}
		return true;
	}

	bool enoughSpaceForAddition(size_t additionSize) { return bufferSize - bufferWriteHead >= additionSize; }

	// NOTE: The following are super important points about reallocation of data, because there are some nuances to how things USUALLY behave on MOST hardware:
	// realloc reallocates a block of memory with a different size and copies all of your data into the new block.
	//			--> it has two behaviours (in the usual case):		1. scale the current allocation up or down if there is enough room in your partition of the heap (there is always enough room if you're scaling down).
	//																2. else: free the current allocation and allocate a new block with the requested size. THE DATA IS COPIED FROM ONE BLOCK TO ANOTHER --> WARNING: if you scale down, data will be truncated.
	// SIDE-NOTE: When data needs to be copied from block to block, realloc is still faster than free/malloc because optimizations can be implemented when both free and malloc are garanteed to happen right after one another inside of realloc.
	// I presume, that instead of worrying about getting everything into a valid state after free(), you can go straight from one block allocation to the other, that's what I'm talking about with optimizations.
	// Other than that, some OS's (Linux for example) will avoid copying the data even in the case that the block position changes. The genius solution: remap the new virtual memory space to the physical memory space of the old allocation location.
	// By doing that, you've copied data with essentially no overhead relative to the original copy approach.
	//
	// IMPORTANT: BOTTOM-LINE: realloc is essentially always faster when you want to preserve the data. Use it in those cases.
	//
	// realloc isn't faster when you don't care about the data though, because realloc always copies the block data, which would in that case be super unnecessary.
	// NOTE: I don't know how this behaves with the virtual memory optimization, maybe realloc should always be used in that case, but I'm going to assume that even in that case, realloc is suboptimal.
	// SOLUTION: Use free/malloc when scaling a block up when you don't care about the data. Use realloc when scaling a block down when you don't care about the memory.
	// NOTE: If you're reasonably certain that the block will scale up without repositioning itself, the better option would almost definitely be realloc, but if you can't garantee that, free/malloc is almost definitely better.
	//			NOTE: According to a benchmark I found, realloc is ~7600x slower than free/malloc for scaling up when repositioning happens. It seems to be about 40 times faster when repositioning doesn't happen.
	//			NOTE: So don't orient yourself by seeing if repositioning happens less than 50% of time. It needs to happen a lot less for realloc to be worth it. Calculate it.
	//
	// EXTRA: If you don't know whether your scaling the block up or down, free/malloc, because of the speed data given above this makes the most sense in these situations.

	bool allocateMoreSpace() {
		if (bufferSize == VECTOR_STRING_BUFFER_MAX_SIZE) { return false; }								// NOTE: This comparison is possible because VECTOR_STRING_BUFFER_MAX_SIZE should always be a multiple of VECTOR_STRING_BUFFER_STEP_SIZE.
		bufferSize += VECTOR_STRING_BUFFER_STEP_SIZE;
		char* tempBuffer = (char*)realloc(buffer, bufferSize);											// NOTE: Use realloc because we want to keep the data in the VectorString.
		if (tempBuffer) { buffer = tempBuffer; return true; }
		bufferSize -= VECTOR_STRING_BUFFER_STEP_SIZE;													// NOTE: Technically, we don't need to leave the object in a valid state if we fail, since the program always exits soon after, but for future expandability, I'm going to leave it in.
		reportError("ran out of heap memory while enlarging VectorString");
		return false;
	}

	// The following functions all add characters to the VectorString. There are so many because depending on the amount you want to add, there are different optimal methods.

	bool operator+=(const char& character) {
		if (enoughSpaceForAddition(sizeof(character))) { addition: buffer[bufferWriteHead] = character; bufferWriteHead++; return true; }
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool operator+=(const uint16_t& characters) {
		if (enoughSpaceForAddition(sizeof(characters))) { addition: *(uint16_t*)(buffer + bufferWriteHead) = characters; bufferWriteHead += sizeof(characters); return true; }
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool operator+=(const uint32_t& characters) {
		if (enoughSpaceForAddition(sizeof(characters))) { addition: *(uint32_t*)(buffer + bufferWriteHead) = characters; bufferWriteHead += sizeof(characters); return true; }
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool operator+=(const uint64_t& characters) {
		if (enoughSpaceForAddition(sizeof(characters))) { addition: *(uint64_t*)(buffer + bufferWriteHead) = characters; bufferWriteHead += sizeof(characters); return true; }
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool write3(const char* characters) {
		if (enoughSpaceForAddition(3)) {
		addition:
			char* position = buffer + bufferWriteHead;
			*(uint16_t*)(position) = *(uint16_t*)characters;
			*(position + 2) = *(characters + 2);
			bufferWriteHead += 3;
			return true;
		}
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool write5(const char* characters) {
		if (enoughSpaceForAddition(5)) {
		addition:
			char* position = buffer + bufferWriteHead;
			*(uint32_t*)(position) = *(uint32_t*)characters;
			*(position + 4) = *(characters + 4);
			bufferWriteHead += 5;
			return true;
		}
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool write6(const char* characters) {
		if (enoughSpaceForAddition(6)) {
		addition:
			char* position = buffer + bufferWriteHead;
			*(uint32_t*)(position) = *(uint32_t*)characters;
			*(uint16_t*)(position + 4) = *(uint16_t*)(characters + 4);
			bufferWriteHead += 6;
			return true;
		}
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	bool write7(const char* characters) {
		if (enoughSpaceForAddition(7)) {
		addition:
			char* position = buffer + bufferWriteHead;
			*(uint32_t*)(position) = *(uint32_t*)characters;
			*(uint16_t*)(position + 4) = *(uint16_t*)(characters + 4);
			*(position + 6) = *(characters + 6);
			bufferWriteHead += 7;
			return true;
		}
		if (allocateMoreSpace()) { goto addition; }
		return false;
	}

	void clear() { bufferWriteHead = 0; }

	void release() { delete[] buffer; }																	// NOTE: delete and delete[] can throw exceptions, but only if the destructors of the objects in question throw exceptions (which isn't encouraged). In this situation, since the object in question is char, this never throws exceptions.
};

// Keeps track of previous lines that didn't contain any matches. Very useful for operation with the --context flag but also keeps track of the one and only current line when in normal operation (for efficiency).
// Implemented as a circular buffer.
class HistoryBuffer {
	static unsigned int buffer_len;
	static unsigned int beginIndex;

public:
	static VectorString* buffer;
	static unsigned int buffer_lastIndex;
	static unsigned int amountFilled;																	// This variable isn't always used, only for specific modes where keeping track of amount of filled slots is the most efficient way.
	static unsigned int index;

	static void incrementAmountFilled() { if (amountFilled != buffer_lastIndex) { amountFilled++; } }

	static void purgeAmountFilled() { amountFilled = 0; }

	// Initialize HistoryBuffer. We do this by initializing all the necessary VectorStrings after allocating them. If one initialization fails, we release all the initializations we've done so far, free memory, and exit the program.
	// We can exit directly from this function because no other memory needs disposing of at this point.
	static void init() {
		buffer_len = buffer_lastIndex + 1;
		buffer = new VectorString[buffer_len];
		for (unsigned int i = 0; i < buffer_len; i++) {
			if (!buffer[i].init()) {
				for (unsigned int j = 0; j < i; j++) { buffer[j].release(); }
				delete[] buffer;
				exit(EXIT_FAILURE);														// NOTE: We use EXIT_FAILURE here because this isn't directly the user's fault. This error happens when too little memory is available on the system, so it's an environment problem. We denote those with EXIT_FAILURE.
			}
		}
	}

	// Push the current line onto the historical partition of the history buffer.
	static void push() {
		if (index == beginIndex - 1) {
			if (beginIndex == buffer_lastIndex) { index++; beginIndex = 0; return; }
			index++; beginIndex++; return;
		}
		if (index == buffer_lastIndex) { if (beginIndex == 0) { beginIndex = 1; } index = 0; return; }
		index++;
	}

	static void pushWithAmountInc() { push(); if (amountFilled != buffer_lastIndex) { amountFilled++; } }

	// Clear the history buffer.
	static void purge() { index = beginIndex; }

	// Purge while also resetting amountFilled.
	static void purgeWithAmountSet() { purge(); purgeAmountFilled(); }

	// Print all the lines in the historical partition of the HistoryBuffer to the console.
	static void print() { for ( ; beginIndex != index; beginIndex = (beginIndex == buffer_lastIndex ? 0 : beginIndex + 1)) { std::cout.write(buffer[beginIndex].buffer, buffer[beginIndex].bufferWriteHead) << '\n'; } }
	// NOTE: It is important to use '\n' instead of std::endl because std::endl puts a newline into the stream but also flushes it, which is a syscall, which is super slow. '\n' doesn't flush, so faster.

	// NOTE: We could store the amount filled and edit the variable with every call to push(), but I doubt that would be efficient since push is called so many more times than print is (at least normally).
	// Doing that would cause amountFilled to be added to over and over with little return on the investment. That's why we're just calculating amountFilled every time we need it, that way push() doesn't have to waste it's time incrementing anything.
	// NOTE: The exceptions to that rule are the peek functions. They're used for flags::inverted, where it's reasonable to cache amountFilled, so we do it there. Not for normal prints though.
	// NOTE: Another exception to that rule is flags::only_line_nums. There, the circular buffer isn't used and amountFilled is also the most efficient way of handling things. This is the case for printLineNums() and a couple functions at the top of the class.
	static unsigned int calculateAmountFilled() { return index > beginIndex ? index - beginIndex : buffer_len - beginIndex + index; }

	static void printLinesWithLineNums() {
		if (index == beginIndex) { return; }
		lineCounter -= calculateAmountFilled();
		(std::cout << lineCounter << ' ').write(buffer[beginIndex].buffer, buffer[beginIndex].bufferWriteHead) << '\n';
		lineCounter++;
		beginIndex = (beginIndex == buffer_lastIndex ? 0 : beginIndex + 1);
		for ( ; beginIndex != index; beginIndex = (beginIndex == buffer_lastIndex ? 0 : beginIndex + 1), lineCounter++) { (std::cout << lineCounter << ' ').write(buffer[beginIndex].buffer, buffer[beginIndex].bufferWriteHead) << '\n'; }
	}

	static void lastPrintLineNums() { for (size_t historyLine = lineCounter - amountFilled; historyLine < lineCounter; historyLine++) { std::cout << historyLine << '\n'; } }
	static void printLineNums() { lastPrintLineNums(); purgeAmountFilled(); }

	static bool peekSafestLine(VectorString*& safestLine) { if (amountFilled == buffer_lastIndex) { safestLine = buffer + beginIndex; return true; } return false; }

	static bool peekSafestLineNum(size_t& safestNum) { if (amountFilled == buffer_lastIndex) { safestNum = lineCounter - buffer_lastIndex; return true; } return false; }

	// Release all the VectorStrings before freeing the memory in which they reside.
	static void release() { for (unsigned int i = 0; i < buffer_len; i++) { buffer[i].release(); } delete[] buffer; }			// TODO: Make sure this is getting release everywhere where it's supposed to.
};

VectorString* HistoryBuffer::buffer;
unsigned int HistoryBuffer::buffer_len;
unsigned int HistoryBuffer::index = 0;
unsigned int HistoryBuffer::beginIndex = 0;
unsigned int HistoryBuffer::buffer_lastIndex;
unsigned int HistoryBuffer::amountFilled = 0;

// SIDE-NOTE: While writing the testing system for this program, I stumbled on a bunch of issues. The main problem was that powershell doesn't just pipe the data to and from programs. It will split it on the newlines and construct a list of strings that it will pass to and from programs.
// I don't exactly know why, probably because Powershell deals with objects instead of raw data when it transfers data, so it tries to convert between the two.
// This was one of the super cool things about powershell that was advertised, but it's kind of stupid in a lot of scenarios. Anyway, when piping the string array into another program, powershell concatinates the strings into one giant string that just has a lot of lines.
// It would be cool if this string was the same as the original string that powershell accepted from this grep program, but it sadly isn't in some cases. The strings are all fitted with a line ending at the end. This is a huge issue when you're trying to write a line that doesn't end the line at the end.
// For me, this became an issue when I tried to write an ANSI escape code for resetting the text color after the last newline. Powershell put another newline at the end, making there be an extra line at the end of the output file.
// To solve this, I just wrote the testing system in C++ and did the piping myself. Powershell just wasn't cutting it. Anyway, it's important that you remember this behaviour that you experienced while doing this project.
// Also, powershell behaves weird in other ways as well. The Out-File and the > and >> operators all write data in 16-bit wide format, which means half your file is just NUL characters. Really stupid but that's the way it goes. You can override this with the -Encoding ascii flag on Out-File, but the > and >> operators just aren't changeable.

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
			reportError("one or more flag arguments are invalid");
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

unsigned int parseUInt(char* string) {
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
	reportError("invalid value for --context flag");
	exit(EXIT_SUCCESS);
}

// NOTE: I have previously only shown help when output is connected to TTY, so as not to pollute stdout when piping. Back then, help was shown sometimes when it wasn't requested, which made it prudent to include that feature. Now, you have to explicitly ask for help, making TTY branching unnecessary.
void showHelp() { std::cout << helpText; }

bool forcedOutputColoring;
bool forcedErrorColoring;								// NOTE: GARANTEE: If something goes wrong while parsing the cmdline args and an error message is necessary, the error message will always be printed with the default error coloring (based on TTY/piped mode).

// Parse flags at the argument level. Calls parseFlagGroup if it encounters flag groups and handles word flags (those with -- in front) separately.
unsigned int parseFlags(int argc, char** argv) {																// NOTE: If you write --context twice or --stdout-color or --stderr-color twice (or any additional flags that we may add), the value supplied to the rightmost instance will be the value that is used. Does not throw an error.
	for (int i = 1; i < argc; i++) {
		const char* arg = argv[i];
		if (arg[0] == '-') {
			if (arg[1] == '-') {
				const char* flagTextStart = arg + 2;
				if (*flagTextStart == '\0') { continue; }

				if (!strcmp(flagTextStart, "context")) {
					i++;
					if (i == argc) {
						reportError("the --context flag was not supplied with a value");
						exit(EXIT_SUCCESS);
					}
					HistoryBuffer::buffer_lastIndex = parseUInt(argv[i]);
					if (HistoryBuffer::buffer_lastIndex == 0) { continue; }															// Context value 0 is the same as no context, so don't bother with doing any context calculations while in the main loops.
					flags::context = true;				// TODO: This doesn't obey the right hand rule thing.
					continue;
				}

				if (!strcmp(flagTextStart, "only-line-nums")) { flags::only_line_nums = true; continue; }

				if (!strcmp(flagTextStart, "stdout-color")) {
					i++;
					if (i == argc) {
						reportError("the --stdout-color flag was not supplied with a value");
						exit(EXIT_SUCCESS);
					}
					if (!strcmp(argv[i], "on")) { forcedOutputColoring = true; continue; }
					if (!strcmp(argv[i], "off")) { forcedOutputColoring = false; continue; }
					if (!strcmp(argv[i], "auto")) { forcedOutputColoring = isOutputColored; continue; }
					reportError("invalid value for --stdout-color flag");
					exit(EXIT_SUCCESS);
				}

				if (!strcmp(flagTextStart, "stderr-color")) {
					i++;
					if (i == argc) {
						reportError("the --stderr-color flag was not supplied with a value");
						exit(EXIT_SUCCESS);
					}
					if (!strcmp(argv[i], "on")) { forcedErrorColoring = true; continue; }
					if (!strcmp(argv[i], "off")) { forcedErrorColoring = false; continue; }
					if (!strcmp(argv[i], "auto")) { forcedErrorColoring = isErrorColored; continue; }
					reportError("invalid value for --stderr-color flag");
					exit(EXIT_SUCCESS);
				}

				if (!strcmp(flagTextStart, "help")) { showHelp(); exit(EXIT_SUCCESS); }
				if (!strcmp(flagTextStart, "h")) { showHelp(); exit(EXIT_SUCCESS); }

				reportError("one or more flag arguments are invalid");
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
		reportError("too few arguments");
		exit(EXIT_SUCCESS);
	case 1:
		{																											// Unnamed namespace because we can't create variables inside switch cases otherwise.
			std::regex_constants::syntax_option_type regexFlags = std::regex_constants::grep | std::regex_constants::nosubs | std::regex_constants::optimize;
			if (!flags::caseSensitive) { regexFlags |= std::regex_constants::icase; }
			try { keyphraseRegex = std::regex(argv[keyphraseArgIndex], regexFlags); }								// Parse regex keyphrase.
			catch (const std::regex_error& err) {																	// Catch any errors relating to keyphrase regex syntax and report them.
				reportRegexError(err.what());
				exit(EXIT_SUCCESS);
			}

			bool previousColoring = isOutputColored;
			isOutputColored = forcedOutputColoring;																	// If everything went great with parsing the cmdline args, finally set output coloring to what the user wants it to be. It is necessary to do this here because of the garantee that we wrote above.
			forcedOutputColoring = previousColoring;																// We repurpose forcedOutputColoring to indicate if there is a tty connected to stdout. This is helpful to some later parts of the code.

			previousColoring = isErrorColored;																		// Same deal as above, but with stderr.
			isErrorColored = forcedErrorColoring;
			forcedErrorColoring = previousColoring;
		}
		return;
	default:																										// If more than 1 non-flag argument exists (includes flags after first non-flag arg), throw error.
		reportError("too many arguments");
		exit(EXIT_SUCCESS);
	}
}

// The following typedef exists on Linux, but not on Windows, so we create it here.
#ifdef PLATFORM_WINDOWS
typedef int ssize_t;
#endif

// Handles input in a buffered, cross-platform way.
class InputStream {
#ifndef PLATFORM_WINDOWS
	static pollfd fds[2];																							// File descriptors to poll. One for stdin and one for a signal fd.
	static sigset_t sigmask;																						// Signals to handle with poll.
#endif

	static char* buffer;
	static ssize_t bufferSize;

	static ssize_t bytesRead;																						// Position of read head in buffer.
	static ssize_t bytesReceived;																					// Position of write head in buffer.

	static bool isReadPositionUnaligned;																			// Flag signals when the current read position is not on an 8-byte boundary.

public:
	static void init() {
		buffer = new char[INPUT_STREAM_BUFFER_START_SIZE];															// Initialize buffer with the starting amount of memory.
		
#ifndef PLATFORM_WINDOWS
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
#endif
	}

	// NOTE: I think we probably could have just used the standard input buffering mechanisms (while retaining the ability to adjust buffer size) instead of building our own.
	// NOTE: That would probably be faster since the standard is optimized for each platform. I'm too proud of my mechanism to remove it right now, so I'm leaving it in for the forseeable future.
	// NOTE: Plus, I haven't done any benchmarks. Maybe this is faster than the standard because the standard might do a bunch of safety checks and unnecessary stuff.

	// NOTE: If this function returns false, no garuantees are made about the validity and reusability of the class instance. Don't rely on either of those things.
	// NOTE: I'm assuming that multiple copies of refillBuffer are generated, one for when the offset is 0 and one for when the offset is variable. That would be an optimization that I would expect the compiler to do. It would make the code faster.
	static bool refillBuffer(size_t offset = 0) {			// TODO: Check for memory leaks with visual studio after going through the code.

		// NOTE: In Windows, the following buffer expansion check basically never triggers, since the bytesReceived are almost always less than the total buffer size. The reason being that the total buffer size is used to determine the amount of bytes to read, but the amount of characters actually put into the buffer
		// NOTE: is determined by how many \r\n's needed to be replaced with \n. Because of this discrepency, there is no good way to do the buffer expansion AFAIK. Still, I'm going to keep the check in, in case you decide to grep an input stream that only has \n.
		// NOTE: If the ecosystem starts to stop being stupid and stops supporting \r\n in the future, having this code around will be helpful then.
		// NOTE: Linux doesn't have this problem, not because of Linux itself, but because there are almost no files that use \r\n there.

		// NOTE: We use free/malloc here instead of realloc because that is usually faster than realloc when you don't want to copy the contents of the allocation. That means that this block has to be before the following block, or else it will be filled and we would have to use realloc, which would be slower.
		if (bytesReceived == bufferSize) {																			// Make buffer bigger if it is filled with one read syscall. This minimizes amount of syscalls we have to do. Buffer doesn't have the ability to get smaller again, doesn't need to.
			if (bufferSize == INPUT_STREAM_BUFFER_MAX_SIZE) {														// NOTE: This comparison is only possible because INPUT_STREAM_BUFFER_MAX_SIZE is a multiple of INPUT_STREAM_BUFFER_SIZE_STEP.
				delete[] buffer;																					// Delete current allocation to make room for the reallocation.
				bufferSize += INPUT_STREAM_BUFFER_SIZE_STEP;
				buffer = new (std::nothrow) char[bufferSize];														// Try to allocate new block with increased size.
				if (!buffer) {																						// If we fail, try to allocate the old block again in order to get back to how things were.
					bufferSize -= INPUT_STREAM_BUFFER_SIZE_STEP;
					buffer = new (std::nothrow) char[bufferSize];								// NOTE: Because of the way this works, if the heap doesn't have any memory left, every call to refillBuffer from then on will have some extra overhead. INPUT_STREAM_BUFFER_MAX_SIZE should prevent this though, so no big deal.
					if (!buffer) {																					// If that fails and somehow the heap doesn't have any storage left even though it did a moment ago, throw an error.
						// NOTE: This error shouldn't ever happen theoretically, but I'm leaving it in in case some part of stdlib on a different thread or a debugger decides to change the heap while we're in the middle of reallocating.
						reportError("reallocation of freed buffer memory failed, heap space snatched away by an unknown entity");
						return false;
					}
				}
			}
		}

#ifndef PLATFORM_WINDOWS
		// When no more data left in buffer, try get more.
		if (poll(fds, 2, -1) == -1) {																				// Block until we either get some input on stdin or get either a SIGINT or a SIGTERM.
			reportError("failed to poll stdin, SIGINT and SIGTERM");
			return false;
		}

		if (fds[1].revents) { return false; }																		// Signal EOF if we caught a signal.
#endif

		bytesReceived = offset + crossplatform_read(STDIN_FILENO, buffer + offset, bufferSize - offset);			// Read as much as we can fit into the buffer.

		if (bytesReceived == 0) { return false; }																	// In case of actual EOF, signal EOF.
		if (bytesReceived == -1) {																					// In case of error, log and signal EOF.
			reportError("failed to read from stdin");
			return false;
		}
		bytesRead = 0;																								// If new data is read, the read head needs to be reset to the beginning of the buffer.

		return true;
	}

	// NOTE: If this function returns false, no garuantees are made about the validity and reusability of the class instance. Don't rely on either of those things.
	static bool readLine(VectorString& line) {																		// Returns true on success. Returns false on EOF or error in Windows. Returns false on EOF or SIGINT or SIGTERM or error on Linux.
		// NOTE: We could implement transfers that are more than one byte wide in this alignment code like we did in the transfer code that comes after, but that would be a lot of work and produce a lot of code.
		// TODO: There might be a way to implement it that I haven't thought of, but as it stands, I'm leaving it as a future improvement.
		if (isReadPositionUnaligned) {
			ssize_t limiter = bytesRead + 8 - (bytesRead % 8) - 1;													// TODO: Research if there is a more efficient way to do this. There is probably some fancy bit twiddling thing I could do here.
			for (; bytesRead < limiter; bytesRead++) {
				if (bytesRead == bytesReceived) {																	// NOTE: If out of buffer data, get more, but at a specific offset into the buffer, so that copying buffer into the VectorString later is still completely und utterly aligned.
					limiter -= bytesRead;
					ssize_t eatenLimiterBytes = 8 - (limiter + 1);													// Get difference in bytes between current position and the next-lowest 8 byte boundary.
					if (!refillBuffer(eatenLimiterBytes)) { return false; }											// Refill buffer starting at that difference.
					bytesRead = eatenLimiterBytes;																	// Start the read head at the difference as well. This makes it seem like all this effort is useless, but the whole point of this is the alignment of the memory, which is subtle.
				}
				if (buffer[bytesRead] == '\n') { bytesRead++; return true; }
				if (!(line += buffer[bytesRead])) { return false; }
			}
			if (bytesRead == bytesReceived) {																		// NOTE: If we run out of data here, we can use precomputed values for the above data fetch algorithm, seeing as we're always one off from the next byte at this point.
				if (!refillBuffer(7)) { return false; }
				bytesRead = 8;
				if (buffer[7] == '\n') { isReadPositionUnaligned = false; return true; }
				if (!(line += buffer[7])) { return false; }
				isReadPositionUnaligned = false;
			}
			else {
				if (buffer[bytesRead] == '\n') { bytesRead++; isReadPositionUnaligned = false; return true; }
				if (!(line += buffer[bytesRead])) { return false; }
				bytesRead++;
				isReadPositionUnaligned = false;
			}
		}

		// TODO: This whole copying code could theoretically be avoided if we just always store the lines in the InputStream and just store indices to those lines in the HistoryBuffer.
		// You would have to copy chunks of data around when resizing the InputStream, since we don't want those to get lost while doing that, but since the InputStream doesn't resize often in the grand scheme of things,
		// it should actually be more efficient, with less overhead. It would be a major redesign and I don't want to do that right now, I'm leaving it as a future improvement.

		while (true) {																								// NOTE: I could check for shouldLoopRun here, but it would only make sense for lines that are incredibly super duper mega long, which none really are. Doing a comparison here would probably bring more harm than good IMO.
			for (; bytesRead < bytesReceived - 7; bytesRead += 8) {													// NOTE: The - 7 is to make sure that, whatever happens, the bytesRead head (which is always snapped to the alignment bounds), doesn't cause unused memory to be read.

				// NOTE: The above explanation of the - 7 leaves a little to be questioned. The main thing is: One would expect to use - 8, why - 7? - 8 would work almost as well, achieving exactly the same result in all cases but one:
				// NOTE: That case is when bytesReceived ends on a completed byte and there is nothing wrong with reading the whole buffer from this loop. With - 8, that would cause the whole last byte not to be read, while - 7 would allow the byte to be read in that case.
				// NOTE: Because of this one case, - 7 is superior to - 8 in this situation.

				// NOTE: This whole - 7 might also seem pointless because the buffer size is always a multiple of 8 anyway, making it impossible for unalignment issues to pop up. That isn't true though, since the buffer refill doesn't ever have to reach the end of the buffer.
				// NOTE: When the user is creating the stdin input on the fly by typing things in, most of the time, the buffer refill won't be able to generate any sort of aligned buffer data, because the input data length is up to the user, making the - 7 here imperative.

				if (buffer[bytesRead + 0] == '\n') { bytesRead++; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 1] == '\n') { if (!(line += buffer[bytesRead])) { return false; } bytesRead += 2; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 2] == '\n') { if (!(line += *(uint16_t*)(buffer + bytesRead))) { return false; } bytesRead += 3; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 3] == '\n') { if (!line.write3(buffer + bytesRead)) { return false; } bytesRead += 4; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 4] == '\n') { if (!(line += *(uint32_t*)(buffer + bytesRead))) { return false; } bytesRead += 5; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 5] == '\n') { if (!line.write5(buffer + bytesRead)) { return false; } bytesRead += 6; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 6] == '\n') { if (!line.write6(buffer + bytesRead)) { return false; } bytesRead += 7; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 7] == '\n') { if (!line.write7(buffer + bytesRead)) { return false; } bytesRead += 8; return true; }
				if (!(line += *(uint64_t*)(buffer + bytesRead))) { return false; }
			}
			for (; bytesRead < bytesReceived; bytesRead++) {														// NOTE: The above protection against reading unused memory causes there to be unread bytes in the buffer. There are always less than 8, which means we don't have to consider the case of \n being the last one like above.
				if (buffer[bytesRead] == '\n') { bytesRead++; isReadPositionUnaligned = true; return true; }
				if (!(line += buffer[bytesRead])) { return false; }
			}
			if (refillBuffer()) { continue; }																		// If we never encounter the end of the line in the current buffer, fetch more data.
			return false;																							// If something went wrong while refilling buffer, return false.
		}
	}

	// NOTE: If this function returns false, no garuantees are made about the validity and reusability of the class instance. Don't rely on either of those things.
	static bool discardLine() {															// Reads the next line but doesn't store it anywhere since we're only reading it to advance the read position. Returns true on success. Returns false on EOF or error in Windows. Returns false on EOF, SIGINT, SIGTERM or error on Linux.
		if (isReadPositionUnaligned) {
			ssize_t limiter = bytesRead + 8 - (bytesRead % 8) - 1;													// TODO: Research if there is a more efficient way to do this. There is probably some fancy bit twiddling thing I could do here.
			for (; bytesRead < limiter; bytesRead++) {
				if (bytesRead == bytesReceived) {																	// NOTE: If there is no more data in the buffer, refill it. This inherently aligns the buffer, and since we don't care about any alignment with any VectorString, we can make use of that fact and directly goto transfer loop.
					if (!refillBuffer()) { return false; }
					goto MainTransferLoop;
				}
				if (buffer[bytesRead] == '\n') { bytesRead++; return true; }
			}
			if (bytesRead == bytesReceived) {																		// NOTE: Contrary to in readLine, this block does the same thing as it's corresponding version inside of the loop.
				if (!refillBuffer()) { return false; }
				goto MainTransferLoop;
			}
			isReadPositionUnaligned = false;																		// No matter what happens, even if the last character we've got is a newline, the read position will be aligned again after we get out of this if statement.
			if (buffer[bytesRead] == '\n') { bytesRead++; return true; }
			bytesRead++;
		}

	MainTransferLoop:
		while (true) {
			for (; bytesRead < bytesReceived - 7; bytesRead += 8) {
				if (buffer[bytesRead + 0] == '\n') { bytesRead++; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 1] == '\n') { bytesRead += 2; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 2] == '\n') { bytesRead += 3; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 3] == '\n') { bytesRead += 4; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 4] == '\n') { bytesRead += 5; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 5] == '\n') { bytesRead += 6; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 6] == '\n') { bytesRead += 7; isReadPositionUnaligned = true; return true; }
				if (buffer[bytesRead + 7] == '\n') { bytesRead += 8; return true; }
			}
			for (; bytesRead < bytesReceived; bytesRead++) { if (buffer[bytesRead] == '\n') { bytesRead++; isReadPositionUnaligned = true; return true; } }			// NOTE: Read the left over bytes, same as in readLine().
			if (refillBuffer()) { continue; }
			return false;
		}
	}

	static void release() { delete[] buffer; }
};

#ifndef PLATFORM_WINDOWS																							// Signalling and polling related static member variables only need to be initialized in Linux because we don't use them in Windows.
pollfd InputStream::fds[] = { STDIN_FILENO, POLLIN, 0, 0, POLLIN, 0 };												// Parts of this data get changed later in runtime.
sigset_t InputStream::sigmask;
#endif

char* InputStream::buffer;
ssize_t InputStream::bufferSize = INPUT_STREAM_BUFFER_START_SIZE;

ssize_t InputStream::bytesRead = 0;
ssize_t InputStream::bytesReceived = 0;

bool InputStream::isReadPositionUnaligned = false;

std::cmatch matchData;

#define CURRENT_LINE_ALIAS HistoryBuffer::buffer[HistoryBuffer::index]
#define REGEX_SEARCH std::regex_search((const char*)CURRENT_LINE_ALIAS.buffer, (const char*)(CURRENT_LINE_ALIAS.buffer + CURRENT_LINE_ALIAS.bufferWriteHead), matchData, keyphraseRegex)

ptrdiff_t matchSuffixIndex;

void highlightMatches() {																							// I assume this will be inlined. Probably not in debug mode, but almost definitely in release mode.
	matchSuffixIndex = 0;				// NOTE: Technically, unrolling the first iteration of the below loop would allow us to skip the overhead of initializing this variable to 0 and the overhead of the following additions, but the compiler probably does it for us.
	const char* stringBufferEnd = CURRENT_LINE_ALIAS.buffer + CURRENT_LINE_ALIAS.bufferWriteHead;
	do {
		ptrdiff_t newMatchPosition = matchData.position();
		std::cout.write(CURRENT_LINE_ALIAS.buffer + matchSuffixIndex, newMatchPosition);
		std::cout << color::red;
		matchSuffixIndex += newMatchPosition;
		ptrdiff_t matchLength = matchData.length();
		std::cout.write(CURRENT_LINE_ALIAS.buffer + matchSuffixIndex, matchLength);
		std::cout << color::reset;
		matchSuffixIndex += matchLength;
		// The following assumes that I've got a copy assignment operator set up, which I don't anymore.
		// CURRENT_LINE_ALIAS = std::move((const std::string)matchData.suffix().str());			// SUPER-IMPORTANT_TODO: For some reason, this line compiles, even though intellisense is adamant that it's an issue.
		//CURRENT_LINE_ALIAS = (const std::string&&)std::move((const std::string)matchData.suffix().str());				// This line doesn't compile though, even though it is the same thing.
					// SUPER-IMPORTANT-TODO: The above issue seems to be a bug in the compiler, which you should definitely tell someone through some feedback thing. Best option would be to post the issue on Stackoverflow and see if it's actually a bug or not.
	} while (std::regex_search((const char*)(CURRENT_LINE_ALIAS.buffer + matchSuffixIndex), (const char*)stringBufferEnd, matchData, keyphraseRegex));
}

// A couple of #defines to help reduce code bloat in the coming sections of the program.

#ifdef PLATFORM_WINDOWS
#define MAIN_WHILE InputStream::init(); while (shouldLoopRun)
#else
#define MAIN_WHILE InputStream::init(); while (true)
#endif

// NOTE: As per the standard, you can use function-style macros while leaving one or more or all of the parameters blank. They just won't be filled with anything and you'll have empty spots, which is exactly the behaviour we want here, so everythings fine.

#define LINE_WHILE_START MAIN_WHILE { if (!InputStream::readLine(CURRENT_LINE_ALIAS)) { break; }
#define LINE_WHILE_END(cleanupCode) CURRENT_LINE_ALIAS.clear(); } cleanupCode; InputStream::release(); HistoryBuffer::release(); return 0;

#define COLORED_RED_ONLY_LINE_WHILE_START std::cout << color::red; LINE_WHILE_START
#define COLORED_RED_ONLY_LINE_WHILE_END() LINE_WHILE_END(std::cout << color::reset)

#define LINE_WHILE_CONTINUE CURRENT_LINE_ALIAS.clear(); continue;

#ifdef PLATFORM_WINDOWS
#define INNER_WINDOWS_SIGNAL_CHECK_START if (shouldLoopRun) {
#define INNER_WINDOWS_SIGNAL_CHECK_END } else { InputStream::release(); HistoryBuffer::release(); return 0; }														// NOTE: It might seem better to break out of main loop here, but the things we are doing here instead are more efficient. It isn't a bug.
#else
#define INNER_WINDOWS_SIGNAL_CHECK_START
#define INNER_WINDOWS_SIGNAL_CHECK_END
#endif

#define INNER_INPUT_STREAM_READ_LINE if (!InputStream::readLine(CURRENT_LINE_ALIAS)) { InputStream::release(); HistoryBuffer::release(); return 0; }				// NOTE: This code also doesn't break out of the main loop on purpose, even though that would work. This method is more efficient.
#define INNER_INPUT_STREAM_DISCARD_LINE if (!InputStream::discardLine()) { InputStream::release(); HistoryBuffer::release(); return 0; }

#define WRITE_LINE_SUFFIX std::cout.write(CURRENT_LINE_ALIAS.buffer + matchSuffixIndex, CURRENT_LINE_ALIAS.bufferWriteHead - matchSuffixIndex)

// NOTE: I don't handle potential errors for some of the small allocations because your system would need to be super super low on resources for the errors to get thrown, which shouldn't happen normally.
// If your system is super super low on resources and you run this program, it's kind of your own fault is it not?
// TODO: Go through the program and make sure that this way of thinking won't bite you in your backside. Why not handle the errors, it's not going to hurt anyone? Really consider doing it.

// TODO: Learn about SFINAE and std::enable_if.

// Program entry point
int main(int argc, char** argv) {
	// Unsynchronize C++ output buffer with C output buffer. Normally set to true to avoid output getting mixed around when mixing C++ and C function calls in source code. Setting to false yields performance improvements because often-times, implementors of the stdlib don't bother implementing buffering until this is set to false
	// (because it's easier to implement for them like that), or so I've heard at least, there might be other reasons for this as well.
	// IMPORTANT: As a result of doing this unsync stuff, we have to choose either C or C++ style buffered output and stick with it for the whole program. It is still possible to use C-style input and C++-style output at same time, the limitation only applies within the boundaries of input and output respectively.
	std::cout.sync_with_stdio(false);

#ifdef PLATFORM_WINDOWS
	signal(SIGINT, signalHandler);			// Handling error here doesn't do any good because program should continue to operate regardless.
	signal(SIGTERM, signalHandler);			// Reacting to errors here might poison stdout for programs on other ends of pipes, so just leave this be.
#endif

	// Only enable output colors if stdout is a TTY to make reading piped output easier for other programs.
	if (isatty(STDOUT_FILENO)) {
#ifdef PLATFORM_WINDOWS						// On windows, you have to set up virtual terminal processing explicitly and it for some reason disables piping. That's why this group is here.
		HANDLE consoleOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { goto ANSIOutputSetupFailure; }		// If ANSI setup fails, just revert back to not coloring the output, so the user can at least see the output, even if it won't be colored.
		DWORD mode;																					// IMPORTANT-NOTE: Because of the way this code plays with the rest of the code, the rest of the program will think that the output is being piped to something instead of being attached to a console, but that doesn't matter in this case.
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { goto ANSIOutputSetupFailure; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { goto ANSIOutputSetupFailure; }
#endif

		// NOTE: Technically, it would be more efficient to place the above virtual terminal processing code in places where we are sure that the possibility exists that we can use colors. Not all places satisfy this requirement and the above code thereby technically runs to early and is slightly inefficient in that regard.
		// NOTE: That would require writing it multiple times though and the code wouldn't look as nice.
		// NOTE: Since this overhead is so incredibly small and only transpires one single time, there is essentially no cost, which is why I'm ok with not moving it. SIDE-NOTE: Yes, we could use a function for this, but that still produces less pretty code than the current situation.

		isOutputColored = true;
		forcedOutputColoring = true;
	}
	else {
	ANSIOutputSetupFailure:
		isOutputColored = false;
		forcedOutputColoring = false;
	}

	// Only enable error colors if stderr is a TTY to make reading piped output easier for other programs.
	if (isatty(STDERR_FILENO)) {
#ifdef PLATFORM_WINDOWS
		HANDLE consoleOutputHandle = GetStdHandle(STD_ERROR_HANDLE);
		if (!consoleOutputHandle || consoleOutputHandle == INVALID_HANDLE_VALUE) { goto ANSIErrorSetupFailure; }			// IMPORTANT-NOTE: The same fallback caviat exists here as in the above stdout ANSI processing.
		DWORD mode;
		if (!GetConsoleMode(consoleOutputHandle, &mode)) { goto ANSIErrorSetupFailure; }
		if (!SetConsoleMode(consoleOutputHandle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { goto ANSIErrorSetupFailure; }
#endif

		// NOTE: Same efficiency comment as above applies here as well.

		isErrorColored = true;
		forcedErrorColoring = true;
	}
	else {
	ANSIErrorSetupFailure:
		isErrorColored = false;
		forcedErrorColoring = false;
	}

	// NOTE: The reason we set the forcedOutputColoring as well as the isOutputColored flag above is because we set isOutputColored to forcedOutputColoring later in the code and we don't want that operation to mess up our coloring code.
	// NOTE: Obviously, the same thing applies to forcedErrorColoring.

	manageArgs(argc, argv);

	HistoryBuffer::init();			// Regardless of whether or not we actually plan to use HistoryBuffer and whether or not it's count is 0, there will always be one slot in it's array that we can use to cache the current line. This method is memory and processing power efficient because we don't have to copy or swap for history pushes.

	// NOTE: As much of the branching is done outside of the loops as possible so as to avoid checking data over and over even though it'll never change it's value.
	// The compiler is really good at doing this and technically would do it for me, but this way it's a safe bet and I can rest easy. Also, the main reason is that this way is more organized.
	// Trying to branch inside the loops and to condense everything down to one single loop with a bunch of conditionals inside gets ugly really fast. I prefer this layout more.

	if (isOutputColored) {																			// If output is colored, activate colors before going into each loop and do the more complex matching algorithm
		if (flags::allLines) {
			if (flags::inverted) { return 0; }
			if (flags::only_line_nums) {
				LINE_WHILE_START
					if (REGEX_SEARCH) { std::cout << color::red << lineCounter << color::reset << '\n'; lineCounter++; LINE_WHILE_CONTINUE; }
					std::cout << lineCounter << '\n'; lineCounter++;
				LINE_WHILE_END()
			}
			if (flags::lineNums) {
				// NOTE: One would think that an improvement would be to snap all line numbers to the same column so as to avoid the text shifting to the right when line numbers go from 9 to 10 or from 99 to 100, but thats easier said than done, because we don't actually know how much room to leave in the column before reading the whole file.
				LINE_WHILE_START				// NOTE: Since we should be prepared to read incredibly long files, I'm going to leave the snapping out, since we can't really implement it without giving ourselves a maximum amount of line numbers we can traverse.
					if (REGEX_SEARCH) { std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); WRITE_LINE_SUFFIX << '\n'; lineCounter++; LINE_WHILE_CONTINUE; }
					(std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; lineCounter++;
				LINE_WHILE_END()
			}
			LINE_WHILE_START
				if (REGEX_SEARCH) { highlightMatches(); WRITE_LINE_SUFFIX << '\n'; }
				else { std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; }
			LINE_WHILE_END()
		}

		if (flags::context) {
			if (flags::only_line_nums) {
				if (flags::inverted) {
					LINE_WHILE_START
						if (REGEX_SEARCH) {
							HistoryBuffer::purgeAmountFilled();
							lineCounter++;
							for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
								INNER_WINDOWS_SIGNAL_CHECK_START
									CURRENT_LINE_ALIAS.clear();
									INNER_INPUT_STREAM_READ_LINE
									if (REGEX_SEARCH) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
									lineCounter++;
								INNER_WINDOWS_SIGNAL_CHECK_END
							}
							LINE_WHILE_CONTINUE;
						}
						size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { std::cout << safestLineNum << '\n'; }
						HistoryBuffer::incrementAmountFilled();
						lineCounter++;
					LINE_WHILE_END(HistoryBuffer::lastPrintLineNums())
				}
				LINE_WHILE_START
					if (REGEX_SEARCH) {
						HistoryBuffer::printLineNums();
						std::cout << color::red << lineCounter << color::reset << '\n';
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {			// SIDE-NOTE: Technically, for a lot of my use-cases, a do-while loop would be more efficient than a for loop since the initial check is garanteed to return true.
							INNER_WINDOWS_SIGNAL_CHECK_START																									// SIDE-NOTE: Even so, I like the way for loops look because you see the information for the loop at the top and not all the way at the bottom, plus, compiler optimizes.
								CURRENT_LINE_ALIAS.clear();
								INNER_INPUT_STREAM_READ_LINE
								if (REGEX_SEARCH) {
									std::cout << color::red << lineCounter << color::reset << '\n';
									lineCounter++;
									afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex;
									continue;
								}
								std::cout << lineCounter << '\n';
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END
						}
						LINE_WHILE_CONTINUE;
					}
					HistoryBuffer::incrementAmountFilled();
					lineCounter++;
				LINE_WHILE_END()
			}
			if (flags::lineNums) {
				if (flags::inverted) {
					LINE_WHILE_START
						if (REGEX_SEARCH) {
							HistoryBuffer::purgeWithAmountSet();
							lineCounter++;
							for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
								INNER_WINDOWS_SIGNAL_CHECK_START
									CURRENT_LINE_ALIAS.clear();
									INNER_INPUT_STREAM_READ_LINE
									if (REGEX_SEARCH) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
									lineCounter++;
								INNER_WINDOWS_SIGNAL_CHECK_END
							}
							LINE_WHILE_CONTINUE;
						}
						VectorString* safestLine;
						if (HistoryBuffer::peekSafestLine(safestLine)) {
							size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { (std::cout << safestLineNum << ' ').write(safestLine->buffer, safestLine->bufferWriteHead) << '\n'; }
						}
						HistoryBuffer::pushWithAmountInc();
						lineCounter++;
					LINE_WHILE_END(HistoryBuffer::printLinesWithLineNums())
				}
				LINE_WHILE_START
					if (REGEX_SEARCH) {
						HistoryBuffer::printLinesWithLineNums();
						std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); WRITE_LINE_SUFFIX << '\n';
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								CURRENT_LINE_ALIAS.clear();
								INNER_INPUT_STREAM_READ_LINE
								if (REGEX_SEARCH) {
									std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); WRITE_LINE_SUFFIX << '\n';
									lineCounter++;
									afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex;
									continue;
								}
								(std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END
						}
						LINE_WHILE_CONTINUE;
					}
					HistoryBuffer::push();
					lineCounter++;
				LINE_WHILE_END()
			}
			if (flags::inverted) {
				LINE_WHILE_START
					if (REGEX_SEARCH) {
						HistoryBuffer::purgeWithAmountSet();
						for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								CURRENT_LINE_ALIAS.clear();
								INNER_INPUT_STREAM_READ_LINE
								if (REGEX_SEARCH) { padding = HistoryBuffer::buffer_lastIndex; continue; }
								padding--;
							INNER_WINDOWS_SIGNAL_CHECK_END
						}
						LINE_WHILE_CONTINUE;
					}
					VectorString* safestLine; if (HistoryBuffer::peekSafestLine(safestLine)) { std::cout.write(safestLine->buffer, safestLine->bufferWriteHead) << '\n'; }
					HistoryBuffer::pushWithAmountInc();
				LINE_WHILE_END(HistoryBuffer::print())
			}
			LINE_WHILE_START
				if (REGEX_SEARCH) {
					HistoryBuffer::print();
					highlightMatches(); WRITE_LINE_SUFFIX << '\n';
					for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							CURRENT_LINE_ALIAS.clear();
							INNER_INPUT_STREAM_READ_LINE
							if (REGEX_SEARCH) {
								highlightMatches(); WRITE_LINE_SUFFIX << '\n';
								padding = HistoryBuffer::buffer_lastIndex;
								continue;
							}
							std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
							padding--;
						INNER_WINDOWS_SIGNAL_CHECK_END
					}
					LINE_WHILE_CONTINUE;
				}
				HistoryBuffer::push();
			LINE_WHILE_END()
		}

		// SIDE-NOTE: Storing these command-line flags in a bit field and switching on it's value might have been a better way to do this in theory, because it's more efficient and might look better, but in practice, it might not have been the way to go.
		// It's not very scalable because you have to write out every single case (or use default, but that isn't applicable when you have complex relationships between cases and such). As soon as you have anywhere near something like 16 flags, your code bloat starts to become insane.
		// So I think the if statement approach is a good one, even though it's technically a very small bit less performant than the switch case approach.

		if (flags::inverted) {
			if (flags::only_line_nums) { LINE_WHILE_START if (REGEX_SEARCH) { lineCounter++; LINE_WHILE_CONTINUE; } std::cout << lineCounter << '\n'; lineCounter++; LINE_WHILE_END() }
			if (flags::lineNums) { LINE_WHILE_START if (REGEX_SEARCH) { lineCounter++; LINE_WHILE_CONTINUE; } (std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; lineCounter++; LINE_WHILE_END() }
			LINE_WHILE_START if (REGEX_SEARCH) { LINE_WHILE_CONTINUE } std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; LINE_WHILE_END()				// TODO: Should we use regex_match here instead of regex_search? Is one better than the other or does it not matter?
		}
		if (flags::only_line_nums) {
			if (forcedOutputColoring) { LINE_WHILE_START if (REGEX_SEARCH) { std::cout << color::red << lineCounter << color::reset << '\n'; } lineCounter++; LINE_WHILE_END() }
			COLORED_RED_ONLY_LINE_WHILE_START if (REGEX_SEARCH) { std::cout << lineCounter << '\n'; } lineCounter++; COLORED_RED_ONLY_LINE_WHILE_END()
		}
		if (flags::lineNums) { LINE_WHILE_START if (REGEX_SEARCH) { std::cout << color::red << lineCounter << color::reset << ' '; highlightMatches(); WRITE_LINE_SUFFIX << '\n'; } lineCounter++; LINE_WHILE_END() }
		LINE_WHILE_START if (REGEX_SEARCH) { highlightMatches(); WRITE_LINE_SUFFIX << '\n'; } LINE_WHILE_END()
	}

	if (flags::allLines) {
		if (flags::inverted) { return 0; }
		if (flags::only_line_nums) { MAIN_WHILE { INNER_INPUT_STREAM_DISCARD_LINE std::cout << lineCounter << '\n'; lineCounter++; } }					// NOTE: At first glance, it looks like InputStream and HistoryBuffer aren't being released here, but they are. This code is completely fine.
		if (flags::lineNums) { LINE_WHILE_START (std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; lineCounter++; LINE_WHILE_END() }
		LINE_WHILE_START std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; LINE_WHILE_END()
	}

	if (flags::context) {
		if (flags::only_line_nums) {
			if (flags::inverted) {
				LINE_WHILE_START
					if (REGEX_SEARCH) {
						HistoryBuffer::purgeAmountFilled();
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								CURRENT_LINE_ALIAS.clear();
								INNER_INPUT_STREAM_READ_LINE
								if (REGEX_SEARCH) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END
						}
						LINE_WHILE_CONTINUE;
					}
					size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { std::cout << safestLineNum << '\n'; }
					HistoryBuffer::incrementAmountFilled();
					lineCounter++;
				LINE_WHILE_END(HistoryBuffer::lastPrintLineNums())
			}
			LINE_WHILE_START
				if (REGEX_SEARCH) {
					HistoryBuffer::printLineNums();
					std::cout << lineCounter << '\n';
					lineCounter++;
					for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							CURRENT_LINE_ALIAS.clear();
							INNER_INPUT_STREAM_READ_LINE
							std::cout << lineCounter << '\n';
							lineCounter++;
							if (REGEX_SEARCH) { afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; }
						INNER_WINDOWS_SIGNAL_CHECK_END
					}
					LINE_WHILE_CONTINUE;
				}
				HistoryBuffer::incrementAmountFilled();
				lineCounter++;
			LINE_WHILE_END()
		}
		if (flags::lineNums) {
			if (flags::inverted) {
				LINE_WHILE_START
					if (REGEX_SEARCH) {
						HistoryBuffer::purgeWithAmountSet();
						lineCounter++;
						for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
							INNER_WINDOWS_SIGNAL_CHECK_START
								CURRENT_LINE_ALIAS.clear();
								INNER_INPUT_STREAM_READ_LINE
								if (REGEX_SEARCH) { lineCounter++; afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; continue; }
								lineCounter++;
							INNER_WINDOWS_SIGNAL_CHECK_END
						}
						LINE_WHILE_CONTINUE;
					}
					VectorString* safestLine;
					if (HistoryBuffer::peekSafestLine(safestLine)) {
						size_t safestLineNum; if (HistoryBuffer::peekSafestLineNum(safestLineNum)) { (std::cout << safestLineNum << ' ').write(safestLine->buffer, safestLine->bufferWriteHead) << '\n'; }
					}
					HistoryBuffer::pushWithAmountInc();
					lineCounter++;
				LINE_WHILE_END(HistoryBuffer::printLinesWithLineNums())
			}
			LINE_WHILE_START
				if (REGEX_SEARCH) {
					HistoryBuffer::printLinesWithLineNums();
					(std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
					lineCounter++;
					for (size_t afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; lineCounter < afterLastLineOfPadding; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							CURRENT_LINE_ALIAS.clear();
							INNER_INPUT_STREAM_READ_LINE
							(std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
							lineCounter++;
							if (REGEX_SEARCH) { afterLastLineOfPadding = lineCounter + HistoryBuffer::buffer_lastIndex; }
						INNER_WINDOWS_SIGNAL_CHECK_END
					}
					LINE_WHILE_CONTINUE;
				}
				HistoryBuffer::push();
				lineCounter++;
			LINE_WHILE_END()
		}
		if (flags::inverted) {
			LINE_WHILE_START
				if (REGEX_SEARCH) {
					HistoryBuffer::purgeWithAmountSet();
					for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
						INNER_WINDOWS_SIGNAL_CHECK_START
							CURRENT_LINE_ALIAS.clear();
							INNER_INPUT_STREAM_READ_LINE
							if (REGEX_SEARCH) { padding = HistoryBuffer::buffer_lastIndex; continue; }
							padding--;
						INNER_WINDOWS_SIGNAL_CHECK_END
					}
					LINE_WHILE_CONTINUE;
				}
				VectorString* safestLine; if (HistoryBuffer::peekSafestLine(safestLine)) { std::cout.write(safestLine->buffer, safestLine->bufferWriteHead) << '\n'; }
				HistoryBuffer::pushWithAmountInc();
			LINE_WHILE_END(HistoryBuffer::print())
		}
		LINE_WHILE_START
			if (REGEX_SEARCH) {
				HistoryBuffer::print();
				std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
				for (unsigned int padding = HistoryBuffer::buffer_lastIndex; padding > 0; ) {
					INNER_WINDOWS_SIGNAL_CHECK_START
						CURRENT_LINE_ALIAS.clear();
						INNER_INPUT_STREAM_READ_LINE
						if (REGEX_SEARCH) {
							std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
							padding = HistoryBuffer::buffer_lastIndex;
							continue;
						}
						std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n';
						padding--;
					INNER_WINDOWS_SIGNAL_CHECK_END
				}
				LINE_WHILE_CONTINUE;
			}
			HistoryBuffer::push();
		LINE_WHILE_END()
	}

	if (flags::inverted) {
		if (flags::only_line_nums) { LINE_WHILE_START if (REGEX_SEARCH) { lineCounter++; LINE_WHILE_CONTINUE; } std::cout << lineCounter << '\n'; lineCounter++; LINE_WHILE_END() }
		if (flags::lineNums) { LINE_WHILE_START if (REGEX_SEARCH) { lineCounter++; LINE_WHILE_CONTINUE; } (std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; lineCounter++; LINE_WHILE_END() }
		LINE_WHILE_START if (REGEX_SEARCH) { LINE_WHILE_CONTINUE; } std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; LINE_WHILE_END()
	}
	if (flags::only_line_nums) { LINE_WHILE_START if (REGEX_SEARCH) { std::cout << lineCounter << '\n'; } lineCounter++; LINE_WHILE_END() }
	if (flags::lineNums) { LINE_WHILE_START if (REGEX_SEARCH) { (std::cout << lineCounter << ' ').write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; } lineCounter++; LINE_WHILE_END() }
	LINE_WHILE_START if (REGEX_SEARCH) { std::cout.write(CURRENT_LINE_ALIAS.buffer, CURRENT_LINE_ALIAS.bufferWriteHead) << '\n'; } LINE_WHILE_END()
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

/*
 * weasel-pageant Win32-side main code.
 *
 * Copyright 2017  Valtteri Vuorikoski
 *
 * This file is part of weasel-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 */

#include "targetver.h"

#include <stdint.h>
#include <stdio.h>

#include <Windows.h>

#include "winpgntc.h"
#include "../linux/common.h"

static uint32_t flags = 0;


// Send a (narrow) string to standard error, which is expected to be connected to stderr
// on the linux side.
void print_error(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "win32 helper: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}


void print_debug(const char *fmt, ...)
{
	if (!(flags & WSLP_CHILD_FLAG_DEBUG))
		return;

	va_list ap;

	fprintf(stderr, "win32 helper DEBUG: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}


static DWORD read_packet(const HANDLE input, uint8_t *buf)
{
	DWORD cnt, rem = 4, got_length = 0;
	DWORD error_code = ERROR_SUCCESS;
	uint8_t *bufp = buf;

	// Documentation indicates that ReadFile() returns when a "write operation completes
	// on the write end of the pipe". Since the linux side may call write() multiple times,
	// loop until everything has been read.
	while (rem > 0) {
		print_debug("start read with rem=%d", rem);
		if (!ReadFile(input, bufp, rem, &cnt, NULL)) {
			error_code = GetLastError();

			if (error_code != ERROR_BROKEN_PIPE)  // EOF
				print_error("reading packet data failed with code %d", error_code);
			else
				print_debug("Broken pipe on input");

			return 0;
		}

		if (cnt == 0) {
			print_debug("EOF on input");
			return 0;
		}

		bufp += cnt;
		rem -= cnt;

		if (rem > 0)
			continue;  // more to read

		if (!got_length) {
			rem = msglen(buf) - 4;
			if (rem > (AGENT_MAX_MSGLEN - 4)) {
				print_error("got packet with length %d exceeding maximum", rem);
				return 0;
			}
			print_debug("input packet length is 4+%d", rem);
			got_length = 1;
		}
	}

	return 1;
}


static DWORD write_packet(const HANDLE output, uint8_t *buf)
{
	DWORD rem = msglen(buf), cnt;
	DWORD error_code;

	// Assume that WriteFile will write everything given to it
	if (!WriteFile(output, buf, rem, &cnt, NULL)) {
		error_code = GetLastError();

		if (error_code != ERROR_BROKEN_PIPE)  // linux side is gone
			print_error("writing %d bytes of packet data failed with code %d", rem, error_code);
		return 0;
	}

	if (cnt != rem) {
		print_error("short write: wanted to write %d byte packet, wrote %d bytes", rem, cnt);
		return 0;
	}

	return 1;
}


static void main_loop(const HANDLE output, const HANDLE input)
{
	uint8_t buf[AGENT_MAX_MSGLEN];

	print_debug("main loop starting");

	while (1) {
		// Get packet from linux side.
		if (!read_packet(input, buf))
			return;

		print_debug("got packet, querying pageant");

		// We should have a valid packet in buf. Send it to the agent and
		// buf will be filled with the response.
		agent_query(buf);

		// Return response to linux side.
		if (!write_packet(output, buf))
			return;
	}
}


int main(const int argc, const char **argv)
{
	LPCWSTR test_error = L"This program is part of weasel-pageant and cannot be executed directly.";

	const HANDLE out_handle = GetStdHandle(STD_OUTPUT_HANDLE);  // expected to be a pipe where the linux side is listening
	const HANDLE in_handle = GetStdHandle(STD_INPUT_HANDLE);  // ...and where the linux side is sending

	if (out_handle == INVALID_HANDLE_VALUE || in_handle == INVALID_HANDLE_VALUE) {
		print_error("GetStdHandle failed (code %d)", GetLastError());
		return 1;
	}

	// Flags bitmask from parent.
	if (argc > 1) {
		flags = strtoul(argv[1], NULL, 16);
		print_debug("flags: %08x", flags);
	}

	// If this succeeds, STD_OUTPUT_HANDLE is attached to a console, meaning that this program
	// is being called manually. 
	if (WriteConsoleW(out_handle, test_error, (DWORD) wcslen(test_error), NULL, NULL))
		return 1;

	// Write an initialization byte to inform the parent that we are alive.
	char initbyte = 'a';
	if (!WriteFile(out_handle, &initbyte, 1, NULL, NULL)) {
		print_error("failed to write init byte to output (error %d); exiting", GetLastError());
		return 1;
	}

	main_loop(out_handle, in_handle);

    return 0;
}


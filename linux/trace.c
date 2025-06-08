#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "FreeRTOS.h"


static struct timespec start_time = {0, 0};
static int64_t last_time = 0;
static UBaseType_t last_task_number = 0xffffffff;
static FILE *header_file = NULL;
static FILE *trace_file = NULL;
static uint64_t task_numbers_initialized = 0;
static bool initialized = false;


static void initialize() {
	if (start_time.tv_sec == 0 && start_time.tv_nsec == 0) {
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		char *trace_dir = getenv("TRACE_DIR");
		if (trace_dir != NULL) {
			{
				char path[strlen(trace_dir) + sizeof("/header")];
				bzero(path, sizeof(path));
				strcpy(path, trace_dir);
				strcpy(path + strlen(trace_dir), "/header");
				header_file = fopen(path, "w");
			}
			{
				char path[strlen(trace_dir) + sizeof("/trace")];
				bzero(path, sizeof(path));
				strcpy(path, trace_dir);
				strcpy(path + strlen(trace_dir), "/trace");
				trace_file = fopen(path, "w");
			}
		}
	}
	initialized = true;
}


static void write_trace_header(UBaseType_t task_number, const char *task_name) {

	// Write:
	// I L NNNNNNNNN
	// | | |
	// | | `- task name
	// | `- length of task name (1 byte)
	// `- task id (1 byte)

	size_t length = strlen(task_name);
	fputc(task_number, header_file);
	fputc(length, header_file);
	for (size_t i = 0; i < length; ++i) {
		fputc(task_name[i], header_file);
	}
	fflush(header_file);
}


/* Write data to trace file */
static void write_trace(int64_t time_diff, void *data, size_t data_size, FILE *fp) {

	// Write:
	// L DDDDDD
	// | |
	// | `- data
	// `- length
	//
	// Where length is 8, 16, 32 or 64 bit number
	//
	// First two bits indicating length of number:
	// 00XXXXXX - 8-bit format
	// 01XXXXXX XXXXXXXX - 16-bit format
	// 10XXXXXX XXXXXXXX XX... 32-bit format
	// 11XXXXXX XXXXXXXX XX... 64-bit format
	//
	// Byte order is big endian (network byte order)

	if (time_diff < (((int64_t)1) << 6)) {
		fputc(time_diff, fp);
	}
	else if (time_diff < (((int64_t)1) << 14)) {
		fputc((time_diff >> 8 & 0x3f) | 0x40, fp);
		fputc(time_diff & 0xff, fp);
	}
	else if (time_diff < (((int64_t)1) << 30)) {
		fputc((time_diff >> 24 & 0x3f) | 0x80, fp);
		fputc(time_diff >> 16 & 0xff, fp);
		fputc(time_diff >> 8 & 0xff, fp);
		fputc(time_diff & 0xff, fp);
	}
	else {
		fputc((time_diff >> 56 & 0x3f) | 0xc0, fp);
		for (size_t i = 7; i > 0; --i) {
			fputc(time_diff >> ((i - 1) << 3) & 0xff, fp);
		}
	}
	fwrite(data, data_size, 1, fp);
	fflush(fp);
}


void trcKERNEL_HOOKS_TASK_CREATE(UBaseType_t task_number, const char *task_name) {
	if (!initialized) {
		initialize();
	}

	if (header_file == NULL) {
		return;
	}

	write_trace_header(task_number, task_name);
}


void trcKERNEL_HOOKS_TASK_SWITCH(UBaseType_t task_number) {
	if (!initialized) {
		initialize();
	}

	if (trace_file == NULL) {
		return;
	}

	// Group repeated tasks together
	if (task_number == last_task_number) {
		return;
	}
	last_task_number = task_number;


	struct timespec end_time;
	clock_gettime(CLOCK_MONOTONIC, &end_time);

	// Calculate time difference
	int64_t start_ns = (int64_t)(start_time.tv_sec) * (int64_t)1000000000 + (int64_t)(start_time.tv_nsec);
	int64_t end_ns = (int64_t)(end_time.tv_sec) * (int64_t)1000000000 + (int64_t)(end_time.tv_nsec);
	int64_t ns_time = end_ns - start_ns;

	int64_t us_from_last_switch = (ns_time - last_time) / 1000;
	last_time = ns_time;

	// Write trace data
	uint8_t task_nr = (uint8_t)task_number;
	write_trace(us_from_last_switch, &task_nr, sizeof(task_nr), trace_file);
}

// Copyright 2024 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#ifndef BATCH_PROCESSOR_H_
#define BATCH_PROCESSOR_H_

#include <xcore/chanend.h>

/* Entry point for batch processing mode.
 * Called from main_tile0() instead of starting FreeRTOS.
 * Reads input WAV from host via xscope_fileio, processes through
 * full DSP pipeline, writes stage outputs back to host as WAV files.
 * Does not return — calls _Exit(0) when done. */
void batch_process(chanend_t c_xscope);

#endif /* BATCH_PROCESSOR_H_ */

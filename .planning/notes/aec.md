Why AEC Uses So Much Memory

Looking at the AEC memory pool (aec_memory_pool.h) and the defines (aec_defines.h), here's what causes the memory explosion with 4 channels:

Key Memory Scaling Factors

1. Main Filter Phase Pool (phase_pool_H_hat_X_fifo)
   complex_s32_t phase_pool_H_hat_X_fifo[
   ((AEC_MAX_Y_CHANNELS * AEC_MAX_X_CHANNELS * AEC_MAIN_FILTER_PHASES) +
   (AEC_MAX_X_CHANNELS * AEC_MAIN_FILTER_PHASES)
   ] \* AEC_FD_FRAME_LENGTH


    - 2 channels: `(2210 + 2*10) * 257 = 102,800 complex samples ≈ 205,600 bytes (4 bytes per complex)
    - 4 channels: `(4210 + 2*10) * 257 = 205,600 complex samples ≈ 411,200 bytes (4 bytes per complex)
    - Memory increase: ~205,600 bytes (~201 KB) for the phase pool alone

2. Total AEC Memory Scales Quadratically
   The AEC maintains separate filter states for each mic-to-ref channel pair:


    - 2 mics × 2 refs = 4 pairs (2 y-channels × 2 x-channels)
    - 4 mics × 2 refs = 8 pairs (4 y-channels × 2 x-channels)

3. Shadow Filter Also Scales


    - The shadow filter has its own phase_pool_H_hat array (smaller but still significant)
    - 2 channels: ~107 KB for shadow pool
    - 4 channels: ~215 KB for shadow pool (doubles)

Why This Scaling Exists

The AEC uses a multi-channel adaptive filter where:

- Each microphone has its own echo cancellation filter
- Each filter maintains frequency-domain coefficients across multiple phases (10 main + 5 shadow)
- The coefficients are stored as complex numbers (real + imaginary parts) → 2× storage
- Each phase needs a full spectrum (257 frequency bins for 512-point FFT)

The Formula

AEC Memory ≈ (y_channels × x_channels × phases × frequency_bins × 2) × 4
= y_channels × x_channels × 10 × 257 × 8 bytes

For 2 mics, 2 refs, 10 phases:
≈ 2 × 2 × 10 × 257 × 8 = ~82 KB

For 4 mics, 2 refs, 10 phases:
≈ 4 × 2 × 10 × 257 × 8 = ~164 KB (doubles)

This is why going from 2 to 4 channels approximately doubles the AEC memory usage, causing the tile memory overflow.

Options to Reduce Memory

1. Reduce filter phases: 10 → 5 (saves ~41 KB for 4 channels)
2. Remove shadow filter: Saves ~107 KB for 4 channels
3. Use only 2-channel AEC: Process mics in pairs (requires more CPU)
4. Skip AEC for 4-mic mode: Current bypass_4mic approach

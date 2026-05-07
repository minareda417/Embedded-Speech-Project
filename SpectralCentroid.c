/*
 * spectral_centroid.c
 * -------------------
 * Computes the spectral centroid from an FFT magnitude spectrum
 * and stores the result in features[2].
 *
 * Target: ATmega32A  (8 MHz / 16 MHz, 2 KB SRAM, 32 KB Flash)
 * Audio:  8 kHz sample rate, 8-bit PCM, FFT_N = 256 points
 *
 * The spectral centroid is the "centre of mass" of the spectrum:
 *
 *          sum( bin_index[k] * magnitude[k] )
 *  SC =   ------------------------------------   (in Hz)
 *               sum( magnitude[k] )
 *
 * Only the first FFT_N/2 bins (0 ? 127) are meaningful for a
 * real-valued signal; bin k corresponds to frequency
 *   f_k = k * Fs / FFT_N  =  k * 8000 / 256  =  k * 31.25 Hz
 *
 * To avoid floating-point on the MCU the ratio is kept as a
 * fixed-point integer (bin index) and converted to Hz only at
 * the very end ? one integer multiply instead of 128 multiplies.
 *
 * features[2] is written in Hz (float, for the classifier).
 * If all magnitudes are zero the centroid defaults to 0 Hz.
 */

#include <stdint.h>
#include "fft.h"          /* FFT_N, uint8_t spectrum type  */

/* Fs / FFT_N  expressed as a scaled integer to stay in integer math.
 * Fs = 8000 Hz, FFT_N = 256  ?  bin_width = 31.25 Hz
 * We work in units of 1/4 Hz  ?  scaled_bin_width = 125               */
#define BIN_WIDTH_Q2    125u          /* bin width × 4  (units: 0.25 Hz) */
#define NUM_BINS        (FFT_N / 2)   /* 128 useful bins                 */

/*
 * spectral_centroid()
 *
 * Parameters
 * ----------
 * spectrum  : pointer to the FFT_N/2 magnitude values produced by
 *             fft_output() ? each entry is a uint8_t.
 * features  : the shared feature vector; result goes into features[2].
 */
void spectral_centroid(const uint8_t *spectrum, float *features)
{
    uint32_t weighted_sum = 0;   /* sum( k * mag[k] ) ? fits in 32 bits:
                                  *  max = 127 * 255 * 128 = 4,145,280   */
    uint32_t total_mag    = 0;   /* sum( mag[k] )       ? max = 255 * 128
                                  *                             = 32,640  */
    uint8_t  k;

    for (k = 0; k < NUM_BINS; k++) {
        uint8_t mag = spectrum[k];
        total_mag    += mag;
        weighted_sum += (uint32_t)k * mag;
    }

    if (total_mag == 0) {
        features[2] = 0.0f;
        return;
    }

    /*
     * centroid_bin  = weighted_sum / total_mag   (fractional bin index)
     *
     * centroid_Hz   = centroid_bin * (Fs / FFT_N)
     *               = centroid_bin * 31.25
     *
     * Integer path (avoids a float division):
     *   centroid_Hz × 4  =  weighted_sum * BIN_WIDTH_Q2 / total_mag
     * then divide by 4 at the end.
     *
     * Both operands of the 32-bit multiply fit comfortably:
     *   weighted_sum_max = 4,145,280   BIN_WIDTH_Q2 = 125
     *   product_max      ? 518 M  ? fits in uint32_t (max ~4.3 G).
     */
    uint32_t centroid_q2 = (weighted_sum * (uint32_t)BIN_WIDTH_Q2)
                           / total_mag;          /* units: 0.25 Hz */

    features[2] = (float)centroid_q2 * 0.25f;   /* convert to Hz  */
}

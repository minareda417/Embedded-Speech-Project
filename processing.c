/*
 * processing.c
 * ------------
 * Features stored in the shared feature vector:
 *
 *   features[0]  ? Short-time energy  (sum of squared samples)
 *   features[1]  ? Zero-crossing rate (crossings of the value 64,
 *                  i.e. the ADC mid-scale in 8-bit unsigned mode)
 *   features[2]  ? Spectral centroid  ? filled by spectral_centroid()
 *   features[3?15] ? 13 MFCCs        ? filled by compute_mfcc()
 *
 * Target : ATmega32A, 8-bit PCM samples at 8 kHz, 1-second buffer
 *          (8000 samples stored as uint8_t).
 *
 * Bugs fixed vs original:
 *   ? #define <stdint.h>  ? #include <stdint.h>
 *   ? #define "FFT.h"     ? #include "fft.h"
 *   ? Pointer parameters (uint8_t*, float*)  not references (&)
 *   ? ZCR condition was always false  (audio[i]>=64 && audio[i]<64)
 *   ? ZCR index used features[i] instead of features[1]
 *   ? features[0] accumulated uint8_t×uint8_t into float without cast
 *     ? can silently overflow on 8-bit; now cast explicitly to uint16_t
 */

#include <stdint.h>
#include "fft.h"

#define AUDIO_LEN  8000u   /* 1 second at 8 kHz                  */
#define MID_SCALE  128u    /* ADC mid-scale for 8-bit unsigned    */

/*
 * power_and_zcr()
 *
 * Parameters
 * ----------
 * audio    : 8-bit unsigned PCM buffer of AUDIO_LEN samples
 *            (ADC values 0?255; silence ? 128)
 * features : feature vector; writes features[0] and features[1]
 *
 * features[0] = sum of (sample ? 128)² over all samples
 *               (mean-removed energy, so DC offset is cancelled)
 * features[1] = number of times the signal crosses the mid-scale
 *               value (128) between consecutive samples
 */
void power_and_zcr(const uint8_t *audio, float *features)
{
    float energy = 0.0f;
    uint16_t zcr = 0;

    uint16_t i;
    for (i = 0; i < AUDIO_LEN; i++) {
        /* Mean-removed sample (centre around 0) */
        int16_t s = (int16_t)audio[i] - (int16_t)MID_SCALE;

        /* Short-time energy: accumulate s² */
        energy += (float)((int32_t)s * s);

        /* Zero-crossing: sign change between consecutive samples */
        if (i < AUDIO_LEN - 1) {
            int16_t s_next = (int16_t)audio[i + 1] - (int16_t)MID_SCALE;
            /* A crossing occurs when the product of adjacent
             * mean-removed samples is negative (opposite signs). */
            if ((s > 0 && s_next < 0) || (s < 0 && s_next > 0))
                zcr++;
        }
    }

    features[0] = energy;
    features[1] = (float)zcr;
}
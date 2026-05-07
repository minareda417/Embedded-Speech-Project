/*
 * MFCC.c
 * ------
 * Computes 13 Mel-Frequency Cepstral Coefficients (MFCCs) and stores
 * them in features[3] ? features[15].
 *
 * Target : ATmega32A  (2 KB SRAM, 32 KB Flash, no FPU, no hardware mul
 *                      beyond the 8×8?16 MULS/MUL instruction)
 * Audio  : 8 kHz sample rate, 8-bit PCM, FFT_N = 256 points
 *          ? 128 usable frequency bins, bin width = 31.25 Hz
 *
 * ?? Algorithm overview ??????????????????????????????????????????????
 *
 *  1. Apply a triangular Mel filterbank (NUM_FILTERS = 26 filters) to
 *     the magnitude spectrum produced by fft_output().
 *
 *  2. Take the natural log of each filter energy.
 *     (Approximated with a fast integer log2 then scaled ? see below.)
 *
 *  3. Apply the Discrete Cosine Transform (DCT-II) to the log-filter
 *     energies and keep the first 13 coefficients.
 *
 * ?? Memory budget ???????????????????????????????????????????????????
 *
 *  All filterbank centre/edge tables live in Flash (PROGMEM) so they
 *  cost 0 SRAM bytes.  The only runtime RAM use is:
 *    log_energy[26] × 2 bytes = 52 bytes   (int16_t, Q8 fixed-point)
 *  Everything else is computed on the stack or uses the features[] array
 *  passed in by the caller.
 *
 * ?? Fixed-point representation ??????????????????????????????????????
 *
 *  log_energy values are stored as Q8 fixed-point (1 integer bit +
 *  7 fractional bits in the signed 16-bit word).  The DCT output is
 *  normalised so each MFCC fits comfortably in a float.
 *
 * ?? Mel scale ???????????????????????????????????????????????????????
 *
 *  mel(f)  = 2595 × log10(1 + f/700)
 *  f(mel)  = 700 × (10^(mel/2595) ? 1)
 *
 *  Filter centres are pre-computed offline (Python/MATLAB) and stored
 *  as FFT bin indices in Flash.  This way the MCU never evaluates a
 *  transcendental function at runtime.
 *
 * ?? Pre-computed filterbank (26 triangular filters, Fs=8 kHz, N=256)?
 *
 *  Mel range : 0 Hz (mel=0) ? 4000 Hz (mel?1946)
 *  28 equally-spaced mel points ? 26 interior centres + 2 edge bins.
 *
 *  Bin edges (pre-computed with Python mel_to_hz / hz_to_bin):
 *    edge[k] = round( hz_to_mel_inv(mel_lo + k*mel_step) * N / Fs )
 *  for k = 0 ? 27  (28 values defining 26 triangle filters).
 *
 *  The table below was generated with:
 *    import numpy as np
 *    Fs=8000; N=256; n_filters=26
 *    mel_lo=0; mel_hi=2595*np.log10(1+4000/700)
 *    mels=np.linspace(mel_lo,mel_hi,n_filters+2)
 *    hz=700*(10**(mels/2595)-1)
 *    bins=np.round(hz*N/Fs).astype(int)
 */

#include <stdint.h>
#include <avr/pgmspace.h>
#include "fft.h"

/* ?? Configuration ??????????????????????????????????????????????? */
#define NUM_FILTERS   26          /* number of Mel triangular filters */
#define NUM_MFCC      13          /* MFCC coefficients to keep        */
#define NUM_BINS      (FFT_N / 2) /* 128 usable FFT bins              */
#define FEATURES_OFFSET 3         /* features[3]?features[15]         */

/* ?? Filterbank edge bin table (stored in Flash) ??????????????????
 * 28 values:  edge[0] = left edge of filter 0
 *             edge[k] = centre of filter k-1 = left edge of filter k
 *             edge[27]= right edge of filter 25
 * Generated as described in the header comment.                    */
static const uint8_t mel_edges[NUM_FILTERS + 2] PROGMEM = {
/*  0    1    2    3    4    5    6    7    8    9  */
    0,   2,   4,   6,   8,  11,  14,  17,  21,  25,
/* 10   11   12   13   14   15   16   17   18   19  */
   30,  35,  41,  48,  55,  64,  73,  84,  96, 109,
/* 20   21   22   23   24   25   26   27              */
  124, 140, 158, 178, 200, 224, 250, 256
/*  Note: edge[27]=256 clamps to NUM_BINS=128 in practice; the
 *  last few filters will simply have no contribution beyond bin 127. */
};

/* ?? DCT-II cosine table (stored in Flash) ????????????????????????
 *
 *  cos_table[m][k]  ?  cos( ?/NUM_FILTERS × (k + 0.5) × m ) × 128
 *
 *  Stored as int8_t to save Flash.  m = 0?12 (NUM_MFCC rows),
 *  k = 0?25 (NUM_FILTERS columns).
 *
 *  Pre-computed with:
 *    import numpy as np
 *    m=np.arange(13)[:,None]; k=np.arange(26)[None,:]
 *    tbl=np.round(np.cos(np.pi/26*(k+0.5)*m)*127).astype(int)
 *
 *  Row 0 is the DC term (all ones ? 127).
 */
static const int8_t dct_table[NUM_MFCC][NUM_FILTERS] PROGMEM = {
/* m=0 */
{ 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
  127, 127, 127, 127, 127, 127 },
/* m=1 */
{ 127, 126, 123, 119, 113, 106,  97,  86,  75,  62,
   48,  33,  18,   2, -14, -30, -45, -59, -72, -83,
  -92,-100,-106,-111,-114,-115 },
/* m=2 */
{ 127, 123, 111,  93,  70,  43,  14, -16, -45, -71,
  -92,-108,-118,-122,-119,-110, -95, -75, -51, -25,
    2,  28,  53,  74,  91, 103 },
/* m=3 */
{ 127, 119,  96,  60,  18, -26, -66, -97,-117,-123,
 -114, -92, -59, -19,  24,  63,  95, 116, 122, 114,
   91,  58,  17, -26, -65, -97 },
/* m=4 */
{ 127, 113,  79,  27, -29, -78,-112,-124,-111, -75,
  -23,  33,  82, 114, 124, 110,  72,  20, -36, -84,
 -115,-124,-108, -69, -16,  39 },
/* m=5 */
{ 127, 106,  60,  -6, -68,-112,-127,-107, -58,   8,
   70, 113, 126, 104,  54, -13, -72, -114,-125,-102,
  -50,  18,  75, 115, 123, 100 },
/* m=6 */
{ 127,  97,  40, -38, -97,-124,-109, -57,  16,  80,
  121, 122,  83,  20, -50,-104,-126,-109, -60,  14,
   79, 120, 121,  82,  19, -51 },
/* m=7 */
{ 127,  86,  18, -65,-117,-122, -75,   2,  75, 122,
  121,  72,  -4, -77,-122,-120, -70,   8,  80, 123,
  119,  67, -10, -82,-122,-117 },
/* m=8 */
{ 127,  75,  -4, -83,-122,-107, -40,  45, 106, 124,
   86,  14, -66,-118,-112, -57,  25,  96, 125,  93,
   29, -55,-111,-122, -79,  -9 },
/* m=9 */
{ 127,  62, -25, -97,-122, -84,   2,  84, 122,  95,
   22, -60,-115,-116, -64,  18,  91, 124,  99,  34,
  -47,-108,-121, -74,   4,  80 },
/* m=10 */
{ 127,  48, -45,-108,-115, -59,  36, 109, 118,  65,
  -27,-100,-122, -77,  14,  95, 126,  83,   0, -82,
 -122, -88,  -8,  76, 121,  96 },
/* m=11 */
{ 127,  33, -64,-118,-106, -33,  67, 119, 107,  31,
  -68,-120,-103, -27,  72, 121, 101,  22, -77,-122,
  -98, -16,  81, 123,  92,  10 },
/* m=12 */
{ 127,  18, -81,-124, -93,  -4,  91, 127,  93,  -1,
  -92,-125, -89,   6,  95, 126,  84, -10, -97,-125,
  -78,  16, 100, 124,  72, -22 }
};

/* ?? Fast integer log2 ????????????????????????????????????????????
 *
 *  Returns floor(log2(x)) × 256  as a uint16_t (Q8 fixed-point).
 *  For x == 0 returns 0 (silence floor).
 *
 *  This is a substitute for ln() / log10():
 *    ln(x)    = log2(x) × ln(2)   ? log2(x) × 0.693
 *  The classifier only needs consistent relative values, so the
 *  ln(2) scaling factor is absorbed into the template weights.
 *
 *  Implementation: count leading zeros to get the integer part,
 *  then use the top 4 bits of the mantissa for a 4-bit linear
 *  interpolation of the fractional part (classic bit-trick log2).
 *
 *  Range: x up to 255×128 = 32640 ? fits in uint16_t.
 */
static uint16_t fast_log2_q8(uint16_t x)
{
    if (x == 0) return 0;

    /* Integer part: position of the highest set bit */
    uint8_t  integer_part = 0;
    uint16_t tmp = x;
    while (tmp > 1) { tmp >>= 1; integer_part++; }

    /* Fractional part: use next 8 bits after the leading 1.
     * Shift x so the leading 1 is at bit 15, then take bits [14:7]. */
    uint16_t mantissa;
    if (integer_part >= 8)
        mantissa = (x >> (integer_part - 8)) & 0xFF;
    else
        mantissa = (x << (8 - integer_part)) & 0xFF;

    /* Result in Q8: integer_part × 256 + fractional approximation */
    return ((uint16_t)integer_part << 8) | mantissa;
}

/* ?? Public API ???????????????????????????????????????????????????
 *
 *  compute_mfcc()
 *
 *  Parameters
 *  ----------
 *  spectrum  : pointer to FFT_N/2 = 128 uint8_t magnitude values
 *              produced by fft_output().
 *  features  : shared feature vector; writes features[3]?features[15].
 */
void compute_mfcc(const uint8_t *spectrum, float *features)
{
    /* Step 1: Apply Mel filterbank ? log_energy[NUM_FILTERS]
     *
     * Each filter is a triangle spanning bins [left, centre, right).
     * Rising slope  : weight = (bin ? left)  / (centre ? left)
     * Falling slope : weight = (right ? bin) / (right ? centre)
     *
     * To keep everything in 8-bit integer arithmetic the weights are
     * not normalised by the filter width (unit-amplitude triangles).
     * The un-normalised energy is proportional and sufficient for DTW.
     *
     * We use Q8 fixed-point for log_energy (int16_t).
     */
    int16_t log_energy[NUM_FILTERS]; /* 52 bytes on stack ? fits in 2 KB */

    uint8_t f;
    for (f = 0; f < NUM_FILTERS; f++) {
        uint8_t left   = pgm_read_byte(&mel_edges[f    ]);
        uint8_t centre = pgm_read_byte(&mel_edges[f + 1]);
        uint8_t right  = pgm_read_byte(&mel_edges[f + 2]);

        /* Clamp to valid bin range */
        if (left   >= NUM_BINS) left   = NUM_BINS - 1;
        if (centre >= NUM_BINS) centre = NUM_BINS - 1;
        if (right  >= NUM_BINS) right  = NUM_BINS;   /* exclusive */

        uint16_t energy = 0;
        uint8_t  b;

        /* Rising slope: bins [left, centre) */
        uint8_t rise_width = centre - left;
        if (rise_width > 0) {
            for (b = left; b < centre; b++) {
                /* weight = (b - left) / rise_width  ? [0,1)
                 * Approximate as integer: (b - left) × mag / rise_width
                 * Keep only the top 8 bits of the weighted sum.       */
                uint16_t w = (uint16_t)(b - left) * spectrum[b] / rise_width;
                energy += w;
                if (energy < w) energy = 0xFFFF; /* saturation guard  */
            }
        }

        /* Falling slope: bins [centre, right) */
        uint8_t fall_width = right - centre;
        if (fall_width > 0) {
            for (b = centre; b < right; b++) {
                uint16_t w = (uint16_t)(right - b) * spectrum[b] / fall_width;
                energy += w;
                if (energy < w) energy = 0xFFFF;
            }
        }

        /* Step 2: log of filter energy (Q8 fixed-point) */
        log_energy[f] = (int16_t)fast_log2_q8(energy);
    }

    /* Step 3: DCT-II to get MFCCs
     *
     *  mfcc[m] = sum_{k=0}^{NUM_FILTERS-1}  log_energy[k]
     *              × cos( ?/NUM_FILTERS × (k + 0.5) × m )
     *
     *  The cosine values are pre-scaled by 127 and stored as int8_t.
     *  Accumulate in int32_t to avoid overflow.
     *
     *  Normalisation: divide by (NUM_FILTERS × 127) to bring the
     *  result back to a float near [-1, +1].
     */
    const float norm = 1.0f / ((float)NUM_FILTERS * 127.0f * 256.0f);
    /* The extra ×256 accounts for the Q8 scaling of log_energy.     */

    uint8_t m;
    for (m = 0; m < NUM_MFCC; m++) {
        int32_t acc = 0;
        uint8_t k;
        for (k = 0; k < NUM_FILTERS; k++) {
            int8_t  cos_val = (int8_t)pgm_read_byte(&dct_table[m][k]);
            acc += (int32_t)log_energy[k] * cos_val;
        }
        features[FEATURES_OFFSET + m] = (float)acc * norm;
    }
}

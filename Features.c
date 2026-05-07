/*
 * features.h
 * ----------
 * Declares all feature extraction functions and the shared
 * feature vector layout for the ATmega32A speech recogniser.
 *
 * Feature vector layout (16 elements total):
 *
 *  Index  Symbol              Filled by
 *  ?????  ??????????????????  ?????????????????????
 *    0    Short-time energy   power_and_zcr()
 *    1    Zero-crossing rate  power_and_zcr()
 *    2    Spectral centroid   spectral_centroid()
 *   3?15  MFCC[0]?MFCC[12]   compute_mfcc()
 */

#ifndef FEATURES_H
#define FEATURES_H

#include <stdint.h>
#include "fft.h"   /* uint8_t spectrum, FFT_N */

#define N_FEATURES  16   /* total number of features */

/* ?? Feature indices ???????????????????????????????????????????? */
#define FEAT_ENERGY    0
#define FEAT_ZCR       1
#define FEAT_SC        2
#define FEAT_MFCC_0    3
#define FEAT_MFCC_12  15

/* ?? Function prototypes ???????????????????????????????????????? */

/*
 * power_and_zcr()  [processing.c]
 * Computes short-time energy ? features[0]
 *          zero-crossing rate ? features[1]
 */
void power_and_zcr(const uint8_t *audio, float *features);

/*
 * spectral_centroid()  [spectral_centroid.c]
 * Computes spectral centroid (Hz) ? features[2]
 * spectrum[] must be the FFT_N/2 output of fft_output().
 */
void spectral_centroid(const uint8_t *spectrum, float *features);

/*
 * compute_mfcc()  [MFCC.c]
 * Computes 13 MFCCs ? features[3] ? features[15]
 * spectrum[] must be the FFT_N/2 output of fft_output().
 */
void compute_mfcc(const uint8_t *spectrum, float *features);

#endif /* FEATURES_H */

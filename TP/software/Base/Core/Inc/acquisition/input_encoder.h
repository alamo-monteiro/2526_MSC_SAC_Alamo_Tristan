/*
 * input_encoder.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#ifndef ACQUISITION_INPUT_ENCODER_H_
#define ACQUISITION_INPUT_ENCODER_H_

#include <stdint.h>
#include "stm32g4xx_hal.h"

// Encodeur : 1000 PPR physiques, lecture quadrature timer => x4
#define ENCODER_PPR_PHYS          1000U
#define ENCODER_COUNTS_PER_REV    (ENCODER_PPR_PHYS * 4U)

// Période d'échantillonnage vitesse
#define ENCODER_SAMPLE_DT_MS      10U  // 10 ms => 100 Hz

void input_encoder_init(TIM_HandleTypeDef *htim_enc, TIM_HandleTypeDef *htim_sample);
void input_encoder_start(void);

// à appeler dans l'ISR du timer 10 ms (TIM7 par ex.)
void input_encoder_sample_isr(void);

// getters
int32_t input_encoder_get_rpm(void);          // rpm signé
int32_t input_encoder_get_mrpm(void);         // milli-rpm
int32_t input_encoder_get_mrad_s(void);       // milli rad/s

// optionnel : reset compteur vitesse
void input_encoder_reset(void);

#endif /* ACQUISITION_INPUT_ENCODER_H_ */

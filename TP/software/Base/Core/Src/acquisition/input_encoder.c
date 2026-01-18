/*
 * input_encoder.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "acquisition/input_encoder.h"

static TIM_HandleTypeDef *s_htim_enc = NULL;
static TIM_HandleTypeDef *s_htim_sample = NULL;

static volatile int32_t s_rpm = 0;
static volatile int32_t s_mrpm = 0;
static volatile int32_t s_mrad_s = 0;

static uint16_t s_last_cnt = 0;

void input_encoder_init(TIM_HandleTypeDef *htim_enc, TIM_HandleTypeDef *htim_sample)
{
    s_htim_enc = htim_enc;
    s_htim_sample = htim_sample;
}

void input_encoder_start(void)
{
    if (!s_htim_enc || !s_htim_sample) return;

    // Démarre la lecture encodeur (TIM3 en Encoder Mode)
    HAL_TIM_Encoder_Start(s_htim_enc, TIM_CHANNEL_ALL);

    // Initialise l'état précédent
    s_last_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(s_htim_enc);

    // Démarre le timer 10 ms en IT (TIM7 par ex.)
    HAL_TIM_Base_Start_IT(s_htim_sample);
}

void input_encoder_reset(void)
{
    if (!s_htim_enc) return;
    __HAL_TIM_SET_COUNTER(s_htim_enc, 0);
    s_last_cnt = 0;
    s_rpm = 0;
    s_mrpm = 0;
    s_mrad_s = 0;
}



void input_encoder_sample_isr(void)
{
    if (!s_htim_enc) return;

    uint16_t cnt = (uint16_t)__HAL_TIM_GET_COUNTER(s_htim_enc);

    // delta signé avec wrap 16-bit (net movement sur 10ms)
    int16_t delta = (int16_t)(cnt - s_last_cnt);
    s_last_cnt = cnt;

    // (optionnel) anti-glitch : rejette les deltas absurdes
    // À 30k rpm, delta ~ 20000 counts/10ms (avec 4000 counts/rev).
    // Donc au-delà de ~30000 c'est très probablement un glitch.
    if (delta > 30000 || delta < -30000) {
        delta = 0;
    }

    s_rpm  = (int32_t)delta * 6000 / (int32_t)ENCODER_COUNTS_PER_REV;
    s_mrpm = (int32_t)delta * 6000000 / (int32_t)ENCODER_COUNTS_PER_REV;
}


int32_t input_encoder_get_rpm(void)    { return s_rpm; }
int32_t input_encoder_get_mrpm(void)   { return s_mrpm; }
int32_t input_encoder_get_mrad_s(void) { return s_mrad_s; }


int32_t input_encoder_get_speed_rpm(void)
{
    return input_encoder_get_rpm();
}

int32_t input_encoder_get_speed_mrad_s(void)
{
    return input_encoder_get_mrad_s();
}


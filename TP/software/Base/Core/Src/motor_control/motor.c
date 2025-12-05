/*
 * motor.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */





#include "motor_control/motor.h"
#include "tim.h"
#include "user_interface/shell.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>



//  Fonctions internes

// Petit printf local pour le shell, basé sur le driver transmit
static void motor_shell_printf(h_shell_t *h_shell, const char *fmt, ...)
{
    char buffer[128];   // taille suffisante pour nos messages
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (len <= 0)
    {
        return;
    }
    if (len > (int)sizeof(buffer))
    {
        len = sizeof(buffer);
    }

    if (h_shell && h_shell->drv.transmit)
    {
        h_shell->drv.transmit(buffer, (uint16_t)len);
    }
}


static inline uint32_t motor_get_arr(void)
{
    return __HAL_TIM_GET_AUTORELOAD(&htim1);
}

//  API moteur "bas niveau"

void motor_set_duty_percent(uint32_t duty_percent)
{
    if (duty_percent > 100U)
    {
        duty_percent = 100U;
    }

    uint32_t arr = motor_get_arr();

    // Duty pour le bras U (CH1)
    uint32_t ccr_u = (arr + 1U) * duty_percent / 100U;

    // Duty pour le bras V (CH2) = complément "décalé"
    uint32_t ccr_v = (arr + 1U) * (100U - duty_percent) / 100U;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr_u); // bras U
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr_v); // bras V
}



void motor_init(void)
{
    // Duty de test (et valeur par défaut au démarrage)
    motor_set_duty_percent(MOTOR_PWM_DUTY_OPEN_LOOP_PC);
}

HAL_StatusTypeDef motor_start(void)
{
    HAL_StatusTypeDef status = HAL_OK;

    status |= HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    status |= HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);

    status |= HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    status |= HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

    return status;
}

// Commande shell : "speed XXXX"

int sh_speed(h_shell_t *h_shell, int argc, char **argv)
{
    if (argc < 2)
    {
        motor_shell_printf(h_shell, "usage: speed 0-%u\r\n", MOTOR_SPEED_CMD_MAX);
        return -1;
    }

    char *endptr = NULL;
    long raw_value = strtol(argv[1], &endptr, 10);

    // Vérif : tous les caractères doivent être des chiffres
    if ((endptr == argv[1]) || (*endptr != '\0'))
    {
        motor_shell_printf(h_shell, "speed: invalid value '%s'\r\n", argv[1]);
        return -1;
    }

    // Saturation
    if (raw_value < 0)
    {
        raw_value = 0;
    }
    if ((uint32_t)raw_value > MOTOR_SPEED_CMD_MAX)
    {
        raw_value = MOTOR_SPEED_CMD_MAX;
    }

    // Mapping 0..MOTOR_SPEED_CMD_MAX -> 0..100 %
    uint32_t duty_percent = (uint32_t)raw_value * 100U / MOTOR_SPEED_CMD_MAX;

    motor_set_duty_percent(duty_percent);

    motor_shell_printf(h_shell,
                       "speed set: cmd=%ld (max=%u) -> duty=%lu%%\r\n",
                       raw_value, MOTOR_SPEED_CMD_MAX, duty_percent);

    return 0;
}



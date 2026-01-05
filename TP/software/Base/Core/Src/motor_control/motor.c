/*
 * motor.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */





#include "motor_control/motor.h"
#include "tim.h"
#include "user_interface/shell.h"
#include "acquisition/input_analog.h"


#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static uint32_t motor_current_duty_percent = 0;   // en % (0..100)
static uint32_t motor_target_duty_percent  = 0;   // consigne en % (0..100)


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
    if (duty_percent > 100U) {
        duty_percent = 100U;
    }
    motor_target_duty_percent = duty_percent;   // on ne fait que fixer la consigne
}


void motor_update(void)
{
    uint32_t arr = motor_get_arr();

    const uint32_t step_percent   = 5U;   // 1 % par step
    const uint32_t step_delay_ms  = 100U;  // 10 ms entre deux steps

    static uint32_t last_tick = 0U;

    uint32_t tick = HAL_GetTick();

    // On avance la rampe seulement toutes les step_delay_ms
    if ((tick - last_tick) < step_delay_ms) {
        return;
    }
    last_tick = tick;

    uint32_t start  = motor_current_duty_percent;
    uint32_t target = motor_target_duty_percent;

    if (start == target) {
        return; // déjà à la bonne valeur
    }

    // On se rapproche de la cible d'un pas de 1 %
    if (target > start) {
        start += step_percent;
        if (start > target) start = target;
    } else {
        if (start > step_percent) {
            start -= step_percent;
        } else {
            start = target;
        }
    }

    motor_current_duty_percent = start;

    // Calcul CCR comme avant (complémentaire U/V)
    uint32_t ccr_u = (arr + 1U) * start / 100U;
    uint32_t ccr_v = (arr + 1U) * (100U - start) / 100U;

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

HAL_StatusTypeDef motor_stop(void)
{
    HAL_StatusTypeDef status = HAL_OK;

    status |= HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    status |= HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);

    status |= HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
    status |= HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

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

    if ((endptr == argv[1]) || (*endptr != '\0'))
    {
        motor_shell_printf(h_shell, "speed: invalid value '%s'\r\n", argv[1]);
        return -1;
    }

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






int sh_start(h_shell_t *h_shell, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    // Point neutre : 50 %
    motor_set_duty_percent(50U);

    if (motor_start() != HAL_OK)
    {
        motor_shell_printf(h_shell, "start: ERROR starting PWM\r\n");
        return -1;
    }

    // On demande une calibration dans quelques ms
    input_analog_request_calibrate();

    motor_shell_printf(h_shell, "start: PWM enabled at 50%% (zero speed, calib en cours)\r\n");
    return 0;
}




int sh_stop(h_shell_t *h_shell, int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (motor_stop() != HAL_OK)
    {
        motor_shell_printf(h_shell, "stop: ERROR stopping PWM\r\n");
        return -1;
    }

    // On peut aussi recentrer la consigne à 50% pour être neutre au prochain start
    motor_set_duty_percent(50U);
    motor_shell_printf(h_shell, "stop: PWM disabled\r\n");
    return 0;
}




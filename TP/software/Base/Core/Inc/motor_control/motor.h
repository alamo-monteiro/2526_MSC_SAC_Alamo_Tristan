/*
 * motor.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */




#ifndef MOTOR_CONTROL_MOTOR_H
#define MOTOR_CONTROL_MOTOR_H

#include "main.h"
#include "user_interface/shell.h"

#define MOTOR_PWM_DUTY_OPEN_LOOP_PC   60U      // duty de test
#define MOTOR_SPEED_CMD_MAX           1000U    // "XXXX" max dans speed XXXX

void motor_init(void);
HAL_StatusTypeDef motor_start(void);
HAL_StatusTypeDef motor_stop(void);
void motor_set_duty_percent(uint32_t duty_percent);
void motor_update(void);


// Commande shell : speed XXXX
int sh_speed(h_shell_t *h_shell, int argc, char **argv);
int sh_start(h_shell_t *h_shell, int argc, char **argv);
int sh_stop(h_shell_t *h_shell, int argc, char **argv);

#endif /* MOTOR_CONTROL_MOTOR_H */


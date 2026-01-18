#ifndef ACQUISITION_INPUT_ANALOG_H_
#define ACQUISITION_INPUT_ANALOG_H_

#include <stdint.h>

void input_analog_init(void);

void input_analog_get_currents(float *i_u, float *i_v);
void input_analog_get_currents_ma(int32_t *iu_ma, int32_t *iv_ma);

void input_analog_request_calibrate(void);
void input_analog_task(void);

float input_analog_get_bus_current(void);
void input_analog_get_raw(uint16_t *ch0, uint16_t *ch1);



void input_analog_get_offsets(float *off_u, float *off_v);
void input_analog_get_offsets_mv(int32_t *off_u_mv, int32_t *off_v_mv);
int input_analog_calibrate_zero_current_avg(uint16_t n);

int32_t input_encoder_get_speed_rpm(void);
int32_t input_encoder_get_speed_mrad_s(void);




#endif /* ACQUISITION_INPUT_ANALOG_H_ */

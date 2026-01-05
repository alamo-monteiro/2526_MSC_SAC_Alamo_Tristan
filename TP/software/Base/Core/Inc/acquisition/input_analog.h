#ifndef ACQUISITION_INPUT_ANALOG_H_
#define ACQUISITION_INPUT_ANALOG_H_

#include <stdint.h>

void input_analog_init(void);

void input_analog_get_currents(float *i_u, float *i_v);
void input_analog_get_currents_ma(int32_t *iu_ma, int32_t *iv_ma);

void input_analog_request_calibrate(void);
void input_analog_task(void);

float input_analog_get_bus_current(void);
void input_analog_get_raw(uint16_t *raw_u, uint16_t *raw_v);


#endif /* ACQUISITION_INPUT_ANALOG_H_ */

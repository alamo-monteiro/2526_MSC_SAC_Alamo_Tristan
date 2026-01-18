#include "acquisition/input_analog.h"
#include "adc.h"
#include <stdint.h>

#define ADC_VREF      3.3f
#define ADC_MAX       4095.0f

// À ADAPTER selon datasheet du capteur
#define GO_SME_UREF        1.65f      // typique si capteur centré Vref/2
#define GO_SME_SENSITIVITY 0.05f      // V/A  (=> *20 au lieu de /0.05)

#define ADC_DMA_CHANNELS 2

volatile uint16_t adc_dma_buffer[ADC_DMA_CHANNELS];

// Offsets (si tu veux calibrer un “0A” logiciel)
static float corr_v_offset_u = 0.0f;
static float corr_v_offset_v = 0.0f;

static uint8_t calib_pending = 0;
static uint32_t calib_tick = 0;

static inline float adc_to_voltage(uint16_t adc, float corr)
{
    return ((float)adc * ADC_VREF) / ADC_MAX + corr;
}

static inline float voltage_to_current(float u_mes)
{
    return (u_mes - GO_SME_UREF) * (1.0f / GO_SME_SENSITIVITY); // ou *(20.0f)
}

void input_analog_init(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, ADC_DMA_CHANNELS);
}

void input_analog_get_currents(float *i_u, float *i_v)
{
    uint16_t raw_u = adc_dma_buffer[0];
    uint16_t raw_v = adc_dma_buffer[1];

    float u_u = adc_to_voltage(raw_u, corr_v_offset_u);
    float u_v = adc_to_voltage(raw_v, corr_v_offset_v);

    if (i_u) *i_u = voltage_to_current(u_u);
    if (i_v) *i_v = voltage_to_current(u_v);
}

// Option calibration : on force I=0A -> on ajuste corr_v_offset_*
// À appeler quand tu es sûr que courant réel = 0A
void input_analog_calibrate_zero_current(void)
{
    uint16_t raw_u = adc_dma_buffer[0];
    uint16_t raw_v = adc_dma_buffer[1];

    float u_u = ((float)raw_u * ADC_VREF) / ADC_MAX;
    float u_v = ((float)raw_v * ADC_VREF) / ADC_MAX;

    // On veut que u_mes == GO_SME_UREF quand I=0
    corr_v_offset_u = GO_SME_UREF - u_u;
    corr_v_offset_v = GO_SME_UREF - u_v;
}


void input_analog_request_calibrate(void)
{
    calib_pending = 1;
    calib_tick = HAL_GetTick();
}

void input_analog_task(void)
{
    if (!calib_pending) return;
    if ((HAL_GetTick() - calib_tick) < 10U) return;

    input_analog_calibrate_zero_current();
    calib_pending = 0;
}

float input_analog_get_bus_current(void)
{
    float iu = 0.0f, iv = 0.0f;
    input_analog_get_currents(&iu, &iv);
    return iu; // ou moyenne, ou bus si tu l’as
}



void input_analog_get_raw(uint16_t *ch0, uint16_t *ch1)
{
    if (ch0) *ch0 = adc_dma_buffer[0];
    if (ch1) *ch1 = adc_dma_buffer[1];
}

void input_analog_get_currents_ma(int32_t *iu_ma, int32_t *iv_ma)
{
    float iu = 0.0f, iv = 0.0f;
    input_analog_get_currents(&iu, &iv);

    if (iu_ma) *iu_ma = (int32_t)(iu * 1000.0f);
    if (iv_ma) *iv_ma = (int32_t)(iv * 1000.0f);
}



void input_analog_get_offsets(float *off_u, float *off_v)
{
    if (off_u) *off_u = corr_v_offset_u;
    if (off_v) *off_v = corr_v_offset_v;
}

void input_analog_get_offsets_mv(int32_t *off_u_mv, int32_t *off_v_mv)
{
    if (off_u_mv) *off_u_mv = (int32_t)(corr_v_offset_u * 1000.0f);
    if (off_v_mv) *off_v_mv = (int32_t)(corr_v_offset_v * 1000.0f);
}

int input_analog_calibrate_zero_current_avg(uint16_t n)
{
    if (n == 0) return -1;

    // Check ADC is running
    uint16_t ch0 = adc_dma_buffer[0];
    uint16_t ch1 = adc_dma_buffer[1];
    if (ch0 == 0 && ch1 == 0)
    {
        return -2; // ADC not running / no trigger
    }

    uint32_t acc_u = 0;
    uint32_t acc_v = 0;

    for (uint16_t k = 0; k < n; k++)
    {
        acc_u += adc_dma_buffer[0];
        acc_v += adc_dma_buffer[1];
    }

    uint16_t mean_u = (uint16_t)(acc_u / n);
    uint16_t mean_v = (uint16_t)(acc_v / n);

    float u_u = ((float)mean_u * ADC_VREF) / ADC_MAX;
    float u_v = ((float)mean_v * ADC_VREF) / ADC_MAX;

    corr_v_offset_u = GO_SME_UREF - u_u;
    corr_v_offset_v = GO_SME_UREF - u_v;

    return 0;
}






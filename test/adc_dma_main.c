#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "ff.h"
#include "tf_card.h"
#include <string.h>
#include "hardware/spi.h"
#include <stdio.h>

#define PWM_PIN 22
#define PWM_FREQ 1000  // 1 kHz

void _pwm_init(void) {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(PWM_PIN);
    uint channel = pwm_gpio_to_channel(PWM_PIN);

    // 125 MHz / (125 * 1000) = 1 kHz
    pwm_set_clkdiv(slice, 125.0f);
    pwm_set_wrap(slice, 999);

    pwm_set_chan_level(slice, channel, 250); // 25% duty cycle
    pwm_set_enabled(slice, true);
}

#define ADC_PIN 26          // ADC0
#define SAMPLE_RATE 4000    // 4 kHz
#define BUF_SIZE 1024       // samples

uint16_t adc_buf[BUF_SIZE];
int dma_chan;

volatile bool buffer_full = false;

void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan;  // clear IRQ

    // Buffer adc_buf[] is now full
    buffer_full = true;              // signal main loop
    
}

int main() {
    stdio_init_all();
    _pwm_init();
    // --- ADC setup ---
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(0);

    adc_fifo_setup(
        true,    // enable FIFO
        true,    // enable DMA request
        1,       // DREQ when >= 1 sample
        false,
        false
    );

    // ADC clock: 48 MHz / clkdiv = sample rate
    float clkdiv = 48000000.0f / SAMPLE_RATE;
    adc_set_clkdiv(clkdiv);

    // --- DMA setup ---
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(
        dma_chan,
        &cfg,
        adc_buf,          // dst
        &adc_hw->fifo,    // src
        BUF_SIZE,
        false
    );

    // DMA IRQ
    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Start
    adc_run(true);
    dma_start_channel_mask(1u << dma_chan);

    while (1) {
        if (buffer_full) {
            buffer_full = false;

            printf("Buffer full, first 5 samples:\n");
            for (int i = 0; i < 5; i++) {
                printf("%u ", adc_buf[i]);
            }
            printf("\n");

            // Restart DMA for next buffer
            dma_channel_set_read_addr(dma_chan, &adc_hw->fifo, false);
            dma_channel_set_write_addr(dma_chan, adc_buf, false);
            dma_channel_set_trans_count(dma_chan, BUF_SIZE, true);
        }
    }
}

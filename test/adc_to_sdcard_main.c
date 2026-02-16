#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "ff.h"
#include "tf_card.h"
#include <string.h>
#include "hardware/spi.h"

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

#define SPI_MISO_PIN 12
#define SPI_CS_PIN   13 
#define SPI_SCK_PIN  10
#define SPI_MOSI_PIN 11

#define CLK_SLOW (2*1000 * 1000)
#define CLK_FAST (4 * 1000 * 1000)

FATFS fs;
FIL fil;

void init_sd_card(){
    printf("SD Init..\n");

    /* SPI configuration */
    pico_fatfs_spi_config_t config = {
        spi1,                  /* SPI instance */
        CLK_SLOW,      /* slow clock */
        CLK_FAST,      /* fast clock */  
        SPI_MISO_PIN, /* MISO */
        SPI_CS_PIN,   /* CS */
        SPI_SCK_PIN,  /* SCK */
        SPI_MOSI_PIN, /* MOSI */
        true                   /* use built-in pullups */
    };

    if (!pico_fatfs_set_config(&config)) {
        printf("Failed to set config\n");
        while (1) sleep_ms(5);
    }

    FRESULT fr  = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("f_mount fail: %d\n", fr);
        while (1) sleep_ms(5);
    }
}

void _create_hello_world_file(){
    FIL fil;
    FRESULT fr = f_open(&fil, "hello.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("f_open fail: %d\n", fr);
        while (1) sleep_ms(5);
    }

    const char *text = "Hello from pico_fatfs!\r\n";
    UINT byte_written;
    fr = f_write(&fil, text, strlen(text), &byte_written);
    if (fr != FR_OK) {
        printf("f_write fail: %d\n", fr);
    } else {
        printf("Wrote %u bytes\n", byte_written);
    }

    f_close(&fil);

    printf("Done\n");
}

#define ADC_PIN 26          // ADC0
#define SAMPLE_RATE 4000    // 4 kHz
#define BUF_SIZE 1024       // samples

uint16_t adc_buf1[BUF_SIZE];
uint16_t adc_buf2[BUF_SIZE];

volatile uint16_t *active_buf;     // DMA writes here
volatile uint16_t *sd_buf;         // ready for SD write
volatile bool sd_write_pending = false;

int dma_chan;
uint byte_written;

void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan;  // clear IRQ

    // Buffer just completed
    sd_buf = active_buf;
    sd_write_pending = true;

    // Swap buffer
    active_buf = (active_buf == adc_buf1) ? adc_buf2 : adc_buf1;

    // Restart DMA immediately
    dma_channel_set_write_addr(dma_chan, active_buf, false);
    dma_channel_set_trans_count(dma_chan, BUF_SIZE, true);
}

int64_t free_time_us[64];
int64_t spi_time_us[64];
absolute_time_t start_loop_us;
uint32_t time_diff_index = 0;

int main() {
    stdio_init_all();

    // for testing
    _pwm_init();
    
    gpio_init(27);
    gpio_set_dir(27, GPIO_OUT);
    gpio_put(27, 1);
    

    init_sd_card();
    FRESULT fr = f_open(&fil, "adc_log.bin", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("f_open fail: %d\n", fr);
        while (1) sleep_ms(5);
    }
    printf("SD Card ready, starting ADC logging...\n");
    // --- ADC setup ---
    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(0);

    adc_fifo_setup(
        true,
        true,
        1,
        false,
        false
    );

    float clkdiv = 48000000.0f / SAMPLE_RATE;
    adc_set_clkdiv(clkdiv);

    // --- DMA setup ---
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);

    active_buf = adc_buf1;

    dma_channel_configure(
        dma_chan,
        &cfg,
        active_buf,
        &adc_hw->fifo,
        BUF_SIZE,
        false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    adc_run(true);
    dma_start_channel_mask(1u << dma_chan);

    printf("DMA started, loxgging ADC data to SD card...\n");
    printf("Logging for 5 seconds...\n");
    absolute_time_t start_time = get_absolute_time();


    start_loop_us = get_absolute_time();
    // --- MAIN LOOP ---
    while (absolute_time_diff_us(start_time, get_absolute_time()) < 5 * 1000 * 1000) {

        if (sd_write_pending) {
            free_time_us[time_diff_index] = absolute_time_diff_us(start_loop_us, get_absolute_time()); 
            start_loop_us = get_absolute_time();
            sd_write_pending = false;

            // SAFE: DMA is not touching sd_buf
            f_write(&fil, (const void *)sd_buf, BUF_SIZE * sizeof(uint16_t), &byte_written);

            // optional debug
            // printf("SD wrote buffer, first = %u\n", sd_buf[0]);

            spi_time_us[time_diff_index] = absolute_time_diff_us(start_loop_us, get_absolute_time());
            start_loop_us = get_absolute_time();
            time_diff_index++;
        }
    }

    adc_run(false);
    
    f_sync(&fil);
    f_close(&fil);
    printf("Stopping...\n");
    dma_channel_abort(dma_chan);
    while(1){ sleep_ms(1000);  // keep running
    }
}

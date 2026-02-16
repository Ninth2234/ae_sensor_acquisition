#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"

#include <stdio.h>
#include "stdlib.h"
#include "pico/stdlib.h"
#include "ff.h"
#include "tf_card.h"
#include <string.h>
#include <ctype.h>
#include "hardware/spi.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "u8g2.h"

#define PWM_PIN 22
#define PWM_FREQ 1000  // 1 kHz

// #define SPI_MISO_PIN 12
// #define SPI_CS_PIN   13 
// #define SPI_SCK_PIN  10
// #define SPI_MOSI_PIN 11

#define SPI_PORT spi1

#define BTN_ENC_PIN 8
#define BTN_RST_PIN 9

// #define LCD_D4_PIN 5
// #define LCD_D5_PIN 2
// #define LCD_RS_PIN 4
// #define LCD_EN_PIN 9

// pi interface board
#define SPI_MISO_PIN 12
#define SPI_CS_PIN   16 
#define SPI_SCK_PIN  14
#define SPI_MOSI_PIN 15

#define LCD_D4_PIN 20
#define LCD_D5_PIN 21
#define LCD_RS_PIN 22
#define LCD_EN_PIN 13

uint8_t u8x8_byte_pico_hw_spi(u8x8_t *u8x8,
                             uint8_t msg,
                             uint8_t arg_int,
                             void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:

        spi_init(SPI_PORT, 5 * 1000 * 1000);   // 5 MHz (match MicroPython)

        // *** THIS IS THE KEY FIX ***
        spi_set_format(SPI_PORT,
                    8,
                    SPI_CPOL_1,   // polarity=1
                    SPI_CPHA_1,   // phase=1
                    SPI_MSB_FIRST);

        gpio_set_function(SPI_SCK_PIN,  GPIO_FUNC_SPI);
        gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);

        // Chip Select (manual)
        gpio_init(LCD_EN_PIN);
        gpio_set_dir(LCD_EN_PIN, GPIO_OUT);
        gpio_put(LCD_EN_PIN, 1);

        break;

    case U8X8_MSG_BYTE_SEND:
        spi_write_blocking(SPI_PORT, (uint8_t *)arg_ptr, arg_int);
        break;

    case U8X8_MSG_BYTE_SET_DC:
        gpio_put(LCD_RS_PIN, arg_int);   // A0 from MicroPython
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        gpio_put(LCD_EN_PIN, 0);         // CS LOW
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        gpio_put(LCD_EN_PIN, 1);         // CS HIGH
        break;


    default:
        return 0;
    }
    return 1;
}


uint8_t u8x8_gpio_and_delay_pico(u8x8_t *u8x8,
                                uint8_t msg,
                                uint8_t arg_int,
                                void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:

        gpio_init(LCD_RS_PIN);   // A0
        gpio_set_dir(LCD_RS_PIN, GPIO_OUT);

        gpio_init(LCD_D4_PIN);   // RESET
        gpio_set_dir(LCD_D4_PIN, GPIO_OUT);

        gpio_init(LCD_EN_PIN);   // CS
        gpio_set_dir(LCD_EN_PIN, GPIO_OUT);
        gpio_put(LCD_EN_PIN, 1);

        break;

    case U8X8_MSG_DELAY_MILLI:
        sleep_ms(arg_int);
        break;

    case U8X8_MSG_GPIO_RESET:
        gpio_put(LCD_D4_PIN, arg_int);   // reset line
        break;
    }
    return 1;
}


#define CLK_SLOW (2* 1000 * 1000)
#define CLK_FAST (4 * 1000 * 1000)

#define LCD_W 128
#define LCD_H 64

#define COLUMN_TIME_US 10000   // time spent sampling for one column (5 ms)
#define ADC_BLOCK 32          // samples returned by adc_capture_frame()


FATFS fs;
FIL fil;

static void sd_spi_recovery(void)
{
    // 1. Make sure SPI is NOT driving the pins yet
    spi_deinit(spi1);

    // 2. Configure pins as GPIO (manual control)
    gpio_init(SPI_MOSI_PIN);
    gpio_init(SPI_MISO_PIN);
    gpio_init(SPI_SCK_PIN);
    gpio_init(SPI_CS_PIN);

    gpio_set_dir(SPI_MOSI_PIN, GPIO_OUT);
    gpio_set_dir(SPI_SCK_PIN,  GPIO_OUT);
    gpio_set_dir(SPI_CS_PIN,   GPIO_OUT);
    gpio_set_dir(SPI_MISO_PIN, GPIO_IN);

    // 3. Force Idle Bus State (required by SD spec)
    gpio_put(SPI_CS_PIN, 1);     // deselect card
    gpio_put(SPI_MOSI_PIN, 1);   // MOSI must be HIGH

    sleep_ms(1);

    // 4. Send >=74 clock pulses (we send 80)
    for (int i = 0; i < 80; i++) {
        gpio_put(SPI_SCK_PIN, 1);
        sleep_us(2);
        gpio_put(SPI_SCK_PIN, 0);
        sleep_us(2);
    }

    sleep_ms(2); // give card time to exit partial command
}

void init_sd_card(){
    printf("SD Init..\n");

    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    sd_spi_recovery();
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


PIO pio = pio0;
int sm = 0;

void neopixel_init(uint32_t pin){
    // Load the PIO program
    uint offset = pio_add_program(pio, &ws2812_program);

    // 👇 Use your LCD pin here
    ws2812_program_init(pio, sm, offset, pin, 800000, false);
}

void neopixel_write(uint32_t* colors, size_t count) {
    
    for (size_t i = 0; i < count; i++) {
        pio_sm_put_blocking(pio, sm, colors[i]);
    }
    sleep_ms(60);
}

u8g2_t u8g2;

void set_spi_mode_sdcard(){
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}

void set_spi_mode_lcd(){
    spi_set_format(SPI_PORT,8,SPI_CPOL_1,SPI_CPHA_1,SPI_MSB_FIRST);
}  

void adc_init_sdcard_logging(){
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
}

void adc_deinit_sdcard_logging(){
    adc_run(false);        // stop conversion (important)
    adc_fifo_drain();      // clear old samples (important)
    // adc_set_clkdiv(newdiv);
}

void adc_capture_frame(uint16_t *buf)
{
    for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = adc_read();    // 12-bit result (0–4095)
        sleep_us(250);  // wait for next sample
    }
}

void lcd_plot(uint16_t *buf)
{
    u8g2_ClearBuffer(&u8g2);

    for (int x = 0; x < 128; x++)
    {
        // scale 0–4095 → 0–63
        int y = 63 - (buf[x] * 63 / 4095);

        u8g2_DrawPixel(&u8g2, x, y);
    }

    u8g2_SendBuffer(&u8g2);
}

void adc_init_polling(void)
{
    adc_init();
    adc_gpio_init(26);          // GPIO26 = ADC0 (change if needed)
    adc_select_input(0);        // ADC channel 0
}

void _dma_init(){
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
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}





FRESULT open_new_log(FIL *fp, char *out_name, size_t name_len)
{
    DIR dir;
    FILINFO fno;
    uint32_t max_index = 0;
    FRESULT fr;

    /* Scan root directory */
    fr = f_opendir(&dir, "/");
    if (fr != FR_OK)
        return fr;

    while (1)
    {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0)
            break;

        /* Skip directories */
        if (fno.fattrib & AM_DIR)
            continue;

        /* Expect exactly: aXXXX.bin (9 chars) */
        if (strlen(fno.fname) != 9)
            continue;

        if (fno.fname[0] != 'a')
            continue;

        if (strcmp(&fno.fname[5], ".bin") != 0)
            continue;

        /* Check digits */
        int valid = 1;
        for (int i = 1; i < 5; i++)
        {
            if (!isdigit((int)fno.fname[i]))
            {
                valid = 0;
                break;
            }
        }
        if (!valid)
            continue;

        uint32_t idx = atoi(&fno.fname[1]);
        if (idx > max_index)
            max_index = idx;
    }

    f_closedir(&dir);

    /* Next index */
    uint32_t next = max_index + 1;
    if (next > 9999)
        next = 9999;

    /* Build filename */
    snprintf(out_name, name_len, "a%04lu.bin", next);

    /* Open new file (fail if already exists = safety) */
    fr = f_open(fp, out_name, FA_WRITE | FA_CREATE_NEW);
    return fr;
}

void lcd_show_logging(const char *fname)
{
    
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_6x12_tf);

    u8g2_DrawStr(&u8g2, 0, 12, "SD Logging Started:");

    // draw filename (second line)
    u8g2_DrawStr(&u8g2, 0, 28, fname);

    u8g2_DrawStr(&u8g2, 0, 48, "Recording...");

    u8g2_SendBuffer(&u8g2);  // push to screen
}

char filename[64];
void task_sdcard_adc_loggin() {
    
    set_spi_mode_sdcard();

    // _create_hello_world_file();

    FRESULT fr = open_new_log(&fil, filename, sizeof(filename));

    if(fr != FR_OK) {
        printf("Failed to open file: %d\n", fr);
        return;
    }

    set_spi_mode_lcd();
    lcd_show_logging(filename);
    set_spi_mode_sdcard();


    adc_init_sdcard_logging();

    _dma_init();
    adc_run(true);
    dma_start_channel_mask(1u << dma_chan);


    printf("DMA started, loxgging ADC data to SD card...\n");
    printf("Logging to file: %s\n", filename);
    printf("Logging for 5 seconds...\n");

    absolute_time_t start_time = get_absolute_time();

    // --- MAIN LOOP ---
    while (absolute_time_diff_us(start_time, get_absolute_time()) < 5 * 1000 * 1000) {

        if (sd_write_pending) {
           
            sd_write_pending = false;
            f_write(&fil, (const void *)sd_buf, BUF_SIZE * sizeof(uint16_t), &byte_written);
            // printf("SD wrote buffer, first = %u\n", sd_buf[0]);
        }
    }
    printf("Stopping...\n");

    f_sync(&fil);
    f_close(&fil);
    
    
    // ---- STEP 1: Stop ADC generating NEW samples ----
    adc_run(false);

    sleep_us(5);  // allow last sample to land in FIFO

    // ---- STEP 2: Drain FIFO manually (THIS IS THE KEY) ----
    while (!adc_fifo_is_empty()) {
        (void)adc_fifo_get();
    }

    // ---- STEP 3: Now disable FIFO + DREQ ----
    adc_fifo_setup(false, false, 0, false, false);

    sleep_us(5);

    // ---- STEP 4: Disable DMA channel gracefully ----
    dma_channel_set_irq0_enabled(dma_chan, false);
    // Disable channel immediately
    dma_hw->ch[dma_chan].ctrl_trig = 0;

    // Reset the DMA block for this channel
    reset_block(RESETS_RESET_DMA_BITS);
    unreset_block_wait(RESETS_RESET_DMA_BITS);

    // ---- STEP 7: Clear any latched interrupt ----
    dma_hw->ints0 = 1u << dma_chan;

    printf("Done logging to SD card.\n");
    printf("Return to default SPI mode for LCD...\n");
    // dma_channel_set_enabled(dma_chan, false);
}


uint8_t x = 0;   // current column
void task_lcd_plotting(){
    uint32_t t0 = time_us_32();

    uint16_t min_v = 0xFFFF;
    uint16_t max_v = 0;

    // ---- Capture for fixed time window ----
    while ((time_us_32() - t0) < COLUMN_TIME_US)
    {
        
        uint16_t v = adc_read();

        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        
    }

    // ---- Convert ADC values to LCD Y coordinates ----
    uint8_t y_min = LCD_H - 1 - (min_v * LCD_H / 4096);
    uint8_t y_max = LCD_H - 1 - (max_v * LCD_H / 4096);

    if (y_min > y_max) {
        uint8_t t = y_min;
        y_min = y_max;
        y_max = t;
    }

    // ---- Erase previous column (for overwrite when wrapping) ----
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawVLine(&u8g2, x, 0, LCD_H);

    // ---- Draw new min/max bar ----
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawVLine(&u8g2, x, y_min, y_max - y_min + 1);

    // ---- Draw cursor at next column ----
    uint8_t cursor_x = (x + 1) % LCD_W;
    u8g2_DrawVLine(&u8g2, cursor_x, 0, LCD_H);

    // ---- Send to LCD ----
    u8g2_SendBuffer(&u8g2);

    // ---- Advance column ----
    x++;
    if (x >= LCD_W) x = 0;
}


int main() {
    stdio_init_all();
    printf("starting...\n");
    gpio_init(BTN_ENC_PIN);
    gpio_set_dir(BTN_ENC_PIN, GPIO_IN);

    neopixel_init(LCD_D5_PIN);
    uint32_t colors[2] = {
        0x3F000000, // Red
        0  // Green
    };
    neopixel_write(colors, 2);



    

    printf("init sdcard\n");
    init_sd_card();
    printf("finish init sdcard\n");

    

    

    
    
    colors[0] = 0x3F3F3F00;
    neopixel_write(colors, 2);

    u8g2_Setup_st7567_jlx12864_f(
        &u8g2,
        U8G2_R2,
        u8x8_byte_pico_hw_spi,
        u8x8_gpio_and_delay_pico
    );

    u8g2_InitDisplay(&u8g2);
    sleep_ms(50);

    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetContrast(&u8g2, 128);

    u8x8_cad_StartTransfer(&u8g2.u8x8);
    u8x8_cad_SendCmd(&u8g2.u8x8, 0x81);   // EV command
    u8x8_cad_SendCmd(&u8g2.u8x8, 0x2F);   // your working value
    u8x8_cad_EndTransfer(&u8g2.u8x8);

    set_spi_mode_sdcard();
    set_spi_mode_lcd();
    adc_init_polling();

    while (1)
    {
        if (gpio_get(BTN_ENC_PIN) == 0) {
            task_sdcard_adc_loggin();
            sleep_ms(500);
            adc_init_polling();
            set_spi_mode_lcd();

            u8g2_ClearBuffer(&u8g2);
            u8g2_SetDrawColor(&u8g2, 1);
            u8g2_SendBuffer(&u8g2);
        }
        task_lcd_plotting();
    }
}


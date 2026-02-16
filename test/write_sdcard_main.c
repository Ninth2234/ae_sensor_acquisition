#include <stdio.h>
#include "pico/stdlib.h"
#include "ff.h"
#include "tf_card.h"
#include <string.h>
#include "hardware/spi.h"


#define SPI_MISO_PIN 12
#define SPI_CS_PIN   13 
#define SPI_SCK_PIN  10
#define SPI_MOSI_PIN 11

#define CLK_SLOW (1000 * 1000)
#define CLK_FAST (4 * 1000 * 1000)



FATFS fs;

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

int main(void) {
    stdio_init_all();
    sleep_ms(1000);
    
    // init_sd_card();
    // _create_hello_world_file();


    while (1) sleep_ms(5);
}

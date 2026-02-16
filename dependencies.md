# Dependencies

This project relies on external libraries that are not included in this repository.

## Required Libraries

### pico_fatfs
- Repository: https://github.com/elehobica/pico_fatfs
- Used for: FAT filesystem access on SD card via SPI.

Clone into:
lib/pico_fatfs

---

### u8g2
- Repository: https://github.com/olikraus/u8g2
- Used for: MINI12864 display graphics and text rendering.

Clone into:
lib/u8g2

---

## Setup

After cloning this project:

```bash
cd lib
git clone https://github.com/elehobica/pico_fatfs.git
git clone https://github.com/olikraus/u8g2.git

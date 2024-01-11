#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_partition.h>

#include "espeon.h"
#include "interrupt.h"
#include "mbc.h"
#include "rom.h"

#define PARTITION_ROM (esp_partition_subtype_t(0x40))
#define MAX_ROM_SIZE (2*1024*1024)

#define JOYPAD_INPUT 5
#define JOYPAD_ADDR  0x88

#define GETBIT(x, b) (((x)>>(b)) & 0x01)

#define GAMEBOY_WIDTH 160
#define GAMEBOY_HEIGHT 144

#define CENTER_X ((240 - GAMEBOY_WIDTH)  >> 1)
#define CENTER_Y ((144 - GAMEBOY_HEIGHT) >> 1)

static fbuffer_t* pixels;

volatile int spi_lock = 0;
volatile bool sram_modified = false;

uint16_t palette[] = { 0xFFFF, 0xAAAA, 0x5555, 0x2222 };

static void espeon_request_sd_write()
{
	spi_lock = 1;
}

void espeon_init(void)
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    M5Cardputer.Display.setColorDepth(8);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setBrightness(50);
    

	pixels = (fbuffer_t*)calloc(GAMEBOY_HEIGHT * GAMEBOY_WIDTH, sizeof(fbuffer_t));

	const uint32_t pal[] = {0xe7eaec, 0xa2adb3, 0x5D797A, 0x183442}; // Default greenscale palette
	espeon_set_palette(pal);

}

#define digitalWrite(pin, level)  gpio_set_level((gpio_num_t)pin, level)
#define digitalRead(pin)          (!gpio_get_level((gpio_num_t)pin))

void espeon_update(void)
{
    btn_directions = 0x00;
    btn_faces = 0x00;
  
  // M5Cardputer.Display.setCursor(0, 0);

  // M5Cardputer.Display.printf("L: 13,15,3,4,5,6,7\n");
  // for (uint8_t i = 0; i < 8; i++) {
  //       uint8_t output = i & 0B00000111;

  //       digitalWrite(8, (output & 0B00000001));
  //       digitalWrite(9, (output & 0B00000010));
  //       digitalWrite(11, (output & 0B00000100));
  //       M5Cardputer.Display.printf("%d: %d ,%d ,%d,%d,%d,%d,%d\n", i, 
  //       digitalRead(13), digitalRead(15), digitalRead(3),
  //       digitalRead(4), digitalRead(5), digitalRead(6),digitalRead(7));
  // }

  // left: 4,13
  // right 4,15
  // up: 1,13
  // down 0,13
  // A: 4,7
  // B: 0,7
  // start: 1,7
  // sel: 0,6

  //0 -> down:13, sel: 6, B: 7
  digitalWrite(11, 0); digitalWrite(9, 0); digitalWrite(8, 0);
  btn_directions |= digitalRead(13) <<3;
  btn_faces |= digitalRead(6)<< 2 | digitalRead(7) << 1;

  //1 -> start:7, up: 13 
  digitalWrite(8, 1);
  btn_directions |= digitalRead(13) <<2;
  btn_faces |= digitalRead(7) <<3;

  //4 -> A:7, left: 13, right: 15
  digitalWrite(11, 1); digitalWrite(8, 0);
  btn_directions |= digitalRead(13) <<1 | digitalRead(15);
  btn_faces |= digitalRead(7);

  uint8_t key_pressed = btn_directions | btn_faces;
  btn_directions = ~btn_directions;
  btn_faces = ~btn_faces;

  if (key_pressed)
    interrupt(INTR_JOYPAD);

}

void espeon_faint(const char* msg)
{
	M5Cardputer.Display.clear();
	M5Cardputer.Display.setCursor(2, 2);
	M5Cardputer.Display.printf("Espeon fainted!\n%s", msg);
	while(true);
}

fbuffer_t* espeon_get_framebuffer(void)
{
	return pixels;
}

void espeon_clear_framebuffer(fbuffer_t col)
{
	memset(pixels, col, sizeof(pixels));
}

void espeon_clear_screen(uint16_t col)
{
	M5Cardputer.Display.fillScreen(col);
}

void espeon_set_palette(const uint32_t* col)
{
	/* RGB888 -> RGB565 */
	for (int i = 0; i < 4; ++i) {
		palette[i] = ((col[i]&0xFF)>>3)+((((col[i]>>8)&0xFF)>>2)<<5)+((((col[i]>>16)&0xFF)>>3)<<11);
	}
}

void espeon_end_frame(void)
{
	if (spi_lock) {
		const s_rominfo* rominfo = rom_get_info();
		if (rominfo->has_battery && rom_get_ram_size())
			espeon_save_sram(mbc_get_ram(), rom_get_ram_size());
		spi_lock = 0;
	}
	M5Cardputer.Display.pushImage(CENTER_X, CENTER_Y, GAMEBOY_WIDTH, GAMEBOY_HEIGHT, pixels);
}

void espeon_save_sram(uint8_t* ram, uint32_t size)
{
//	if (!ram) return;
//
//	static char path[20];
//	sprintf(path, "/%.8s.bin", rom_get_title());
//
//	File sram = SD.open(path, FILE_WRITE);
//	if (sram) {
//		sram.seek(0);
//		sram.write(ram, size);
//		sram.close();
//	}
}

void espeon_load_sram(uint8_t* ram, uint32_t size)
{
//	if (!ram) return;
//
//	static char path[20];
//	sprintf(path, "/%.8s.bin", rom_get_title());
//
//	File sram = SD.open(path, FILE_READ);
//	if (sram) {
//		sram.seek(0);
//		sram.read(ram, size);
//		sram.close();
//	}
}

//const uint8_t* espeon_load_bootrom(const char* path)
//{
//	static uint8_t bootrom[256];
//
//	File bf = SD.open(path, FILE_READ);
//	if (bf) {
//		bf.seek(0);
//		bf.read(bootrom, sizeof(bootrom));
//		bf.close();
//		return bootrom;
//	}
//
//	return nullptr;
//}

static inline const uint8_t* espeon_get_last_rom(const esp_partition_t* part)
{
	spi_flash_mmap_handle_t hrom;
	const uint8_t* romdata;
	esp_err_t err;
	err = esp_partition_mmap(part, 0, MAX_ROM_SIZE, SPI_FLASH_MMAP_DATA, (const void**)&romdata, &hrom);
	if (err != ESP_OK)
		return nullptr;
	return romdata;
}

//const uint8_t* espeon_load_rom(const char* path)
//{
//	const esp_partition_t* part;
//	part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, PARTITION_ROM, NULL);
//	if (!part)
//		return nullptr;
//
//	if (!path)
//		return espeon_get_last_rom(part);
//
//	File romfile = SD.open(path, FILE_READ);
//	if (!romfile)
//		return nullptr;
//
//	esp_err_t err;
//	err = esp_partition_erase_range(part, 0, MAX_ROM_SIZE);
//	if (err != ESP_OK)
//		return nullptr;
//
//	const size_t bufsize = 32 * 1024;
//	size_t romsize = romfile.size();
//	if (romsize > MAX_ROM_SIZE)
//		return nullptr;
//
//	uint8_t* rombuf = (uint8_t*)calloc(bufsize, 1);
//	if (!rombuf)
//		return nullptr;
//
//	M5Cardputer.Display.clear();
//	M5Cardputer.Display.setTextColor(TFT_WHITE);
//	M5Cardputer.Display.drawString("Flashing ROM...", 0, 0);
//	size_t offset = 0;
//	while(romfile.available()) {
//		romfile.read(rombuf, bufsize);
//		esp_partition_write(part, offset, (const void*)rombuf, bufsize);
//		offset += bufsize;
//		M5Cardputer.Display.progressBar(50, 100, 200, 40, (offset*100)/romsize);
//	}
//	M5Cardputer.Display.clear();
//	free(rombuf);
//	romfile.close();
//
//	return espeon_get_last_rom(part);
//}

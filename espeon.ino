#include "timer.h"
#include "rom.h"
#include "mem.h"
#include "cpu.h"
#include "lcd.h"
#include "espeon.h"

#include "gbfiles.h"

void setup()
{
	espeon_init();
    const uint8_t* rom = (const uint8_t*)gb_rom;
    const uint8_t* bootrom = (const uint8_t*)gb_bios;
	
	if (!rom_init(rom))
		espeon_faint("rom_init failed.");

	if (!mmu_init(bootrom))
		espeon_faint("mmu_init failed.");

	if (!lcd_init())
		espeon_faint("lcd_init failed.");

	cpu_init();

	// while(true) {
	// 	espeon_update();
	// }
}

void loop()
{
	uint32_t cycles = cpu_cycle();
  espeon_update();
	lcd_cycle(cycles);
	timer_cycle(cycles);
}

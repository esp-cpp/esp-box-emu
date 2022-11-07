#include "gnuboy.h"
#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "cpu.h"
#include "sound.h"
#include "mem.h"
#include "lcd.h"
#include "rtc.h"
#include "rc.h"
#include "fb.h"

#include "spi_lcd.h"

static int framelen = 16743;
static int framecount;


void emu_init()
{

}


/*
 * emu_reset is called to initialize the state of the emulated
 * system. It should set cpu registers, hardware registers, etc. to
 * their appropriate values at powerup time.
 */

void emu_reset()
{
	hw_reset();
	lcd_reset();
	cpu_reset();
	mbc_reset();
	sound_reset();
}




/* emu_step()
	make CPU catch up with LCDC
*/
void emu_step()
{
	cpu_emulate(cpu.lcdc);
}


/*
	Time intervals throughout the code, unless otherwise noted, are
	specified in double-speed machine cycles (2MHz), each unit
	roughly corresponds to 0.477us.

	For CPU each cycle takes 2dsc (0.954us) in single-speed mode
	and 1dsc (0.477us) in double speed mode.

	Although hardware gbc LCDC would operate at completely different
	and fixed frequency, for emulation purposes timings for it are
	also specified in double-speed cycles.

	line = 228 dsc (109us)
	frame (154 lines) = 35112 dsc (16.7ms)
	of which
		visible lines x144 = 32832 dsc (15.66ms)
		vblank lines x10 = 2280 dsc (1.08ms)
*/
void emu_run()
{
	// FIXME: how to handle timing?
	// void *timer = sys_timer();
	int delay;

	// FIXME: what does vid do?
	// vid_begin();
	// for (;;)
	{
		/* FRAME BEGIN */

		/* FIXME: djudging by the time specified this was intended
		to emulate through vblank phase which is handled at the
		end of the loop. */
		cpu_emulate(2280);

		gb_lcd_begin();

		/* FIXME: R_LY >= 0; comparsion to zero can also be removed
		altogether, R_LY is always 0 at this point */
		while (R_LY > 0 && R_LY < 144)
		{
			/* Step through visible line scanning phase */
			emu_step();
		}
		// static size_t frame_num=0;
		// printf("frame: %d\n", frame_num++);
		// lcd_write_frame(0, 0, 160, 144, fb.ptr);
		/* VBLANK BEGIN */

		// FIXME: what does this do?
		// vid_end();
		rtc_tick();
		sound_mix();
		/* pcm_submit() introduces delay, if it fails we use
		sys_sleep() instead */
		// FIXME: what does this do...
		// if (!pcm_submit())
		{
			/* FIXME: need to replace this with waits?
			delay = framelen - sys_elapsed(timer);
			sys_sleep(delay);
			sys_elapsed(timer);
			*/
		}
		// FIXME: what does this function do?
		// doevents();
		// vid_begin();
		if (framecount) { if (!--framecount) die("finished\n"); }

		if (!(R_LCDC & 0x80)) {
			/* LCDC operation stopped */
			/* FIXME: djudging by the time specified, this is
			intended to emulate through visible line scanning
			phase, even though we are already at vblank here */
			cpu_emulate(32832);
		}

		while (R_LY > 0) {
			/* Step through vblank phase */
			emu_step();
		}
		/* VBLANK END */
		/* FRAME END */
	}
}

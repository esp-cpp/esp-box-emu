void sio_output_start(void)
{
}

void sio_output_stop(void)
{
}

void init_sio(void)
{
}

void sio_print(void)
{
}

void sio_printf(void)
{
}

int sio_getchar2(void)
{
#ifdef MGS_ESPBOX
    /* The mts idle task polls this in a tight loop and would hold core 0 at
     * the game's priority for as long as every game thread is waiting on the
     * vblank; the emulator menu, gamepad and idle tasks share that core.
     * Blocking a tick here hands it to them; the vblank tick wakes a game
     * thread by suspending this task, which works just as well from a delay. */
    extern void Mgs_IdleYield(void);
    Mgs_IdleYield();
#endif
    return -1; /* EOF */
}

int getchar(void)
{
    return 0;
}

void sio_putchar(void)
{
}

void sio_getchar(void)
{
}

void sio_puts(void)
{
}

void sio_setup(void)
{
}

void sio_dump(void)
{
}

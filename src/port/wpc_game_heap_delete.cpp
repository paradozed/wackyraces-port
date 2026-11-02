/*
 * wpc_game_heap_delete.cpp - C++ stubs for game bank and heap routines.
 *
 * The original game bank (src/game_winmain.cpp, src/game_heap_delete.cpp)
 * uses i386-specific instruction matching and Win32 APIs. These are replaced
 * with minimal stubs.
 */

extern "C" {

int wpc_game_initialize_bank(void)
{
    /* No-op: native port does not yet have a game bank. */
    return 1;
}

void wpc_game_shutdown_bank(void)
{
    /* No-op. */
}

void wpc_game_tick_bank(void)
{
    /* No-op: one frame of game logic. */
}

void wpc_game_heap_delete(void *ptr)
{
    (void)ptr;
    /* No-op: native port uses C++ delete / free instead. */
}

} /* extern "C" */

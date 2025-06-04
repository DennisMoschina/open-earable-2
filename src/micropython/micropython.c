#include "micropython.h"

#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(micropython, 3);

void real_main();
void mp_console_init();

void init_mp() {
    #ifdef CONFIG_CONSOLE_SUBSYS
    mp_console_init();
    #else
    zephyr_getchar_init();
    #endif

    real_main();
    LOG_INF("MicroPython initialized");
}
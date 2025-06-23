#include "micropython.h"

#include <zephyr/kernel.h>

#include "zephyr/logging/log.h"
LOG_MODULE_REGISTER(micropython, 4);

#define MICROPYTHON_THREAD_STACK_SIZE (16 * 1024) // 16 KB stack size
#define MICROPYTHON_THREAD_PRIORITY 10

K_THREAD_STACK_DEFINE(micropython_thread_stack, MICROPYTHON_THREAD_STACK_SIZE);
static struct k_thread micropython_thread;

void real_main();
void mp_console_init();

static void micropython_thread_fn(void *p1, void *p2, void *p3) {
    // This function is called when the MicroPython thread starts.
    // It should contain the main logic for running MicroPython.
    LOG_DBG("MicroPython thread started");
    
    // Call the real main function of MicroPython
    real_main();
}

/**
 * @brief Initialize MicroPython.
 * @details This function initializes the MicroPython environment
 *         by setting up the console and creating a thread to run
 *         the MicroPython interpreter.
 */
void init_mp() {
    LOG_DBG("Initializing MicroPython...\n");
    #ifdef CONFIG_CONSOLE_SUBSYS
    mp_console_init();
    #else
    zephyr_getchar_init();
    #endif

    k_thread_name_set(&micropython_thread, "micropython");

    k_thread_create(
        &micropython_thread,
        micropython_thread_stack,
        K_THREAD_STACK_SIZEOF(micropython_thread_stack),
        micropython_thread_fn,
        NULL, NULL, NULL,
        MICROPYTHON_THREAD_PRIORITY,
        10,
        K_NO_WAIT
    );
    LOG_DBG("MicroPython initialized\n");
}
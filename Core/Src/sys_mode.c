#include "sys_mode.h"

static volatile SystemMode_t s_mode = MODE_NORMAL;

void sys_mode_init(void)  { s_mode = MODE_NORMAL; }
SystemMode_t sys_mode_get(void) { return s_mode; }
void sys_mode_set(SystemMode_t mode) { s_mode = mode; }

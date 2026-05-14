#ifndef SYS_MODE_H
#define SYS_MODE_H

typedef enum {
    MODE_NORMAL = 0,
    MODE_SIT,
    MODE_SUT,
} SystemMode_t;

void         sys_mode_init(void);
SystemMode_t sys_mode_get(void);
void         sys_mode_set(SystemMode_t mode);

#endif /* SYS_MODE_H */

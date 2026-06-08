#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void run_furmark_start(int which);
void run_furmark_stop(void);
int run_furmark_running(void);

#ifdef __cplusplus
}
#endif

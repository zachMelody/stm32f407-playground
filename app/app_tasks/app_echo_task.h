#ifndef __APP_ECHO_TASK_H__
#define __APP_ECHO_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void App_EchoTask(void *argument);
void App_EchoTask_OnButtonPressed(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ECHO_TASK_H__ */

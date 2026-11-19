/* SceActivityDb's static constructors and destructors, run at its start and stop */
#ifndef ACTIVITY_DB_INIT_H
#define ACTIVITY_DB_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

void activityDbInit(void);
void activityDbTerm(void);

#ifdef __cplusplus
}
#endif

#endif

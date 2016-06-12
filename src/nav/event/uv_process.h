#ifndef NV_EVENT_UV_PROCESS_H
#define NV_EVENT_UV_PROCESS_H

#include <uv.h>
#include "nav/event/process.h"

typedef struct uv_process {
  Process process;
  uv_process_t uv;
  uv_process_options_t uvopts;
  uv_stdio_container_t uvstdio[3];
} UvProcess;

static inline UvProcess uv_process_init(Loop *loop, void *data)
{
  UvProcess rv;
  rv.process = process_init(loop, kProcessTypeUv, data);
  return rv;
}

bool uv_process_spawn(UvProcess *uvproc);
void uv_process_close(UvProcess *uvproc);

#endif

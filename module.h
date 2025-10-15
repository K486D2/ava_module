#ifndef MODULE_H
#define MODULE_H

#include "version.h"

#if defined(__linux__) || defined(_WIN32)
#include "comm/comm.h"
#include "shm/shm.h"
#endif

#include "ctl/ctl.h"
#include "ds/ds.h"
#include "filter/filter.h"
#include "foc/foc.h"
#include "log/log.h"
#include "obs/obs.h"
#include "sched/sched.h"
#include "trans/trans.h"
#include "util/util.h"
#include "wavegen/wavegen.h"

#endif // !MODULE_H

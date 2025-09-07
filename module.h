#ifndef MODULE_H
#define MODULE_H

#include "version.h"

#if defined(__linux__) || defined(_WIN32)
#include "communicate/communicate.h"
#include "shared_mem/shared_mem.h"
#endif

#include "container/container.h"
#include "controller/controller.h"
#include "filter/filter.h"
#include "foc/foc.h"
#include "logger/logger.h"
#include "observer/observer.h"
#include "scheduler/scheduler.h"
#include "transform/transform.h"
#include "util/util.h"
#include "wavegen/wavegen.h"

#endif // !MODULE_H

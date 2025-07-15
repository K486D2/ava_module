#ifndef MODULE_H
#define MODULE_H

#ifdef __cpluscplus
extern "C" {
#endif

#include "version.h"

#if defined(__linux__) || defined(_WIN32) || defined(__ZEPHYR__)
#include "communicate/communicate.h"
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

#ifdef __cpluscplus
}
#endif

#endif // !MODULE_H

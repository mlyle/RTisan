#ifndef __RTISAN_INTERNAL_H
#define __RTISAN_INTERNAL_H

#if defined(STM32F303xC)
#define STM32F30X
#include <stm32f303xc.h>
#include <core_cm4.h>
#elif defined(STM32F405)
#define STM32F4XX
#include <stm32f405xx.h>
#include <core_cm4.h>
#endif

#include <rtisan.h>

#include <rtisan_assert.h>

#define HAVE_FPU

#endif

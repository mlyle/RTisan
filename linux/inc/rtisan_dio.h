#ifndef _RTISAN_DIO_H
#define _RTISAN_DIO_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t DIOInitTag_t;

#define DIOInit (void)
#define GET_DIO_INITIAL(x) false
#define DIOWrite(x,y) (void) x
#define DIOToggle(x) (void) x

#endif

#ifndef ZCC_STDDEF_H
#define ZCC_STDDEF_H

#ifndef __ZSIZE_T
    #define __ZSIZE_T
    #define __ZSIZE_TYPE__ __SIZE_TYPE__
    typedef __ZSIZE_TYPE__ size_t;
#endif

#ifndef __ZNULL
    #define __ZNULL
    #define ZNULL ((void*)0)
    #define NULL ZNULL
#endif

#endif
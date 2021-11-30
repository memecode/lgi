#ifndef __OS_CLASS_H
#define __OS_CLASS_H

#include <functional>

#ifdef _MSC_VER
#define LTHREAD_DATA __declspec( thread )
#else
#define LTHREAD_DATA __thread
#endif



#endif

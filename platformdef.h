#ifndef __PLATFORMDEF_H_
#define __PLATFORMDEF_H_

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#endif

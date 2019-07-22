#ifndef PLATFORMDEF_H_
#define PLATFORMDEF_H_

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#if defined (_MSC_VER)
#define isspace iswspace
#define isprint iswprint
#endif

#endif

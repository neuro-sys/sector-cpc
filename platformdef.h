#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

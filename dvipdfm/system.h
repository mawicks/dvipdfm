#ifndef _SYSTEM_H_

#define _SYSTEM_H_

#define FOPEN_R_MODE "rb"
#define FOPEN_RBIN_MODE "rb"
#define FOPEN_WBIN_MODE "wb"


#ifdef WIN32
#  undef ERROR
#  undef NO_ERROR
#  undef OUT
#  undef CDECL
#  define CDECL __cdecl
#  pragma warning(disable : 4101 4018)
#else
#  define CDECL
#endif /* WIN32 */

#endif /* _SYSTEM_H_ */

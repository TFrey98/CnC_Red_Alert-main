/* objbase.h -- COM base declarations, reduced to what the shims need. */
#ifndef WWPORT_COMPAT_OBJBASE_H
#define WWPORT_COMPAT_OBJBASE_H
#include "windows.h"
typedef struct _GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID, IID, CLSID;
typedef const GUID & REFIID;
typedef const GUID & REFCLSID;
#define STDMETHODCALLTYPE
#endif

/* iostream.h -- pre-standard spelling; hoists the std names to global scope
** the way the original <iostream.h> did. */
#ifndef WWPORT_COMPAT_IOSTREAM_H
#define WWPORT_COMPAT_IOSTREAM_H
#include <iostream>
using std::cin; using std::cout; using std::cerr; using std::endl;
#endif

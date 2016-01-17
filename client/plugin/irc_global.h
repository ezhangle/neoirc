#ifndef IRC_GLOBAL_H
#define IRC_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef IRC_LIB
# define IRC_EXPORT Q_DECL_EXPORT
#else
# define IRC_EXPORT Q_DECL_IMPORT
#endif

#endif // IRC_GLOBAL_H

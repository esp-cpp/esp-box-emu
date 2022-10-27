
/**
    \file gameboycore_api.h
    \brief Define export macros
    \author Natesh Narain
    \date Nov 12 2016
    \defgroup API
*/

#ifndef GAMEBOYCORE_API_H
#define GAMEBOYCORE_API_H

/* DLL Export/Import */

#if defined(_MSC_VER) && !defined(GAMEBOYCORE_STATIC)
#   if defined(GAMEBOYCORE_EXPORT)
#       define GAMEBOYCORE_API __declspec(dllexport)
#   else
#       define GAMEBOYCORE_API __declspec(dllimport)
#   endif
#else
#   define GAMEBOYCORE_API
#endif

#endif

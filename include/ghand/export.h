#ifndef GHAND_EXPORT_H_
#define GHAND_EXPORT_H_

#ifdef _WIN32
  #ifdef GHAND_BUILD
    #define GHAND_API __declspec(dllexport)
  #else
    #define GHAND_API __declspec(dllimport)
  #endif
#else
  #define GHAND_API
#endif

#endif  // GHAND_EXPORT_H_

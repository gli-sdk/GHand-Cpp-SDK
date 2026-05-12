#ifndef XIAOYAO_EXPORT_H_
#define XIAOYAO_EXPORT_H_

#ifdef _WIN32
  #ifdef XIAOYAO_BUILD
    #define GHAND_API __declspec(dllexport)
  #else
    #define GHAND_API __declspec(dllimport)
  #endif
#else
  #define GHAND_API
#endif

#endif  // XIAOYAO_EXPORT_H_

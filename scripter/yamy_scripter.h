#ifndef _YAMY_SCRIPTER_H
#define _YAMY_SCRIPTER_H

#ifdef _YAMY_SCRIPTER_IMPL
#  define SCRIPTER_API __declspec(dllexport)
#else // _YAMY_SCRIPTER_IMPL
#  define SCRIPTER_API __declspec(dllimport)
#endif // _YAMY_SCRIPTER_IMPL

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
#if 0 // Dummy code to prevent the IDE from indenting inside extern "C"
}
#endif

extern SCRIPTER_API void scripter_engine(int argc, wchar_t *argv[]);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus
#endif // _YAMY_SCRIPTER_H

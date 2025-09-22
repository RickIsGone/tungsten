#include <cstdarg>
#include <cstdlib>
#include <cstdio>
// core tungsten functions

extern "C" {
double shell(char* command) {
   if (!command)
      return -1;

   int result = system(command);
#ifdef _WIN32
   return result;
#else
   return WEXITSTATUS(result);
#endif
}

void print(const char* str, ...) {
   if (!str)
      return;

   va_list args;
   va_start(args, str);
   vprintf(str, args);
   va_end(args);
}

void input(const char* str, ...) {
   if (!str)
      return;

   va_list args;
   va_start(args, str);
   vfscanf(stdin, str, args);
   va_end(args);
}
}

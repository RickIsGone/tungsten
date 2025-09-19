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

void print(char* str) {
   if (!str)
      return;
   printf("%s", str);
}
}

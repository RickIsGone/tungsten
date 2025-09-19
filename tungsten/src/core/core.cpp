#include <cstdlib>
// core tungsten functions

extern "C" double shell(char* command) {
   if (!command)
      return -1;

   int result = system(command);
#ifdef _WIN32
   return result;
#else
   return WEXITSTATUS(result);
#endif
}

#include <cstdlib>
// core tungsten functions

extern "C" int shell(const unsigned char* command) {
   if (!command)
      return -1;

   int result = system(reinterpret_cast<const char*>(command));
#ifdef _WIN32
   return result;
#else
   return WEXITSTATUS(result);
#endif
}


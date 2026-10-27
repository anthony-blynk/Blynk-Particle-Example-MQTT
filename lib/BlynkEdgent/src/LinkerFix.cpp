#include <Particle.h>

// Keep the linker happy
extern "C" {

__attribute__((weak))
int _gettimeofday( struct timeval *tv, void *tzvp )
{
  tv->tv_sec = 0;
  tv->tv_usec = 0;
  return 0;
}

}

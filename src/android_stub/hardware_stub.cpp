#include <hardware/hardware.h>

extern "C" {

int hw_get_module(const char *id, const struct hw_module_t **module)
{
   /* PATCH: honest stub - no real Android HAL module registry is present
    * under Termux, so there is nothing to return. Returning 0 (success)
    * without setting *module previously left callers dereferencing an
    * uninitialized/NULL pointer, since real hw_get_module() guarantees
    * *module is valid on success. Report failure instead so callers take
    * their existing (and already-correct) fallback path. */
   *module = 0;
   return -1;
}

}

/******************************************************************************
 * WebAssembly memory cleansing without an OpenSSL runtime dependency.
 ******************************************************************************/

#include <support/cleanse.h>

void memory_cleanse(void* pointer, size_t length) {
    volatile auto* bytes = static_cast<volatile unsigned char*>(pointer);
    while (length-- > 0) {
        *bytes++ = 0;
    }
}

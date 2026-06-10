#ifndef BDKFFI_ERROR_STRINGS_H
#define BDKFFI_ERROR_STRINGS_H

#ifdef __cplusplus
extern "C" {
#endif

const char* bdkffi_script_error_string(int code);
const char* bdkffi_dos_error_string(int code);
int bdkffi_cpp_script_err_error_count(void);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_ERROR_STRINGS_H */

#ifndef __SCRIPT_ERROR_CGO_H__
#define __SCRIPT_ERROR_CGO_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cgo_script_error_string convert an error code to C-string
 * 
 * The client code is responsible to free memory of the string
 * 
 * By convention, if an returned error is higher than SCRIPT_ERR_ERROR_COUNT
 * This is an exception being thrown by C++ code
 *
 */
const char *cgo_script_error_string(const int error);

#ifdef __cplusplus
}
#endif

#endif /* __SCRIPT_ERROR_CGO_H__ */

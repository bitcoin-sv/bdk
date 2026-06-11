#include <bdkffi/bdkffi.h>

int main()
{
    bdkffi_txvalidator_t validator = bdkffi_txvalidator_create("main", 4);
    if (validator == nullptr) {
        return 1;
    }

    const int script_error_count = bdkffi_cpp_script_err_error_count();
    bdkffi_txvalidator_destroy(validator);

    return script_error_count > 0 ? 0 : 2;
}

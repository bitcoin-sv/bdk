# Native verification benchmarks intentionally suppress core logging so
# measurements do not include node log calls.
list(APPEND BDK_CORE_PRIVATE_COMPILE_DEFINITIONS DISABLE_LOGGING)

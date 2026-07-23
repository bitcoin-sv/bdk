# Canonical native bdk_core configuration. Specialized consumers may override
# these extension points from their own directory before core is configured.
set(BDK_CORE_FIND_OPENSSL ON)
set(BDK_CORE_LINK_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
set(BDK_CORE_EXCLUDED_BSV_SOURCES)
set(BDK_CORE_ADDITIONAL_SOURCES)
set(BDK_CORE_PRIVATE_COMPILE_DEFINITIONS)

function(bdk_configure_secp256k1_targets)
endfunction()

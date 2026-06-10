#ifndef BDKFFI_VERSION_H
#define BDKFFI_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

int bdkffi_bsv_client_version_major(void);
int bdkffi_bsv_client_version_minor(void);
int bdkffi_bsv_client_version_revision(void);
const char* bdkffi_bsv_version_string(void);

const char* bdkffi_bsv_git_commit_tag_or_branch(void);
const char* bdkffi_bsv_git_commit_hash(void);
const char* bdkffi_bsv_git_commit_datetime(void);

int bdkffi_bdk_version_major(void);
int bdkffi_bdk_version_minor(void);
int bdkffi_bdk_version_patch(void);
const char* bdkffi_bdk_version_string(void);

const char* bdkffi_source_git_commit_tag_or_branch(void);
const char* bdkffi_source_git_commit_hash(void);
const char* bdkffi_source_git_commit_datetime(void);
const char* bdkffi_bdk_build_datetime_utc(void);

int bdkffi_bdk_rust_version_major(void);
int bdkffi_bdk_rust_version_minor(void);
int bdkffi_bdk_rust_version_patch(void);
const char* bdkffi_bdk_rust_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* BDKFFI_VERSION_H */

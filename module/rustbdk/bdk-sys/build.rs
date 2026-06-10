use std::env;
use std::path::{Path, PathBuf};

fn main() {
    let os = env::var("CARGO_CFG_TARGET_OS").expect("CARGO_CFG_TARGET_OS must be set by Cargo");
    let arch =
        env::var("CARGO_CFG_TARGET_ARCH").expect("CARGO_CFG_TARGET_ARCH must be set by Cargo");

    let os_arch = match (os.as_str(), arch.as_str()) {
        ("linux", "x86_64") => "linux_x86_64",
        ("linux", "aarch64") => "linux_aarch64",
        ("macos", "aarch64") => "darwin_arm64",
        ("macos", "x86_64") => "darwin_x86_64",
        _ => panic!("unsupported target {os}/{arch}"),
    };

    let lib_name = env::var("BDK_LIB_NAME").unwrap_or_else(|_| format!("bdkffi_{os_arch}"));
    if lib_name.contains('/') || lib_name.contains('\\') {
        panic!("BDK_LIB_NAME must be a library stem, not a path");
    }

    let archive_name = format!("lib{lib_name}.a");
    let lib_dir = resolve_lib_dir(&archive_name);

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=static={lib_name}");

    let cxx_runtime = env::var("BDK_CXX_RUNTIME").unwrap_or_else(|_| match os.as_str() {
        "macos" => "c++".to_owned(),
        _ => "stdc++".to_owned(),
    });
    match cxx_runtime.as_str() {
        "none" => {}
        "stdc++" | "c++" => println!("cargo:rustc-link-lib=dylib={cxx_runtime}"),
        other => panic!("unsupported BDK_CXX_RUNTIME={other}; expected stdc++, c++, or none"),
    }

    println!("cargo:rustc-link-lib=dylib=m");
    if os == "linux" {
        println!("cargo:rustc-link-lib=dylib=pthread");
        println!("cargo:rustc-link-lib=dylib=dl");
    }

    println!("cargo:rerun-if-env-changed=BDK_LIB_DIR");
    println!("cargo:rerun-if-env-changed=BDK_LIB_NAME");
    println!("cargo:rerun-if-env-changed=BDK_CXX_RUNTIME");
    println!("cargo:rerun-if-env-changed=CMAKE_INSTALL_PREFIX");
    println!("cargo:rerun-if-env-changed=BDK_BUILD_DIR");
    println!("cargo:rerun-if-env-changed=CMAKE_BINARY_DIR");
}

fn resolve_lib_dir(archive_name: &str) -> PathBuf {
    let manifest_dir =
        PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR must be set"));
    let rustbdk_dir = manifest_dir
        .parent()
        .expect("bdk-sys must live under module/rustbdk")
        .to_path_buf();
    let repo_root = rustbdk_dir
        .parent()
        .and_then(Path::parent)
        .expect("module/rustbdk must live under the repository root")
        .to_path_buf();

    if let Ok(dir) = env::var("BDK_LIB_DIR") {
        let dir = PathBuf::from(dir);
        if archive_exists(&dir, archive_name) {
            return dir;
        }
        panic!(
            "BDK_LIB_DIR was set to {}, but {} was not found there",
            dir.display(),
            archive_name
        );
    }

    let mut candidates = Vec::new();
    candidates.push(("bdk-sys/lib", manifest_dir.join("lib")));

    if let Ok(prefix) = env::var("CMAKE_INSTALL_PREFIX") {
        candidates.push((
            "CMAKE_INSTALL_PREFIX/lib/bdkffi",
            PathBuf::from(prefix).join("lib").join("bdkffi"),
        ));
    }

    if let Ok(build_dir) = env::var("BDK_BUILD_DIR") {
        push_raw_build_tree_candidates(&mut candidates, "BDK_BUILD_DIR", PathBuf::from(build_dir));
    }
    if let Ok(build_dir) = env::var("CMAKE_BINARY_DIR") {
        push_raw_build_tree_candidates(
            &mut candidates,
            "CMAKE_BINARY_DIR",
            PathBuf::from(build_dir),
        );
    }

    // The documented priority list is: BDK_LIB_DIR, bdk-sys/lib,
    // CMAKE_INSTALL_PREFIX/lib/bdkffi, then raw build trees. The repo-local
    // entries below are common raw CMake build-tree locations and still only
    // accept the selected rustbdk archive name.
    push_raw_build_tree_candidates(&mut candidates, "repo build", repo_root.join("build"));
    push_raw_build_tree_candidates(
        &mut candidates,
        "repo cmake-build-release",
        repo_root.join("cmake-build-release"),
    );
    push_raw_build_tree_candidates(
        &mut candidates,
        "repo cmake-build-debug",
        repo_root.join("cmake-build-debug"),
    );

    for (_, dir) in &candidates {
        if archive_exists(dir, archive_name) {
            return dir.clone();
        }
    }

    let searched = candidates
        .iter()
        .map(|(label, dir)| format!("  - {label}: {}", dir.display()))
        .collect::<Vec<_>>()
        .join("\n");

    // libbdkffi archives are local build outputs, not committed artifacts. Run a prior
    // CMake build that produces MergeBDKFFI, or point BDK_LIB_DIR at an existing archive.
    // TODO: replace the local-only flow when CI-published rustbdk archives are available.
    panic!(
        "could not find {archive_name}. A prior cmake --build producing MergeBDKFFI is required, \
         or set BDK_LIB_DIR to the directory containing {archive_name}.\nSearched:\n{searched}"
    );
}

fn push_raw_build_tree_candidates(
    candidates: &mut Vec<(&'static str, PathBuf)>,
    label: &'static str,
    build_dir: PathBuf,
) {
    candidates.push((label, build_dir.clone()));
    candidates.push((label, build_dir.join("x64").join("release")));
    candidates.push((label, build_dir.join("x64").join("debug")));
}

fn archive_exists(dir: &Path, archive_name: &str) -> bool {
    dir.join(archive_name).is_file()
}

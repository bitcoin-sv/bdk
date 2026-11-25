#pragma once

// A temporary directory for tests that read files by path, such as a fake
// filesystem root, and the files written into it.

#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace bsv::test
{
    // A new, empty directory under temp_directory_path(), removed with
    // everything below it on destruction.
    class TempDirectory
    {
    public:
        TempDirectory()
        {
            std::random_device random;
            do
                path_ = std::filesystem::temp_directory_path()
                      / ("bdk_test_" + std::to_string(random()));
            while(!std::filesystem::create_directory(path_));
        }

        ~TempDirectory()
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }

        TempDirectory(const TempDirectory&) = delete;
        TempDirectory& operator=(const TempDirectory&) = delete;
        TempDirectory(TempDirectory&&) = delete;
        TempDirectory& operator=(TempDirectory&&) = delete;

        [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

    private:
        std::filesystem::path path_;
    };

    // Creates any missing parent directories. Throws std::runtime_error when the
    // file can't be written, so the test fails where it wrote rather than where
    // the file is read.
    inline void WriteFile(const std::filesystem::path& path, const std::string& content)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file{path, std::ios::binary};
        file << content;
        file.close();
        if(!file)
            throw std::runtime_error{"can't write " + path.string()};
    }
}

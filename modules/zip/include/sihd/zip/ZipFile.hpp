#ifndef __SIHD_ZIP_ZIPFILE_HPP__
#define __SIHD_ZIP_ZIPFILE_HPP__

#include <string_view>

#include <sihd/util/Array.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::zip
{

class ZipFile: public sihd::util::IReader
{
    public:
        struct ZipEntry
        {
                const char *name = nullptr;
                ssize_t index = -1;
                size_t size = 0;
                size_t compressed_size = 0;
                sihd::util::Timestamp modification_time = sihd::util::Timestamp(-1);
                uint32_t crc = 0;
        };

        ZipFile();
        ZipFile(const std::string & path, bool read_only = false, bool do_strict_checks = false);
        virtual ~ZipFile();

        ZipFile(const ZipFile &) = delete;
        ZipFile & operator=(const ZipFile &) = delete;
        ZipFile(ZipFile &&) = delete;
        ZipFile & operator=(ZipFile &&) = delete;

        // set internal reading buff size
        sihd::util::Result<void> set_buffer_size(size_t size);
        // set general password for encrypted content
        sihd::util::Result<void> set_password(const std::string & password);
        // choose encryption method
        sihd::util::Result<void> set_aes_encryption(int aes);

        sihd::util::Result<void> open(const std::string & path, bool read_only = false, bool do_strict_checks = false);
        bool is_open() const;
        // close and save
        sihd::util::Result<void> close();
        // close and discard changes
        void discard();

        void no_encrypt();
        void encrypt_in_aes_128();
        void encrypt_in_aes_192();
        void encrypt_in_aes_256();

        std::string_view archive_comment() const;
        sihd::util::Result<void> comment_archive(std::string_view comment);
        sihd::util::Result<void> set_archive_readonly(bool active);

        // encrypt all current entries
        sihd::util::Result<void> encrypt_all(const std::string & password);

        // total original entries before new additions
        ssize_t count_original_entries() const;
        // total current entries
        ssize_t count_entries() const;

        // load a zip file entry from the archive
        sihd::util::Result<ZipEntry> load_entry(size_t index);
        sihd::util::Result<ZipEntry> load_entry(const std::string & name);
        // iterate over all original entries (excluding new additions)
        sihd::util::Result<bool> read_next_entry();

        // loaded entry informations
        bool is_entry_loaded() const;
        std::string_view entry_name() const;
        ssize_t entry_index() const;
        std::string_view entry_comment() const;
        const ZipEntry & current_entry() const;
        bool is_entry_directory() const;

        // read loaded entry: false means end of entry, an error means the read failed
        sihd::util::Result<bool> read_next();
        // get data from entry
        bool get_read_data(sihd::util::ArrCharView & view) const;
        // read loaded entry with custom password: 0 means end of entry
        sihd::util::Result<ssize_t> read_entry(const std::string & password = "");

        // change entry
        sihd::util::Result<void> unchange_entry();
        sihd::util::Result<void> remove_entry();
        sihd::util::Result<void> rename_entry(const std::string & new_name);
        sihd::util::Result<void> modify_entry_time(sihd::util::Timestamp new_timestamp);
        sihd::util::Result<void> comment_entry(std::string_view comment);
        sihd::util::Result<void> encrypt_entry(const std::string & password);
        // note: the view data must remain valid until close() or discard() is called
        sihd::util::Result<void> replace_entry(sihd::util::ArrCharView view);

        // add dir in memory
        sihd::util::Result<void> add_dir(const std::string & name);
        // add file content in memory
        // note: the view data must remain valid until close() or discard() is called
        sihd::util::Result<void> add_file(const std::string & name, sihd::util::ArrCharView view);

        // add either file or directory from filesystem
        sihd::util::Result<void> add_from_fs(const std::string & name, const std::string & path);
        // recursively add directory and its content from filesystem
        sihd::util::Result<void> add_dir_from_fs(const std::string & name, const std::string & path);
        // add file from filesystem
        sihd::util::Result<void> add_file_from_fs(const std::string & name, const std::string & path);

        // dump loaded entry to fs either making a directory or writing whole content in file
        sihd::util::Result<void> dump_entry_to_fs(const std::string & path, const std::string & password = "");

    protected:

    private:
        struct ZipHandle;

        std::unique_ptr<ZipHandle> _zip_handle;

        // current loaded entry data
        ZipEntry _current_zip_entry;

        // read buffer
        sihd::util::ArrChar _buf;

        // libzip encryption method
        short _encryption_method;
};

} // namespace sihd::zip

#endif
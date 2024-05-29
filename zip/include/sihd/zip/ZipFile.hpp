#ifndef __SIHD_ZIP_ZIPFILE_HPP__
#define __SIHD_ZIP_ZIPFILE_HPP__

#include <sihd/util/Array.hpp>
#include <sihd/util/IReader.hpp>
#include <sihd/util/IWriter.hpp>

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
                sihd::util::Timestamp modification_time = -1;
                uint32_t crc = 0;
        };

        ZipFile();
        ZipFile(std::string_view path, bool read_only = false, bool do_strict_checks = false);
        virtual ~ZipFile();

        // set internal reading buff size
        bool set_buffer_size(size_t size);
        // set general password for encrypted content
        bool set_password(std::string_view password);
        // choose encryption method
        bool set_aes_encryption(int aes);

        bool open(std::string_view path, bool read_only = false, bool do_strict_checks = false);
        bool is_open() const;
        // close and save
        bool close();
        // close and discard changes
        void discard();

        void no_encrypt();
        void encrypt_in_aes_128();
        void encrypt_in_aes_192();
        void encrypt_in_aes_256();

        std::string_view archive_comment() const;
        bool comment_archive(std::string_view comment);
        bool set_archive_readonly(bool active);

        // encrypt all current entries
        bool encrypt_all(std::string_view password);

        // total original entries before new additions
        ssize_t count_original_entries() const;
        // total current entries
        ssize_t count_entries() const;

        // load a zip file entry from the archive
        bool load_entry(size_t index);
        bool load_entry(std::string_view name);
        // iterate over all original entries (excluding new additions)
        bool read_next_entry();

        // loaded entry informations
        bool is_entry_loaded() const;
        std::string_view entry_name() const;
        ssize_t entry_index() const;
        std::string_view entry_comment() const;
        const ZipEntry & current_entry() const;
        bool is_entry_directory() const;

        // read loaded entry
        bool read_next();
        // get data from entry
        bool get_read_data(sihd::util::ArrCharView & view) const;
        // read loaded entry with custom password
        ssize_t read_entry(std::string_view password = "");

        // change entry
        bool unchange_entry();
        bool remove_entry();
        bool rename_entry(std::string_view new_name);
        bool modify_entry_time(sihd::util::Timestamp new_timestamp);
        bool comment_entry(std::string_view comment);
        bool encrypt_entry(std::string_view password);
        bool replace_entry(sihd::util::ArrCharView view);

        // add dir in memory
        bool add_dir(std::string_view name);
        // add file content in memory
        bool add_file(std::string_view name, sihd::util::ArrCharView view);

        // add either file or directory from filesystem
        bool add_from_fs(std::string_view name, std::string_view path);
        // recursively add directory and its content from filesystem
        bool add_dir_from_fs(std::string_view name, std::string_view path);
        // add file from filesystem
        bool add_file_from_fs(std::string_view name, std::string_view path);

        // dump loaded entry to fs either making a directory or writing whole content in file
        bool dump_entry_to_fs(std::string_view path, std::string_view password = "");

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
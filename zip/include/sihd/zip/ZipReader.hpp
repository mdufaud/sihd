#ifndef __SIHD_ZIP_ZIPREADER_HPP__
# define __SIHD_ZIP_ZIPREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/util/IReader.hpp>
# include <sihd/zip/ZipUtils.hpp>
# include <sihd/util/Array.hpp>

namespace sihd::zip
{

class ZipReader:    public sihd::util::Named,
                    public sihd::util::Configurable,
                    public sihd::util::IReaderTimestamp
{
    public:
        ZipReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~ZipReader();

        // set internal reading buff size
        bool set_buffer_size(size_t size);
        // set general password for encrypted content
        bool set_password(std::string_view password);
        // when looping on read_next, get_read_data returns only entries names
        bool set_read_entry_names(bool active);

        bool open(std::string_view path, bool do_strict_checks = false);
        bool is_open() const { return _zip_ptr != nullptr; }
        // close and save
        bool close();
        // close and discard changes
        void discard();

        bool read_next();
		bool get_read_data(char **data, size_t *size) const;
		bool get_read_timestamp(time_t *nano_timestamp) const;

        // modify zip entries
        bool remove(size_t index);
        bool rename(size_t index, std::string_view name);

        size_t count_entries() const { return _total_entries; }
        bool load_entry(size_t index);
        bool load_entry(std::string_view name);

        const struct zip_stat *get_entry() const;
        bool is_entry_directory() const;

        ssize_t read_entry(std::string_view password = "");
        bool write_entry(std::string_view path, std::string_view password = "");

    protected:

    private:
        void _init();
        void _delete_buffer();
        void _close_file();
        bool _allocate_buffer_if_null();

        zip_t *_zip_ptr;
        bool _only_load_entries;

        // read buffer
        char *_buf_ptr;
        size_t _buf_total_size;
        ssize_t _read_buf_size;
        // information on current zip
        size_t _total_entries;
        size_t _current_idx;
        bool _entry_error;
        struct zip_stat _current_zip_entry;
        // zip file entry
        bool _zip_reading_file;
        struct zip_file *_current_zip_file_ptr;
};

}

#endif
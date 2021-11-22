#ifndef __SIHD_ZIP_ZIPREADER_HPP__
# define __SIHD_ZIP_ZIPREADER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/zip/ZipUtils.hpp>
# include <sihd/util/Array.hpp>

namespace sihd::zip
{

class ZipReader: public sihd::util::Named, public sihd::util::Configurable
{
    public:
        ZipReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~ZipReader();

        bool set_buff_size(size_t size);
        bool set_password(const char *password);

        bool open(const std::string & path, bool do_strict_checks = false);
        bool close();
        void discard();

        bool remove(size_t index);
        bool rename(size_t index, const char *name);

        bool get_entries(std::vector<struct zip_stat> & entries);
        bool get_entry(const std::string & name, struct zip_stat *entry);
        bool is_entry(const std::string & name);
        size_t count_entries();

        bool fs_write_entry(const struct zip_stat *entry, const std::string & output_path, const char *password = nullptr);
        bool fs_write_entry(const std::string & name, const std::string & output_path, const char *password = nullptr);
        bool fs_write_all(const std::string & output_path);

        ssize_t read_entry(const struct zip_stat *entry, void *buf, size_t size, const char *password = nullptr);
        ssize_t read_entry(const struct zip_stat *entry, const char *password = nullptr);
        bool read_entry_into(const struct zip_stat *entry, std::string & into, const char *password = nullptr);

        ssize_t read_entry(const std::string & name, void *buf, size_t size, const char *password = nullptr);
        ssize_t read_entry(const std::string & name, const char *password = nullptr);
        bool read_entry_into(const std::string & name, std::string & into, const char *password = nullptr);

        char *buf() { return _buf_ptr; }
        ssize_t buf_size() const { return _buf_size; }

    protected:

    private:
        void _delete_buffer();
        bool _allocate_buffer_if_null();

        zip_t *_zip_ptr;
        size_t _buf_size;
        char *_buf_ptr;
};

}

#endif
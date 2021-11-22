#ifndef __SIHD_ZIP_ZIPWRITER_HPP__
# define __SIHD_ZIP_ZIPWRITER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/util/Configurable.hpp>
# include <sihd/zip/ZipUtils.hpp>

namespace sihd::zip
{

class ZipWriter: public sihd::util::Named, public sihd::util::Configurable
{
    public:
        ZipWriter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~ZipWriter();

        // choose encryption method
        bool set_aes_encryption(int aes);
        void no_encrypt();
        void encrypt_in_aes_128();
        void encrypt_in_aes_192();
        void encrypt_in_aes_256();

        bool open(const std::string & path, bool fails_if_exists = true, bool truncate = false);

        // add either file or directory from filesystem
        bool fs_add(const std::string & path, const std::string & name);
        // recursively add directory and its content from filesystem
        bool fs_add_dir(const std::string & path, const std::string & name);
        // add file from filesystem
        bool fs_add_file(const std::string & path, const std::string & name);

        // add dir in memory
        bool add_dir(const std::string & name);
        // add file content in memory
        bool add_file(const std::string & name, const void *data, size_t size);

        // encrypt a single entry
        bool encrypt(size_t index, const char *password);
        // encrypt all entries
        bool encrypt_all(const char *password);

        // close and save changes
        bool close();

    protected:

    private:
        bool _add_source(const std::string & name, zip_source_t *source);

        zip_t *_zip_ptr;
        short _encryption_method;
};

}

#endif
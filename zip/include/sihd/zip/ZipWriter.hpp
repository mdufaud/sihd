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

        bool set_aes_encryption(int aes);

        void no_encrypt();
        void encrypt_in_aes_128();
        void encrypt_in_aes_192();
        void encrypt_in_aes_256();

        bool open(const std::string & path, bool fails_if_exists = true, bool truncate = false);
        bool fs_add(const std::string & path, const std::string & name);
        bool fs_add_dir(const std::string & path, const std::string & name);
        bool fs_add_file(const std::string & path, const std::string & name);

        bool add_dir(const std::string & name);
        bool add_file(const std::string & name, const void *data, size_t size);

        bool encrypt(size_t index, const char *password);
        bool encrypt_all(const char *password);

        bool close();

    protected:

    private:
        bool _add_source(const std::string & name, zip_source_t *source);

        zip_t *_zip_ptr;
        short _encryption_method;
};

}

#endif
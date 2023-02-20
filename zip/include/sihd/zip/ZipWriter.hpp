#ifndef __SIHD_ZIP_ZIPWRITER_HPP__
#define __SIHD_ZIP_ZIPWRITER_HPP__

#include <zip.h>

#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Configurable.hpp>
#include <sihd/util/Node.hpp>

namespace sihd::zip
{

class ZipWriter: public sihd::util::Named,
                 public sihd::util::Configurable
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

        bool open(std::string_view path, bool fails_if_exists = true, bool truncate = false);

        // add either file or directory from filesystem
        bool fs_add(std::string_view path, std::string_view name);
        // recursively add directory and its content from filesystem
        bool fs_add_dir(std::string_view path, std::string_view name);
        // add file from filesystem
        bool fs_add_file(std::string_view path, std::string_view name);

        // add dir in memory
        bool add_dir(std::string_view name);
        // add file content in memory
        bool add_file(std::string_view name, sihd::util::ArrCharView view);

        // encrypt a single entry
        bool encrypt(size_t index, std::string_view password);
        // encrypt all entries
        bool encrypt_all(std::string_view password);

        // close and save changes
        bool close();

    protected:

    private:
        bool _add_source(std::string_view name, zip_source_t *source);

        zip_t *_zip_ptr;
        short _encryption_method;
};

} // namespace sihd::zip

#endif
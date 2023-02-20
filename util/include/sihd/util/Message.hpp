#ifndef __SIHD_UTIL_MESSAGE_HPP__
#define __SIHD_UTIL_MESSAGE_HPP__

#include <stdexcept>
#include <unordered_map>

#include <sihd/util/Array.hpp>
#include <sihd/util/IMessageField.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/Types.hpp>

namespace sihd::util
{

class Message: public Node,
               public IMessageField
{
    public:
        Message(const std::string & name, Node *parent = nullptr);
        virtual ~Message();

        // IMessageField
        virtual size_t field_size() const override { return 1; }
        virtual size_t field_byte_size() const override { return _total_size; }
        virtual Type field_type() const override { return Type::TYPE_BYTE; }
        virtual bool field_assign_buffer(void *buffer) override;
        virtual bool field_read_from(const void *buffer, size_t size) override;
        virtual bool field_write_to(void *buffer, size_t size) override;
        virtual bool field_resize(size_t size) override;
        virtual bool is_finished() const override { return _finished; }
        virtual bool finish() override;

        // Node
        virtual bool on_check_link(const std::string & name, Named *child) override;
        virtual bool on_add_child(const std::string & name, Named *child) override;
        virtual void on_remove_child(const std::string & name, Named *child) override;

        // ICloneable
        virtual IMessageField *clone() const override;

        template <typename T>
        bool add_field(const std::string & name, size_t size = 1)
        {
            return this->add_field(name, Types::type<T>(), size);
        }

        virtual bool add_field(const std::string & name, Type dt, size_t size = 1);

        /**
         * @brief
         *
         * @param name
         * @param field must be a sihd::util::Named to be added as a composite
         * @return false if not a Named
         */
        virtual bool add_field(const std::string & name, IMessageField *field);

        virtual std::string description() const override;

        const IArray *array() const { return &_arr; }

    protected:
        ArrByte _arr;
        size_t _total_size;
        bool _finished;

        std::unordered_map<std::string, IMessageField *> _fields;

    private:
        bool _add_field_size(IMessageField *field);
        bool _assign_field_array(IMessageField *field);

        size_t __assign_arr_at;
};

} // namespace sihd::util

#endif

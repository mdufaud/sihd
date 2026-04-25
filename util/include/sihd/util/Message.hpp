#ifndef __SIHD_UTIL_MESSAGE_HPP__
#define __SIHD_UTIL_MESSAGE_HPP__

#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <sihd/util/Array.hpp>
#include <sihd/util/IMessageField.hpp>
#include <sihd/util/MessageField.hpp>
#include <sihd/util/Node.hpp>
#include <sihd/util/type.hpp>

namespace sihd::util
{

class Message: public Node,
               public IMessageField
{
    public:
        Message(const std::string & name, Node *parent = nullptr);
        virtual ~Message() = default;

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
            return this->add_field(name, type::from<T>(), size);
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

        virtual const IArray *array() const { return &_arr; }

        std::vector<uint8_t> to_bytes();
        bool from_bytes(std::span<const uint8_t> data);

        template <typename T>
        T get(const std::string & name, size_t idx = 0) const
        {
            return _require_field(name)->read_value<T>(idx);
        }

        template <typename T>
        bool set(const std::string & name, T value, size_t idx = 0)
        {
            MessageField *field = this->get_child<MessageField>(name);
            if (field == nullptr)
                return false;
            return field->write_value<T>(idx, value);
        }

    protected:
        ArrByte _arr;
        size_t _total_size;
        bool _finished;

        std::unordered_map<std::string, IMessageField *> _fields;

    private:
        bool _add_field_size(IMessageField *field);
        bool _assign_field_array(IMessageField *field);
        const MessageField *_require_field(const std::string & name) const;

        size_t __assign_arr_at;
};

} // namespace sihd::util

#endif

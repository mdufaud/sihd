#ifndef __SIHD_UTIL_MESSAGE_HPP__
# define __SIHD_UTIL_MESSAGE_HPP__

# include <string>
# include <stdexcept>
# include <sihd/util/Datatype.hpp>
# include <sihd/util/Array.hpp>
# include <sihd/util/OrderedNode.hpp>
# include <sihd/util/Logger.hpp>
# include <sihd/util/IMessageField.hpp>
# include <sihd/util/MessageField.hpp>

namespace sihd::util
{

class Message:  public sihd::util::OrderedNode,
                virtual public IMessageField
{
    public:
        Message(const std::string & name, Node *parent = nullptr);
        virtual ~Message();

        // IMessageField
        virtual size_t  get_field_byte_size() override { return _total_size; }
        virtual bool    assign_field_buffer(uint8_t *buffer) override;
        virtual bool    field_read_from(uint8_t *buffer, size_t size) override;
        virtual bool    field_write_to(uint8_t *buffer, size_t size) override;
        virtual bool    is_finished() const override { return _finished; }
        virtual bool    finish() override;

        // ICloneable
        virtual IMessageField *clone() override;

        template <typename T>
        bool    add_field(const std::string & name, size_t size = 1)
        {
            return this->add_field(name, Datatype::type_to_datatype<T>(), size);
        }
        virtual bool    add_field(const std::string & name, Type dt, size_t size = 1);
        /**
         * @brief 
         * 
         * @param name 
         * @param field must be a sihd::util::Named to be added as a composite
         * @return true if added
         * @return false if not a Named
         */
        virtual bool    add_field(const std::string & name, IMessageField *field);

        // Named
        virtual std::string     get_description() const override
        {
            return Str::format("%lu bytes", _total_size);
        }

        IArray *array() { return &_arr; };

    protected:
        virtual bool    _add_field_size(IMessageField *field);
        virtual bool    _assign_field_array(IMessageField *field);

    private:
        ArrByte _arr;
        size_t  _total_size;
        bool    _finished;
        size_t  __assign_arr_at;
};

}

#endif 
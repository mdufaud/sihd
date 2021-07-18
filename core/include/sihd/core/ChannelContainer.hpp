#ifndef __SIHD_CORE_CHANNELCONTAINER_HPP__
# define __SIHD_CORE_CHANNELCONTAINER_HPP__

# include <sihd/util/Node.hpp>
# include <sihd/core/Channel.hpp>
# include <sihd/util/Configurable.hpp>

namespace sihd::core
{

using namespace sihd::util;

class ChannelContainer: public Node,
                        public Configurable
{
    public:
        ChannelContainer(const std::string & name, Node *parent = nullptr);
        virtual ~ChannelContainer();

        Channel *get_channel(const std::string & name);
        bool    get_channel(const std::string & name, Channel **to_fill);

        Channel *add_unlinked_channel(const std::string & name, const std::string & type, size_t size = 1);
        Channel *add_channel(const std::string & name, const std::string & type, size_t size = 1);

    protected:
        virtual bool    _check_link(const std::string & name, Named *child) override;
    
    private:
        struct ChannelConfiguration
        {
            sihd::util::Datatypes type;
            size_t size;
        };
        std::map<std::string, ChannelConfiguration> _channels_link;
};

}

#endif 
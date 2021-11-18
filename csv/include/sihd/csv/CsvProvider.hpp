#ifndef __SIHD_CSV_CSVPROVIDER_HPP__
# define __SIHD_CSV_CSVPROVIDER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::csv
{

class CsvProvider:   public sihd::util::Named
{
    public:
        CsvProvider(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvProvider();

    protected:

    private:
};

}

#endif
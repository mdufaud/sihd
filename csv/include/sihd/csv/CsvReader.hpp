#ifndef __SIHD_CSV_CSVREADER_HPP__
# define __SIHD_CSV_CSVREADER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::csv
{

class CsvReader:   public sihd::util::Named
{
    public:
        CsvReader(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvReader();

    protected:

    private:
};

}

#endif
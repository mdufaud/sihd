#ifndef __SIHD_CSV_CSVWRITER_HPP__
# define __SIHD_CSV_CSVWRITER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::csv
{

class CsvWriter:   public sihd::util::Named
{
    public:
        CsvWriter(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvWriter();

    protected:

    private:
};

}

#endif
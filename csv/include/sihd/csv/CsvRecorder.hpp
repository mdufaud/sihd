#ifndef __SIHD_CSV_CSVRECORDER_HPP__
# define __SIHD_CSV_CSVRECORDER_HPP__

# include <sihd/util/Node.hpp>

namespace sihd::csv
{

class CsvRecorder:   public sihd::util::Named
{
    public:
        CsvRecorder(const std::string & name, sihd::util::Node *parent = nullptr);
        virtual ~CsvRecorder();

    protected:

    private:
};

}

#endif
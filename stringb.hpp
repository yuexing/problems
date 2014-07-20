#ifndef STRINGB_HPP
#define STRINGB_HPP

#include <iostream>
#include <sstream>
using namespace std;

struct oss_wrapper
{
public:
    oss_wrapper() :
        oss(new ostringstream()) {
        }
    ~oss_wrapper() {
        delete oss;
    }
    ostream &get_stream() 
    {
        return *oss;
    }
private:
    ostringstream *oss;
};

#define stringb(stuff)          \
    static_cast<ostringstream*>(&(oss_wrapper().get_stream() << stuff))->str()
#endif /* STRINGB_HPP */

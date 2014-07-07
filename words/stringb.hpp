#ifndef STRINGB_HPP
#define STRINGB_HPP

#include <iostream>
#include <sstream>
using namespace std;

#define stringb(stuff) \
    ((static_cast<ostringstream*>(&(*(new ostringstream()) << stuff)))->str())

#endif /* STRINGB_HPP */

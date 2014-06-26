#include <iostream>
#include <sstream>
using namespace std;

// the stringb1 works well.

#define stringb(stuff) \
    (static_cast<ostringstream*>(&(ostringstream() << stuff)))->str();

#define stringb1(stuff) \
    (static_cast<ostringstream*>(&(*(new ostringstream()) << stuff)))->str();

int main()
{
    cout << stringb("this " << " is " << " contatenated " << endl);
    cout << stringb1("this " << " is " << " contatenated " << endl);
}

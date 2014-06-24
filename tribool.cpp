#include <iostream>
using namespace std;

struct tribool_t {
public:
    enum value_t{
        FALSE,
        TRUE,
        UNKNOWN 
    };

    tribool_t()
        :v(UNKNOWN) {
        }

    tribool_t(bool b) 
        :v(b? TRUE: FALSE) {
    }

    tribool_t(value_t v)
        :v(v) {
        }

    friend ostream& operator<<(ostream& os, const tribool_t& t);
private:
    value_t v;
};

ostream& operator<<(ostream& os, const tribool_t& t) 
{
    if(t.v == tribool_t::FALSE) {
        os << "False";
    } else if(t.v == tribool_t::TRUE) {
        os << "True";
    } else {
        os  << "Unknown";
    }
}

int main()
{
    tribool_t t;
    cout << t << endl;
}

#include "jobs.hpp"

#include <string>
int main()
{
	tango::async::Job<std::string> job = tango::async::make_job(
    []() {
      return 5;
    }
  ).then(
    [](int num) {
      return std::to_string(num);
    }
  ).then(
  	[](std::string s) {
  		std::cout << s << std::endl;
  	}
  );

	return 0;
}
#include "validators.hpp"

namespace lavka {

bool IsDigits(const std::string& str) {
  for(char ch: str) {
    if(!std::isdigit(ch)) return false;
  }

  return true;
}

bool checkInterval(std::string str) {
  if(str.length() != 11) return false;
  
  std::string t1 = str.substr(0, 5);
  std::string t2 = str.substr(6, 5);

  return true;
}

}
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <stdio.h>

int main(int argc, char *argv[]) {
  boost::property_tree::ptree pt;
  std::string patt;

  std::string str("map.google.com");
  std::string::iterator it;
  for(it = str.begin() ; it <str.end(); it++) {
    if(*it == '.')
      *it = '_';
  }
  printf("new str = %s\n", str.c_str());
  boost::property_tree::ini_parser::read_ini("/home/user/boost.ini", pt);
  patt = pt.get<std::string>("maptiles_accuweather_com.pattern");
  printf("patt = %s\n", patt.c_str());

  return 0;
}


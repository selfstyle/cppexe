#include <iostream>
#include <string>

unsigned int SDBMHash(const char *str){

  unsigned int hash = 0;
  while (*str){
  hash = (*str++) + (hash << 6) + (hash << 16) - hash;
  }
  return (hash & 0x7FFFFFFF);
}

int main(){

  std::string s("hello");

  std::cout << SDBMHash(s.c_str()) << std::endl;
  return 0;
}


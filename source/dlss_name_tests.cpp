#include "dlss_name.h"
#include <cassert>
int main(){for(int n=0;n<1000;n++){std::wstring path((size_t)n,L'x');assert(dlssExecutableName(path)==std::string((size_t)n,'x'));}assert(dlssExecutableName(L"witcher3.exe")=="witcher3.exe");}

#include <windows.h>
#include <string>
#include <cassert>
#include "i18n.h"
#define SERVERINFO_NO_UI
#include "server_badge.h"
int main(){
 assert(serverinfo::withinDisplayTime(100000,90000,0,"loading"));
 assert(serverinfo::withinDisplayTime(109999,90000,100000,"started"));
 assert(!serverinfo::withinDisplayTime(110000,90000,100000,"started"));
 assert(!serverinfo::withinDisplayTime(100000,90000,0,"ended"));
 assert(!serverinfo::withinDisplayTime(700000,100000,0,"loading"));
}

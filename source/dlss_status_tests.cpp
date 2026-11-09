#include "dlss_status.h"
#include <cassert>
int main(){using namespace dlssstatus;
 assert(classify(false,false,false,false,false)==State::Absent);
 assert(classify(true,false,false,false,false)==State::Installed);
 assert(classify(true,true,true,false,false)==State::Restart);
 assert(classify(true,true,false,true,false)==State::Active);
 assert(classify(true,true,false,false,true)==State::Waiting);
 assert(logState("NR-VERDICT v3 state=ENGAGED\nNR-VERDICT v3 state=OFF\n")==Log::Off);
 assert(logState("DlssNr_Dx12::Dispatch DLSS-NR cost: 1 ms\n")==Log::Active);
 assert(logState("random DLSS text\n")==Log::Unknown);
}

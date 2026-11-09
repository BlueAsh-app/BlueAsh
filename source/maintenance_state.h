#pragma once
// Ignore failed/stale reads. Only an explicit return to normal ends maintenance.
struct MaintenanceState {
 int current=-1;bool awaitingEnd=false;
 int accept(int state,bool fresh){
  if(!fresh||state<0||state>3){current=-1;return 0;}
  if(state==1){bool announce=!awaitingEnd;awaitingEnd=true;current=state;return announce?1:0;}
  if(state==0){bool announce=awaitingEnd;awaitingEnd=false;current=state;return announce?2:0;}
  current=state;return 0;
 }
};

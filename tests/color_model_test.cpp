#include "../src/color_model.h"
#include <cassert>
#include <cmath>
int main(){
 int v[CONTROL_COUNT]{};
 assert(decodeProfile(L"100,100,0,100,110,90,100",v));
 assert(v[4]==110 && v[5]==90 && v[7]==50);
 assert(decodeProfile(L"100,100,0,100,110,90,100,80",v) && v[7]==80);
 assert(!decodeProfile(L"100,100,0,100,110,90,100,101",v));
 assert(!decodeProfile(L"100,100,0,100,110,90,100,",v));
 assert(!decodeProfile(L"100,100,0,100,110,90,100,80,1",v));
 for(int c=0;c<3;c++)assert(temperatureGain(50,c)==1.0);
 assert(temperatureGain(0,0)<temperatureGain(50,0));
 assert(temperatureGain(0,2)==1.0);
 assert(temperatureGain(100,2)<temperatureGain(50,2));
 assert(temperatureGain(100,0)==1.0);
}

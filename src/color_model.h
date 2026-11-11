#pragma once
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>

constexpr int CONTROL_COUNT = 8;
constexpr int TEMPERATURE_DEFAULT = 50;

// Seven-field profiles from 1.2 get a neutral temperature.
inline bool decodeProfile(const std::wstring &text, int *values) {
 int v[CONTROL_COUNT]{};
 int used=0;
 int count=std::swscanf(text.c_str(),L"%d,%d,%d,%d,%d,%d,%d%n",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&used);
 if(count!=7 || used<=0) return false;
 const wchar_t *rest=text.c_str()+used;
 if(*rest==L',') {
  int length=0;
  if(std::swscanf(rest,L",%d%n",&v[7],&length)!=1 || length<=0) return false;
  rest+=length;
 } else v[7]=TEMPERATURE_DEFAULT;
 while(*rest && std::iswspace(*rest)) ++rest;
 if(*rest) return false;
 for(int i=0;i<CONTROL_COUNT;i++) {
  int lo=i==2||i==7?0:50;
  int hi=i==2?30:(i==7?100:150);
  if(v[i]<lo || v[i]>hi) return false;
  values[i]=v[i];
 }
 return true;
}

// Relative visual tint, not an absolute Kelvin measurement.
inline double temperatureGain(int temperature, int channel) {
 const double t=(std::clamp(temperature,0,100)-50)/50.0;
 if(channel==0 && t<0) return 1.0+0.25*t;
 if(channel==2 && t>0) return 1.0-0.30*t;
 if(channel==1) return 1.0-0.04*(t<0?-t:t);
 return 1.0;
}

// Windows GDI documents a maximum distance of 32768 from the identity ramp.
inline bool withinGdiRange(unsigned index, unsigned value) {
 const int delta=static_cast<int>(value)-static_cast<int>(index*257u);
 return delta >= -32768 && delta <= 32768;
}

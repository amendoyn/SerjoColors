#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <powrprof.h>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include "updater.h"
#include "color_model.h"

struct Monitor { HMONITOR handle; std::wstring device, name, key; WORD original[3][256]; bool haveOriginal = false; };
static std::vector<Monitor> monitors;
static HWND mainWindow, monitorBox, profileBox, statusText, startupCheck, sliders[CONTROL_COUNT], values[CONTROL_COUNT], applyButton, saveButton;
static HFONT font, titleFont;
static HBRUSH background;
static std::wstring iniPath, dataDir;
static std::map<std::wstring, std::vector<WORD>> baselines;
struct Draft {std::array<int,CONTROL_COUNT> values{};std::wstring profile;bool hasBefore=false;WORD before[3][256]{};};
static std::map<std::wstring,Draft> drafts;
static std::wstring lastLiveKey,lastLiveValues;
static int selected = -1;
static bool comparisonReady=false, comparing=false, liveTimer=false, checkingUpdate=false;
static WORD comparisonRamp[3][256];
static std::wstring comparisonKey;
static bool updating = false;
static NOTIFYICONDATAW tray{};
static const UINT trayMessage = WM_APP + 1;
static const wchar_t *labels[] = {L"Brilho da imagem",L"Contraste da imagem",L"Profundidade de preto",L"Gama",L"Vermelho",L"Verde",L"Azul",L"Temp.  frio ↔ quente"};
static const int defaults[] = {100,100,0,100,100,100,100,TEMPERATURE_DEFAULT};
enum { ID_MONITOR=101, ID_PROFILE, ID_NEW, ID_SAVE, ID_DELETE, ID_PREVIEW, ID_REVERT, ID_STARTUP, ID_REFRESH, ID_APPLY, ID_UPDATE, ID_SLIDER=200 };
static HWND control(const wchar_t* cls,const wchar_t* txt,DWORD style,int x,int y,int w,int h,int id=0) {
 HWND c=CreateWindowExW(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,mainWindow,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr); SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE); return c;
}
static void status(const std::wstring &s) {SetWindowTextW(statusText,s.c_str());}
static std::wstring hashKey(const std::wstring &s) {uint64_t h=14695981039346656037ULL; for(wchar_t c:s){h^=(uint16_t)c;h*=1099511628211ULL;} wchar_t b[32]; swprintf(b,32,L"M%016llX",(unsigned long long)h);return b;}
static std::wstring section() {return selected>=0?L"Profiles_"+monitors[selected].key:L"";}
static std::wstring readIni(const std::wstring &sec,const std::wstring &key,const wchar_t *fallback=L"") { wchar_t b[4096]; GetPrivateProfileStringW(sec.c_str(),key.c_str(),fallback,b,4096,iniPath.c_str()); return b; }
static bool writeIni(const std::wstring &sec,const std::wstring &key,const std::wstring &v) {return WritePrivateProfileStringW(sec.c_str(),key.c_str(),v.c_str(),iniPath.c_str())!=FALSE;}
static HDC monitorDC(const std::wstring &device){return CreateDCW(L"DISPLAY",device.c_str(),nullptr,nullptr);}
static bool isSDR(const std::wstring &device){
 UINT32 pc=0,mc=0; if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pc,&mc)!=ERROR_SUCCESS)return false;
 std::vector<DISPLAYCONFIG_PATH_INFO> paths(pc);std::vector<DISPLAYCONFIG_MODE_INFO> modes(mc);
 if(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pc,paths.data(),&mc,modes.data(),nullptr)!=ERROR_SUCCESS)return false;
 bool found=false;
 for(UINT32 i=0;i<pc;i++) {DISPLAYCONFIG_SOURCE_DEVICE_NAME src{};src.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;src.header.size=sizeof(src);src.header.adapterId=paths[i].sourceInfo.adapterId;src.header.id=paths[i].sourceInfo.id;
  if(DisplayConfigGetDeviceInfo(&src.header)!=ERROR_SUCCESS)return false;
  if(device!=src.viewGdiDeviceName) continue;
  found=true;
  DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO info{};info.header.type=DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;info.header.size=sizeof(info);info.header.adapterId=paths[i].targetInfo.adapterId;info.header.id=paths[i].targetInfo.id;
  if(DisplayConfigGetDeviceInfo(&info.header)!=ERROR_SUCCESS||info.advancedColorEnabled)return false;
 }
 return found;
}
struct RampFile {uint32_t magic; uint32_t checksum; WORD values[3][256];};
static uint32_t rampChecksum(const WORD (&r)[3][256]){uint32_t h=2166136261u;for(int c=0;c<3;c++)for(int i=0;i<256;i++){h^=r[c][i];h*=16777619u;}return h;}
static bool persistBaseline(const std::wstring &key,WORD (&ramp)[3][256]){
 std::wstring path=dataDir+L"\\"+key+L".ramp";RampFile data{};
 HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(f!=INVALID_HANDLE_VALUE){DWORD size=GetFileSize(f,nullptr),n=0;bool ok=size==sizeof(data)&&ReadFile(f,&data,sizeof(data),&n,nullptr)&&n==sizeof(data)&&data.magic==0x31524353&&data.checksum==rampChecksum(data.values);CloseHandle(f);if(ok)memcpy(ramp,data.values,sizeof(ramp));return ok;}
 f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
 data.magic=0x31524353;memcpy(data.values,ramp,sizeof(ramp));data.checksum=rampChecksum(data.values);
 DWORD n=0;bool ok=WriteFile(f,&data,sizeof(data),&n,nullptr)&&n==sizeof(data)&&FlushFileBuffers(f);CloseHandle(f);if(!ok)DeleteFileW(path.c_str());return ok;
}
static bool getRamp(const std::wstring &device,WORD (&ramp)[3][256]){ HDC dc=monitorDC(device); if(!dc)return false; BOOL ok=GetDeviceGammaRamp(dc,ramp); DeleteDC(dc);return ok!=FALSE; }
static bool setRamp(const std::wstring &device,const WORD (&ramp)[3][256]) { if(!isSDR(device))return false;HDC dc=monitorDC(device);if(!dc)return false;BOOL ok=SetDeviceGammaRamp(dc,(void*)ramp);DeleteDC(dc);if(ok){lastLiveKey.clear();lastLiveValues.clear();}return ok!=FALSE; }
static BOOL CALLBACK collect(HMONITOR h,HDC,LPRECT,LPARAM) {
 MONITORINFOEXW mi{};mi.cbSize=sizeof(mi);if(!GetMonitorInfoW(h,&mi))return TRUE;
 DISPLAY_DEVICEW dd{};dd.cb=sizeof(dd); EnumDisplayDevicesW(mi.szDevice,0,&dd,EDD_GET_DEVICE_INTERFACE_NAME);
 std::wstring identity=dd.DeviceID[0]?dd.DeviceID:mi.szDevice;
 Monitor m{};m.handle=h;m.device=mi.szDevice;m.key=dd.DeviceID[0]?hashKey(identity):L"";m.name=dd.DeviceString[0]?dd.DeviceString:L"Monitor";
 m.name += (mi.dwFlags & MONITORINFOF_PRIMARY)? L"  •  Principal":L"  •  Externo / secundário";
 if(m.key.empty()){m.name+=L"  •  identificação indisponível";monitors.push_back(m);return TRUE;}
 auto it=baselines.find(m.key);
 if(it!=baselines.end() && it->second.size()==768){memcpy(m.original,it->second.data(),sizeof(m.original));m.haveOriginal=true;}
 else if(isSDR(m.device)&&getRamp(m.device,m.original)&&persistBaseline(m.key,m.original)){m.haveOriginal=true;baselines[m.key]=std::vector<WORD>(&m.original[0][0],&m.original[0][0]+768);}
 monitors.push_back(m);return TRUE;
}
static bool sameMonitor(const Monitor &m){DISPLAY_DEVICEW d{};d.cb=sizeof(d);return !m.key.empty()&&EnumDisplayDevicesW(m.device.c_str(),0,&d,EDD_GET_DEVICE_INTERFACE_NAME)&&d.DeviceID[0]&&hashKey(d.DeviceID)==m.key;}
static bool applyCurrent(bool force=false);
static void finishComparison(){if(!comparing)return;comparing=false;if(selected>=0&&monitors[selected].key==comparisonKey){if(!applyCurrent())status(L"Não foi possível restaurar o ajuste atual.");}}
static void resetComparison(){KillTimer(mainWindow,3);liveTimer=false;finishComparison();comparisonReady=false;comparisonKey.clear();}
static bool ensureComparison(){if(comparisonReady&&selected>=0&&monitors[selected].key==comparisonKey)return sameMonitor(monitors[selected]);if(selected<0||monitors[selected].key.empty()||!monitors[selected].haveOriginal||!sameMonitor(monitors[selected]))return false;if(!getRamp(monitors[selected].device,comparisonRamp))return false;comparisonKey=monitors[selected].key;comparisonReady=true;return true;}
static LRESULT CALLBACK compareProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
 if(msg==WM_LBUTTONDOWN){if(ensureComparison()&&selected>=0){KillTimer(mainWindow,3);liveTimer=false;if(setRamp(monitors[selected].device,comparisonRamp)){comparing=true;SetCapture(h);status(L"Comparando com a imagem anterior. Solte para voltar ao ajuste.");}else status(L"Não foi possível comparar nesta tela.");}return 0;}
 if(msg==WM_LBUTTONUP){if(GetCapture()==h)ReleaseCapture();finishComparison();return 0;}
 if(msg==WM_KEYDOWN&&wp==VK_SPACE){if(!(lp&(1LL<<30)))SendMessageW(h,WM_LBUTTONDOWN,0,0);return 0;}
 if(msg==WM_KEYUP&&wp==VK_SPACE){SendMessageW(h,WM_LBUTTONUP,0,0);return 0;}
 if(msg==WM_KILLFOCUS){finishComparison();return DefSubclassProc(h,msg,wp,lp);}
 if(msg==WM_CAPTURECHANGED||msg==WM_CANCELMODE){finishComparison();return 0;}
 return DefSubclassProc(h,msg,wp,lp);
}
static void updateValues(){for(int i=0;i<CONTROL_COUNT;i++){int v=(int)SendMessageW(sliders[i],TBM_GETPOS,0,0);wchar_t b[24];if(i==7)swprintf(b,24,L"%+d",v-50);else swprintf(b,24,L"%d",v);SetWindowTextW(values[i],b);}}
static void setValues(const int *a){updating=true;for(int i=0;i<CONTROL_COUNT;i++)SendMessageW(sliders[i],TBM_SETPOS,TRUE,a[i]);updating=false;updateValues();}
static std::wstring profileName(){wchar_t b[128]{};GetWindowTextW(profileBox,b,128);return b;}
static bool decode(const std::wstring &s,int *v){return decodeProfile(s,v);}
static std::wstring encode(){std::wstring result;for(int i=0;i<CONTROL_COUNT;i++){if(i)result+=L",";result+=std::to_wstring(SendMessageW(sliders[i],TBM_GETPOS,0,0));}return result;}
static bool savedValuesMatch(const std::wstring &profile){int v[CONTROL_COUNT];if(!decode(readIni(section(),profile),v))return false;for(int i=0;i<CONTROL_COUNT;i++)if(v[i]!=(int)SendMessageW(sliders[i],TBM_GETPOS,0,0))return false;return true;}
static bool makeRamp(WORD (&out)[3][256]) {
 if(selected<0||!monitors[selected].haveOriginal)return false;
 int v[CONTROL_COUNT];if(!decode(encode(),v))return false;
 for(int c=0;c<3;c++)for(int i=0;i<256;i++){
  double x=i/255.0; x=(x-0.5)*(v[1]/100.0)+0.5; x=std::clamp(x,0.0,1.0); x=pow(x,100.0/v[0])+v[2]/100.0; x=std::clamp(x,0.0,1.0);
  x=pow(x,100.0/v[3]);x=std::clamp(x*(v[4+c]/100.0)*temperatureGain(v[7],c),0.0,1.0);
  double pos=x*255;int low=(int)pos,high=std::min(255,low+1);double fraction=pos-low;
  out[c][i]=(WORD)std::lround(monitors[selected].original[c][low]*(1-fraction)+monitors[selected].original[c][high]*fraction);
 }
 return true;
}
static bool applyCurrent(bool force){
 if(selected<0||!sameMonitor(monitors[selected]))return false;
 std::wstring valuesNow=encode(),key=monitors[selected].key;
 if(!force && lastLiveKey==key && lastLiveValues==valuesNow)return true;
 WORD ramp[3][256];if(!makeRamp(ramp)||!setRamp(monitors[selected].device,ramp))return false;
 lastLiveKey=key;lastLiveValues=valuesNow;return true;
}
static void rememberDraft(){
 if(selected<0||monitors[selected].key.empty())return;
 Draft &d=drafts[monitors[selected].key];
 for(int i=0;i<CONTROL_COUNT;i++)d.values[i]=(int)SendMessageW(sliders[i],TBM_GETPOS,0,0);
 d.profile=profileName();d.hasBefore=comparisonReady&&comparisonKey==monitors[selected].key;
 if(d.hasBefore)memcpy(d.before,comparisonRamp,sizeof(d.before));
}
static bool restoreDraft(){
 if(selected<0)return false;
 auto it=drafts.find(monitors[selected].key);if(it==drafts.end())return false;
 Draft &d=it->second;setValues(d.values.data());SetWindowTextW(profileBox,d.profile.c_str());
 comparisonReady=d.hasBefore;comparisonKey=d.hasBefore?monitors[selected].key:L"";
 if(d.hasBefore)memcpy(comparisonRamp,d.before,sizeof(comparisonRamp));
 return true;
}
static void loadProfile(){if(selected<0)return;std::wstring p=profileName();int v[CONTROL_COUNT]; if(p==L"Padrão"||!decode(readIni(section(),p),v))std::copy(defaults,defaults+CONTROL_COUNT,v);setValues(v);status(L"Ajustes ao vivo. Segure Comparar para ver a imagem anterior.");}
static void fillProfiles(){SendMessageW(profileBox,CB_RESETCONTENT,0,0);SendMessageW(profileBox,CB_ADDSTRING,0,(LPARAM)L"Padrão");wchar_t buf[32768]{};GetPrivateProfileSectionW(section().c_str(),buf,32768,iniPath.c_str());for(wchar_t *p=buf;*p;p+=wcslen(p)+1){wchar_t *eq=wcschr(p,L'=');if(eq){std::wstring key(p,eq-p);if(!key.empty()&&key!=L"Padrão")SendMessageW(profileBox,CB_ADDSTRING,0,(LPARAM)key.c_str());}}
 std::wstring active=readIni(L"Active",monitors[selected].key,L"Padrão");LRESULT ix=SendMessageW(profileBox,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)active.c_str());SendMessageW(profileBox,CB_SETCURSEL,ix==CB_ERR?0:ix,0);loadProfile();}
static void selectMonitor(){resetComparison();selected=(int)SendMessageW(monitorBox,CB_GETCURSEL,0,0);if(selected<0||selected>=(int)monitors.size()){selected=-1;status(L"Nenhum monitor encontrado.");return;}if(monitors[selected].key.empty()){SendMessageW(profileBox,CB_RESETCONTENT,0,0);setValues(defaults);status(L"Identificação indisponível; ajustes e perfis bloqueados nesta tela.");return;}fillProfiles();if(restoreDraft()&&!applyCurrent())status(L"Não foi possível restaurar o ajuste em andamento nesta tela.");}
static void enumerate(bool preserve=true){std::wstring old=selected>=0&&selected<(int)monitors.size()?monitors[selected].key:L"";resetComparison();monitors.clear();selected=-1;EnumDisplayMonitors(nullptr,nullptr,collect,0);SendMessageW(monitorBox,CB_RESETCONTENT,0,0);int chosen=0;for(size_t i=0;i<monitors.size();i++){SendMessageW(monitorBox,CB_ADDSTRING,0,(LPARAM)monitors[i].name.c_str());if(preserve&&monitors[i].key==old)chosen=(int)i;}if(!monitors.empty()){SendMessageW(monitorBox,CB_SETCURSEL,chosen,0);selectMonitor();}else status(L"Nenhum monitor encontrado.");}
static bool validProfile(const std::wstring &p){return !p.empty()&&p.size()<60&&p!=L"Padrão"&&p.find_first_of(L"[]=\r\n") == std::wstring::npos&&p.front()!=L' '&&p.back()!=L' ';}
static bool startupEnabled(){HKEY k;wchar_t value[1024]{};DWORD n=sizeof(value),type=0;if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&k)!=ERROR_SUCCESS)return false;LONG r=RegQueryValueExW(k,L"SerjoColors",nullptr,&type,(BYTE*)value,&n);RegCloseKey(k);return r==ERROR_SUCCESS;}
static bool setStartup(bool enable){HKEY k;if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return false;LONG r;if(enable){wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);std::wstring cmd=L"\""+std::wstring(path)+L"\" --startup";r=RegSetValueExW(k,L"SerjoColors",0,REG_SZ,(BYTE*)cmd.c_str(),(DWORD)((cmd.size()+1)*sizeof(wchar_t)));}else{r=RegDeleteValueW(k,L"SerjoColors");if(r==ERROR_FILE_NOT_FOUND)r=ERROR_SUCCESS;}RegCloseKey(k);return r==ERROR_SUCCESS;}
static void applySaved(bool onlyDrafts=false){int old=selected;for(size_t i=0;i<monitors.size();i++){selected=(int)i;if(monitors[i].key.empty())continue;auto draft=drafts.find(monitors[i].key);if(draft!=drafts.end()){setValues(draft->second.values.data());applyCurrent(true);continue;}if(onlyDrafts)continue;std::wstring p=readIni(L"Active",monitors[i].key,L"Padrão");int v[CONTROL_COUNT];if(p!=L"Padrão"&&decode(readIni(section(),p),v)){setValues(v);applyCurrent();}}selected=old;if(selected>=0){fillProfiles();restoreDraft();}}
static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_CREATE:{mainWindow=h; font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");titleFont=CreateFontW(-27,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); background=CreateSolidBrush(RGB(248,250,253)); HWND title=control(L"STATIC",L"SerjoColors  " SERJO_VERSION_TEXT,0,24,16,400,38);SendMessageW(title,WM_SETFONT,(WPARAM)titleFont,TRUE);
 control(L"STATIC",L"Monitor",0,24,66,110,23);monitorBox=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,24,90,360,240,ID_MONITOR);control(L"BUTTON",L"Atualizar",WS_TABSTOP,396,89,105,30,ID_REFRESH);
 control(L"STATIC",L"Perfil",0,24,132,100,22);profileBox=control(WC_COMBOBOXW,L"",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,24,156,255,220,ID_PROFILE);SendMessageW(profileBox,CB_LIMITTEXT,59,0);control(L"BUTTON",L"Novo",WS_TABSTOP,288,155,72,30,ID_NEW);saveButton=control(L"BUTTON",L"Salvar",WS_TABSTOP,366,155,72,30,ID_SAVE);control(L"BUTTON",L"Excluir",WS_TABSTOP,444,155,72,30,ID_DELETE);
 control(L"STATIC",L"AJUSTES DE IMAGEM  ·  experimental / SDR",0,24,205,490,23);
 for(int i=0;i<CONTROL_COUNT;i++){int y=228+i*28;control(L"STATIC",labels[i],0,24,y,195,24);sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,217,y-3,244,32,ID_SLIDER+i);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(i==2||i==7?0:50,i==2?30:(i==7?100:150)));values[i]=control(L"STATIC",L"",SS_RIGHT,466,y,48,24);}
 control(L"STATIC",L"Saturação e nitidez: indisponíveis",0,24,459,480,22);
 applyButton=control(L"BUTTON",L"Segure para comparar",WS_TABSTOP,24,487,210,34,ID_PREVIEW);SetWindowSubclass(applyButton,compareProc,1,0);control(L"BUTTON",L"Restaurar",WS_TABSTOP,244,487,110,34,ID_REVERT);control(L"BUTTON",L"Aplicar",WS_TABSTOP,364,487,145,34,ID_APPLY);
 startupCheck=control(L"BUTTON",L"Iniciar com o Windows e aplicar perfis",BS_AUTOCHECKBOX|WS_TABSTOP,24,531,440,26,ID_STARTUP);SendMessageW(startupCheck,BM_SETCHECK,startupEnabled()?BST_CHECKED:BST_UNCHECKED,0);
 control(L"BUTTON",L"Verificar atualização",WS_TABSTOP,310,16,200,34,ID_UPDATE);statusText=control(L"STATIC",L"",0,24,561,500,48);tray.cbSize=sizeof(tray);tray.hWnd=h;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=trayMessage;tray.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wcscpy(tray.szTip,L"SerjoColors");Shell_NotifyIconW(NIM_ADD,&tray);enumerate(false);return 0;}
 case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:SetBkColor((HDC)wp,RGB(248,250,253));SetTextColor((HDC)wp,RGB(34,44,60));return (LRESULT)background;
 case WM_HSCROLL:if(!updating){updateValues();if(!comparing&&ensureComparison()){rememberDraft();if(!liveTimer)liveTimer=SetTimer(h,3,220,nullptr)!=0;}}return 0;
 case WM_COMMAND:{int id=LOWORD(wp),code=HIWORD(wp);if(id==ID_MONITOR&&code==CBN_SELCHANGE)selectMonitor();else if(id==ID_PROFILE&&code==CBN_SELCHANGE){bool ready=ensureComparison();loadProfile();if(ready){rememberDraft();if(!applyCurrent())status(L"Não foi possível mostrar o perfil nesta tela.");}}else if(id==ID_REFRESH)enumerate();else if(id==ID_UPDATE&&!checkingUpdate){checkingUpdate=true;status(L"Consultando atualizações no GitHub...");HANDLE thread=CreateThread(nullptr,0,checkUpdateThread,h,0,nullptr);if(thread)CloseHandle(thread);else{checkingUpdate=false;status(L"Não foi possível iniciar a verificação.");}}else if(id==ID_STARTUP){bool on=SendMessageW(startupCheck,BM_GETCHECK,0,0)==BST_CHECKED;if(!setStartup(on)){SendMessageW(startupCheck,BM_SETCHECK,on?BST_UNCHECKED:BST_CHECKED,0);status(L"Não foi possível alterar a inicialização automática.");}else status(on?L"Inicialização automática ativada para este usuário.":L"Inicialização automática desativada.");}
 else if(id==ID_NEW){resetComparison();bool ready=ensureComparison();SetWindowTextW(profileBox,L"");SetFocus(profileBox);setValues(defaults);if(ready)rememberDraft();if(ready&&!applyCurrent())status(L"Não foi possível mostrar os ajustes iniciais nesta tela.");else status(L"Digite um nome no campo Perfil, ajuste e clique em Salvar.");}
 else if(id==ID_SAVE&&selected>=0&&!monitors[selected].key.empty()){std::wstring p=profileName();if(!validProfile(p)){status(L"Digite um nome válido para o perfil (diferente de Padrão).");break;}if(!writeIni(section(),p,encode())){status(L"Não foi possível salvar o perfil; seus ajustes continuam na tela.");break;}fillProfiles();LRESULT found=SendMessageW(profileBox,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)p.c_str());if(found!=CB_ERR)SendMessageW(profileBox,CB_SETCURSEL,found,0);loadProfile();rememberDraft();if(!applyCurrent())status(L"Perfil salvo, mas não foi possível mostrar o ajuste.");else status(L"Perfil salvo. Clique em Aplicar para usar na inicialização.");}
 else if(id==ID_DELETE&&selected>=0&&!monitors[selected].key.empty()){std::wstring p=profileName();if(validProfile(p)&&MessageBoxW(h,L"Excluir este perfil?",L"SerjoColors",MB_YESNO|MB_ICONQUESTION)==IDYES){if(readIni(L"Active",monitors[selected].key,L"Padrão")==p){status(L"Restaure o padrão antes de excluir o perfil ativo.");break;}if(!WritePrivateProfileStringW(section().c_str(),p.c_str(),nullptr,iniPath.c_str())){status(L"Não foi possível excluir o perfil.");break;}auto d=drafts.find(monitors[selected].key);if(d!=drafts.end()&&d->second.profile==p)drafts.erase(d);resetComparison();fillProfiles();if(!applyCurrent(true))status(L"Perfil excluído, mas não foi possível restaurar a imagem.");}}
 else if(id==ID_APPLY&&selected>=0&&!monitors[selected].key.empty()){resetComparison();std::wstring p=profileName();if(p!=L"Padrão"&&!validProfile(p)){status(L"Selecione ou salve um perfil primeiro.");break;}if(p==L"Padrão"){if(monitors[selected].haveOriginal&&sameMonitor(monitors[selected])&&setRamp(monitors[selected].device,monitors[selected].original)){if(writeIni(L"Active",monitors[selected].key,L"Padrão")){drafts.erase(monitors[selected].key);status(L"Padrão solicitado. Verifique o resultado na tela.");}else status(L"Imagem alterada, mas perfil ativo não foi salvo. Desative o início automático.");}else status(L"Não foi possível restaurar esta tela.");}else if(!savedValuesMatch(p)){status(L"Salve as alterações no perfil antes de aplicar.");}else if(applyCurrent(true)){if(writeIni(L"Active",monitors[selected].key,p)){drafts.erase(monitors[selected].key);status(L"Perfil aplicado. Confira o resultado na tela.");}else status(L"Imagem alterada, mas perfil ativo não foi salvo. Desative o início automático.");}else status(L"O driver recusou este ajuste.");}
 else if(id==ID_REVERT&&selected>=0&&!monitors[selected].key.empty()){resetComparison();if(monitors[selected].haveOriginal&&sameMonitor(monitors[selected])&&setRamp(monitors[selected].device,monitors[selected].original)){setValues(defaults);SendMessageW(profileBox,CB_SETCURSEL,0,0);if(writeIni(L"Active",monitors[selected].key,L"Padrão")){drafts.erase(monitors[selected].key);status(L"Referência inicial restaurada (verifique na tela).");}else status(L"Imagem alterada, mas perfil ativo não foi salvo. Desative o início automático.");}else status(L"Não foi possível restaurar a configuração desta tela.");}
 return 0;}
 case WM_TIMER:if(wp==3){KillTimer(h,3);liveTimer=false;if(!comparing&&!applyCurrent())status(L"O driver não aplicou o ajuste. Verifique SDR e o suporte da tela.");}else if(wp==2){KillTimer(h,2);enumerate();applySaved(!startupEnabled());}return 0;
 case UPDATE_RESULT:{checkingUpdate=false;UpdateResult *r=(UpdateResult*)lp;if(r){if(r->kind==2){if(MessageBoxW(h,r->message.c_str(),L"Atualização do SerjoColors",MB_YESNO|MB_ICONQUESTION)==IDYES){resetComparison();if(startUpdateHelper(r->staged,dataDir,r->expectedHash)){delete r;DestroyWindow(h);return 0;}status(L"Não foi possível iniciar o atualizador.");}else DeleteFileW(r->staged.c_str());}else MessageBoxW(h,r->message.c_str(),L"Atualização do SerjoColors",MB_OK|(r->kind?MB_ICONINFORMATION:MB_ICONWARNING));delete r;}return 0;}
 case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE)finishComparison();return 0;
 case WM_DISPLAYCHANGE: lastLiveKey.clear();lastLiveValues.clear();KillTimer(h,3);liveTimer=false;comparing=false;comparisonReady=false;comparisonKey.clear();if(GetCapture()==applyButton)ReleaseCapture();SetTimer(h,2,2000,nullptr);return 0;
 case WM_POWERBROADCAST:if(wp==PBT_APMRESUMEAUTOMATIC||wp==PBT_APMRESUMESUSPEND){lastLiveKey.clear();lastLiveValues.clear();SetTimer(h,2,3000,nullptr);}return TRUE;
 case WM_APP+1:if(lp==WM_LBUTTONDBLCLK){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}else if(lp==WM_RBUTTONUP){POINT pt;GetCursorPos(&pt);HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Abrir SerjoColors");AppendMenuW(menu,MF_STRING,2,L"Sair");SetForegroundWindow(h);int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,h,nullptr);DestroyMenu(menu);if(choice==1){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}else if(choice==2){resetComparison();DestroyWindow(h);}}return 0;
 case WM_CLOSE:resetComparison();ShowWindow(h,SW_HIDE);return 0;
 case WM_DESTROY:Shell_NotifyIconW(NIM_DELETE,&tray);DeleteObject(background);DeleteObject(font);DeleteObject(titleFont);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,LPWSTR cmd,int show){int argc=0;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argv&&argc>1&&wcscmp(argv[1],L"--update-helper")==0){int result=runUpdateHelper(argc,argv);LocalFree(argv);return result;}if(argv)LocalFree(argv);HWND existing=FindWindowW(L"SerjoColorsWindow",nullptr);if(existing){if(!(cmd&&wcsstr(cmd,L"--startup"))){ShowWindow(existing,SW_SHOW);SetForegroundWindow(existing);}return 0;}InitCommonControls();wchar_t local[MAX_PATH];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);if(!n||n>=MAX_PATH){MessageBoxW(nullptr,L"Não foi possível encontrar a pasta de configurações.",L"SerjoColors",MB_OK|MB_ICONERROR);return 1;}std::wstring dir=std::wstring(local)+L"\\SerjoColors";CreateDirectoryW(dir.c_str(),nullptr);dataDir=dir;iniPath=dir+L"\\profiles.ini";if(GetFileAttributesW(iniPath.c_str())==INVALID_FILE_ATTRIBUTES){HANDLE f=CreateFileW(iniPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE){WORD bom=0xFEFF;DWORD written;WriteFile(f,&bom,2,&written,nullptr);CloseHandle(f);}}
 WNDCLASSW wc{};wc.lpfnWndProc=wndproc;wc.hInstance=inst;wc.lpszClassName=L"SerjoColorsWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassW(&wc);HWND h=CreateWindowExW(0,wc.lpszClassName,L"SerjoColors",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,555,660,nullptr,nullptr,inst,nullptr);if(!h)return 1;bool startup=cmd&&wcsstr(cmd,L"--startup");if(startup){applySaved();ShowWindow(h,SW_HIDE);}else ShowWindow(h,show);UpdateWindow(h);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}return 0;}

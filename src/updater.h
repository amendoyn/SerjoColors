#pragma once
#include <windows.h>
#include "version.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdio>

// Manifest: major.minor followed by SHA-256, both on separate lines.
static const wchar_t *UPDATE_HOST=L"raw.githubusercontent.com";
static constexpr unsigned VERSION_MAJOR=SERJO_VERSION_MAJOR, VERSION_MINOR=SERJO_VERSION_MINOR;
static constexpr UINT UPDATE_RESULT=WM_APP+10;
struct UpdateResult {int kind=0; std::wstring message, staged, version, expectedHash;};
static bool fetchUpdate(const wchar_t *path,std::vector<unsigned char> &out,size_t limit){
 HINTERNET session=WinHttpOpen(L"SerjoColors/" SERJO_VERSION_TEXT,WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session)return false;
 WinHttpSetTimeouts(session,10000,10000,15000,15000);
 HINTERNET conn=WinHttpConnect(session,UPDATE_HOST,INTERNET_DEFAULT_HTTPS_PORT,0);
 HINTERNET req=conn?WinHttpOpenRequest(conn,L"GET",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH):nullptr;
 bool ok=req && WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0) && WinHttpReceiveResponse(req,nullptr);
 DWORD code=0,cb=sizeof(code);if(ok)ok=WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&cb,WINHTTP_NO_HEADER_INDEX)&&code==200;
 while(ok){DWORD avail=0;if(!WinHttpQueryDataAvailable(req,&avail)){ok=false;break;}if(!avail)break;if(out.size()+avail>limit){ok=false;break;}size_t old=out.size();out.resize(old+avail);DWORD read=0;if(!WinHttpReadData(req,out.data()+old,avail,&read)||!read){ok=false;break;}out.resize(old+read);}
 if(req)WinHttpCloseHandle(req);
 if(conn)WinHttpCloseHandle(conn);
 WinHttpCloseHandle(session);return ok;
}
static bool sha256(const std::vector<unsigned char>& data,unsigned char out[32]){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;DWORD n=0,objlen=0;bool ok=false;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 if(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&objlen,sizeof(objlen),&n,0)>=0){std::vector<unsigned char> object(objlen);if(BCryptCreateHash(alg,&hash,object.data(),objlen,nullptr,0,0)>=0){ok=BCryptHashData(hash,(PUCHAR)data.data(),(ULONG)data.size(),0)>=0&&BCryptFinishHash(hash,out,32,0)>=0;BCryptDestroyHash(hash);}}
 BCryptCloseAlgorithmProvider(alg,0);return ok;
}
static DWORD WINAPI checkUpdateThread(void *param){
 HWND hwnd=(HWND)param;UpdateResult *r=new UpdateResult;std::vector<unsigned char> meta;
 if(!fetchUpdate(L"/amendoyn/SerjoColors/main/version.txt",meta,1024)){r->message=L"Não foi possível consultar o GitHub. Verifique a conexão.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 std::string str(meta.begin(),meta.end());unsigned major=0,minor=0;char expected[65]{};char extra=0;
 if(sscanf(str.c_str(),"%u.%u %64[0-9a-fA-F] %c",&major,&minor,expected,&extra)!=3||strlen(expected)!=64||major>999||minor>999){r->message=L"O arquivo de versão do repositório é inválido.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 if(major<VERSION_MAJOR||(major==VERSION_MAJOR&&minor<=VERSION_MINOR)){r->kind=1;r->message=L"Você já está usando a versão mais recente disponível.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 std::vector<unsigned char> bytes;if(!fetchUpdate(L"/amendoyn/SerjoColors/main/dist/SerjoColors.exe",bytes,25*1024*1024)||bytes.size()<1024||bytes[0]!='M'||bytes[1]!='Z'){r->message=L"Não foi possível baixar o executável da atualização.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 unsigned char hash[32];char actual[65]{};if(!sha256(bytes,hash)){r->message=L"Não foi possível conferir a atualização.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}for(int i=0;i<32;i++)sprintf(actual+i*2,"%02x",hash[i]);
 if(_stricmp(actual,expected)!=0){r->message=L"A atualização não corresponde ao arquivo de versão; tente novamente mais tarde.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 wchar_t exe[MAX_PATH]{};if(!GetModuleFileNameW(nullptr,exe,MAX_PATH)){r->message=L"Não foi possível localizar o aplicativo.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 std::wstring stage=std::wstring(exe)+L".download";HANDLE f=CreateFileW(stage.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;bool ok=f!=INVALID_HANDLE_VALUE&&WriteFile(f,bytes.data(),(DWORD)bytes.size(),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 if(!ok){DeleteFileW(stage.c_str());r->message=L"Não foi possível salvar a atualização ao lado do aplicativo.";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;}
 wchar_t ver[32];swprintf(ver,32,L"%u.%u",major,minor);r->version=ver;r->staged=stage;for(int i=0;i<64;i++)r->expectedHash+=(wchar_t)expected[i];r->kind=2;r->message=L"Versão "+r->version+L" disponível. Atualizar e reiniciar agora?";PostMessageW(hwnd,UPDATE_RESULT,0,(LPARAM)r);return 0;
}
static bool startUpdateHelper(const std::wstring &staged,const std::wstring &dir,const std::wstring &expectedHash){
 wchar_t exe[MAX_PATH]{};if(!GetModuleFileNameW(nullptr,exe,MAX_PATH))return false;
 std::wstring helper=dir+L"\\update-helper.exe";if(!CopyFileW(exe,helper.c_str(),FALSE))return false;
 std::wstring args=L"\""+helper+L"\" --update-helper \""+exe+L"\" \""+staged+L"\" "+std::to_wstring(GetCurrentProcessId())+L" "+expectedHash;
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};std::vector<wchar_t> cmd(args.begin(),args.end());cmd.push_back(0);
 if(!CreateProcessW(helper.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return false;
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return true;
}
static int runUpdateHelper(int argc,wchar_t **argv){
 if(argc!=6)return 1;
 DWORD pid=wcstoul(argv[4],nullptr,10);HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);if(parent){DWORD wait=WaitForSingleObject(parent,30000);CloseHandle(parent);if(wait!=WAIT_OBJECT_0){MessageBoxW(nullptr,L"O aplicativo anterior não encerrou. A atualização foi cancelada.",L"SerjoColors",MB_ICONERROR);return 1;}}
 std::wstring target=argv[2],stage=argv[3],backup=target+L".backup";
 if(stage!=target+L".download"||wcslen(argv[5])!=64)return 1;
 HANDLE f=CreateFileW(stage.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return 1;
 LARGE_INTEGER size{};bool ok=GetFileSizeEx(f,&size)&&size.QuadPart>=1024&&size.QuadPart<=25*1024*1024;std::vector<unsigned char> data(ok?(size_t)size.QuadPart:0);DWORD read=0;
 ok=ok&&ReadFile(f,data.data(),(DWORD)data.size(),&read,nullptr)&&read==data.size();unsigned char hash[32];ok=ok&&sha256(data,hash);wchar_t actual[65]{};if(ok)for(int i=0;i<32;i++)swprintf(actual+i*2,3,L"%02x",hash[i]);
 if(!ok||_wcsicmp(actual,argv[5])!=0){CloseHandle(f);MessageBoxW(nullptr,L"A atualização foi alterada ou está danificada. Nenhum arquivo foi substituído.",L"SerjoColors",MB_ICONERROR);return 1;}
 // Keep the validated file open without write sharing until the replacement begins.
 CloseHandle(f);
 if(!MoveFileExW(target.c_str(),backup.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){MessageBoxW(nullptr,L"Não foi possível substituir o aplicativo. A versão anterior foi preservada.",L"SerjoColors",MB_ICONERROR);return 1;}
 if(!MoveFileExW(stage.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)){bool restored=MoveFileExW(backup.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE;MessageBoxW(nullptr,restored?L"Atualização falhou; a versão anterior foi restaurada.":L"Atualização falhou. Renomeie o arquivo .backup para .exe.",L"SerjoColors",MB_ICONERROR);return 1;}
 HINSTANCE launched=ShellExecuteW(nullptr,L"open",target.c_str(),nullptr,nullptr,SW_SHOWNORMAL);if((INT_PTR)launched<=32){MessageBoxW(nullptr,L"Atualizado, mas não foi possível abrir automaticamente. Abra o aplicativo novamente.",L"SerjoColors",MB_ICONWARNING);return 1;}return 0;
}

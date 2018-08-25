// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once
#include "targetver.h"
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <tchar.h>
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit
#define _AFX_NO_MFC_CONTROLS_IN_DIALOGS         // remove support for MFC controls in dialogs

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#include <afx.h>
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>                     // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT


#include <iostream>
#include <stdlib.h>
#include <vector>
#include <random>
#include <stdint.h>
#include <string>
#include <conio.h>
#include <fstream>
#include <WinSock2.h>
#include <afxsock.h>
#include "dirent.h"
//#include <boost/filesystem/operations.hpp>
//#include <boost/filesystem/fstream.hpp>
using namespace std;

string ServerIP;

#define localhost "127.0.0.1"
//#define addrClient "192.168.1.115"  // máy mỗi người mỗi khác
#define BUFSIZE 512
#define ENTER 13
#define BACKSPACE 8
//Đường dẫn mặc định của client, cho người dùng tự nhập (do máy tính mỗi người khác nhau)
string directorLocal;

//Tạo p1, p2 cho lệnh PORT
#define LSB(x) ((x) & 0xFF)
#define MSB(x) (((x) >> 8) & 0xFF)

//kiểm tra FTP response codes
#define FTP_REPLY_CODE_1YZ(code) ((code) >= 100 && (code) < 200)
#define FTP_REPLY_CODE_2YZ(code) ((code) >= 200 && (code) < 300)
#define FTP_REPLY_CODE_3YZ(code) ((code) >= 300 && (code) < 400)
#define FTP_REPLY_CODE_4YZ(code) ((code) >= 400 && (code) < 500)
#define FTP_REPLY_CODE_5YZ(code) ((code) >= 500 && (code) < 600)	

struct _FtpClientContext
{
	bool passiveMode = 0;		// passive mode
	CSocket controlSocket;		// điều khiển kết nối
	SOCKET dataSocket;			// điểu khiển truyền dữ liệu
	char buffer[BUFSIZE + 1];
};

#define FtpClientContext struct _FtpClientContext
// TODO: reference additional headers your program requires here

// Đầu vào: FtpClientContext
// Đầu ra: Không có
// Chức năng: Đăng nhập vào server
bool Login(FtpClientContext* clnt);

// Đầu vào: FtpClientContext, loại lệnh ("dir" hay "ls"), thư mục gốc, port
// Đầu ra: Không có
// Chức năng: Liệt kê các file và folder trên Server (lệnh ls)
string List(FtpClientContext* clnt, string type, string loc, int& port);

// Đầu vào: FtpClientContext, loại lệnh ("dir" hay "ls"), thư mục gốc
// Đầu ra: Không có
// Chức năng:
string SendLIST_NLSTCommand(FtpClientContext* clnt, string type, string loc);

// Đầu vào: FtpClientContext, tên file, port
// Đầu ra: Không có
// Chức năng: Download 1 file từ server
void Get(FtpClientContext* clnt, string file, int& port);

// Đầu vào: FtpClientContext, tên file
// Đầu ra: Không có
// Chức năng: Download 1 file (Hàm dùng trong hàm get)
bool DownloadFile(FtpClientContext* clnt, string serfname, string locfname);

// Đầu vào: FtpClientContext, tên file (đường dẫn + tên file trong máy client), port
// Đầu ra: Không có
// Chức năng: Upload 1 file
void Put(FtpClientContext* clnt, string file, int& port);

// Đầu vào: FtpClientContext, tên file trong client, tên file trên server (tên của file sau khi up)
// Đầu ra: 1: thành công, 0: thất bại
// Chức năng: Upload file (Hàm dùng trong hàm Put)
bool StorFileToServer(FtpClientContext* clnt, string file, string serfname);

// Đầu vào: FtpClientContext, chuỗi các file/folder, port
// Đầu ra: Không có
// Chức năng: Download nhiều file
void MGet(FtpClientContext* clnt, string cmd, int& port);

// Đầu vào: FtpClientContext, tên file (đường dẫn + tên file trong máy client)
// Đầu ra: Không có
// Chức năng: Upload nhiều file
void MPut(FtpClientContext* clnt, string files, int& port);

// Đầu vào: đường dẫn cần liệt kê file/folder, chuỗi nhận các file/folder trong đó
// Đầu ra: 1: Không mở được đường dẫn, 0: Hoàn thành
// Chức năng: liệt kê file/folder trong mấy client
bool ListFileInClientFolder(string dirpath, string& allnames);

// Đầu vào: FtpClientContext, đường dẫn
// Đầu ra: không có
// Chức năng: Thay đổi đường dẫn trên Server (cd)
void ChangeDirector(FtpClientContext *context, string pathName);

// Đầu vào: pathname
// Đầu ra: không có
// Chức năng: thay đổi đường dẫn dưới client
void ChangeLocalDirector(string pathName);

// Đầu vào: FtpClientContext, fileName
// Đầu ra: không có
// Chức năng: xóa một file trên server
void DeleteFileOnServer(FtpClientContext *context, string fileName);

// Đầu vào: FtpClientContext, tên của file cần xóa, port client
// Đầu ra: không có
// Chức năng: xóa nhiều file trên server
void DeleteMultipleFileOnServer(FtpClientContext *context, string pathName, int& port);

// Đầu vào: FtpClientContext, pathName
// Đầu ra: không có
// Chức năng: tạo thư mục trên server (mkdir)
void MakeDir(FtpClientContext *context, string pathName);

// Đầu vào: FtpClientContext, tên thư mục (có đường dẫn)
// Đầu ra: không có
// Chức năng: Xóa thư mục rỗng
void DeleteEmptyFolder(FtpClientContext *context, string dirname);

// Đầu vào: FtpClientContext
// Đầu ra: không có
// Chức năng: Hiển thị đường dẫn hiện tại trên server
void DisplayCurrentWorkingDirectory(FtpClientContext *client);

// Đầu vào: FtpClientContext
// Đầu ra: không có
// Chức năng: Thoát khỏi server
void Quit(FtpClientContext *client);

// Đầu vào: FtpClientContext
// Đầu ra: trả về codeftp
// Chức năng: Mở kết nối active
int Active(FtpClientContext* clnt, int port);

// Đầu vào: port
// Đầu ra: NULL: tạo thất bại, SOCKET: tạo thành công
// Chức năng: Tạo socket listen cho cơ chế active
SOCKET CreateListen(int port);

// Đầu vào: FtpClientContext
// Đầu ra: 0: kết nối thất bại, 1: kết nối thành công
// Chức năng: Gửi lệnh chuyển sang passive, lấy ra port
bool Passive(CSocket& clnt, SOCKET& data);

// Đầu vào: FtpClientContext
// Đầu ra: không có
// Chức năng: Chuyển đổi qua lại giữa active và passive
void ActiveOrPassive(FtpClientContext* clnt);

// Đầu vào: CSocket, chuỗi nhận chuỗi kí tự server gửi về
// Đầu ra: Không có
// Chức năng: Nhận tin từ server, loại bỏ kí tự rỗng
uint8_t ReceivingMessage(CSocket& clnt, char* temp);

//Đầu vào: SOCKET, chuỗi server trả về
//Đầu ra: error
//Chức năng: nhận data từ server
uint8_t ReceivingData(SOCKET *clnt, char *temp);

// Đầu vào: Đường dẫn
// Đầu ra: Tên file trong đường dẫn
// Chức năng: Lấy tên file ra khỏi đường dẫn
string GetFileNameOutOfPath(string path);

// Đầu vào: Đường dẫn
// Đầu ra: Tên file trong đường dẫn
// Chức năng: Xóa tất cả dấu " (phục vụ cho mở file)
void DeleteAllDoubleQuotationMarks(string& objname);

// Đầu vào: FtpClientContext, command, reply
// Đầu ra: error code
// Chức năng: gửi command tới server, nhận tin trả lời của server
uint8_t ftpSendCommand(FtpClientContext *context, const char *command, int *replyCode);

// Đầu vào: Tên file/folder
// Đầu ra: 0: folder, 1: file
// Chức năng: Xác định đây là tên file hay folder
bool IsAFile(string name);

// Đầu vào: không có
// Đầu ra: không có
// Chức năng: Yêu cầu người dùng nhập đường dẫn mặc định
void EnterLocalDirectory();
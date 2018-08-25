// Socket.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Socket.h"


//#ifdef _DEBUG
//#define new DEBUG_NEW
//#endif


// The one and only application object

CWinApp theApp;

int main()
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			wprintf(L"Fatal Error: MFC initialization failed\n");
			nRetCode = 1;
		}
		else
		{
			// TODO: code your application's behavior here.
			if (AfxSocketInit() == FALSE)
			{
				cout << "Can't create Socket Libraray";
				return FALSE;
			}			
				
			// Tao socket dau tien
			FtpClientContext ftpClient;
			//tạo port cho socket control connection
			srand(time(NULL));
			int portClient = (rand() % 50000) + 1023;
			ftpClient.controlSocket.Create();
			
			// Ket noi den Server
			if (ftpClient.controlSocket.Connect(_T("127.0.0.1"), 21) != 0)
			{
				cout << "Successfully connected to server" << endl;
				// --- Welcome message ---
				ReceivingMessage(ftpClient.controlSocket, ftpClient.buffer);
				cout << ftpClient.buffer;
				memset(ftpClient.buffer, 0, BUFSIZE);
				// Nếu đăng nhập thất bại, ngắt kết nối, dừng chương trình
				if (Login(&ftpClient) == 0) {
					ftpClient.controlSocket.Close();
					_getch();
					return nRetCode;
				}
				// Mặc định sử dụng active
				ftpClient.passiveMode = 0;

				// cmd: cả câu lệnh nhập vào (gồm lệnh và các tham số)
				// buf: chỉ phần lệnh
				string cmd, buf;
				int i;

				EnterLocalDirectory();

				while (1) {
					cout << "ftp> ";
					getline(cin, cmd);
					for (i = 0; cmd[i] != ' ' && cmd[i] != '\0'; i++)
					{
						buf = buf + cmd[i];
					}
					cmd.erase(0, i + 1);	// xóa phần lệnh đi

					if (buf == "ls" || buf == "dir") {
						cout << List(&ftpClient, buf, cmd, portClient);
					}
					else if (buf == "get") {
						Get(&ftpClient, cmd, portClient);
					}
					else if (buf == "put") {
						Put(&ftpClient, cmd, portClient);
					}
					else if (buf == "mget") {
						MGet(&ftpClient, cmd, portClient);
					}
					else if (buf == "mput") {
						MPut(&ftpClient, cmd, portClient);
					}
					else if (buf == "cd") {
						ChangeDirector(&ftpClient, cmd);
					}
					else if (buf == "lcd") {
						ChangeLocalDirector(cmd);
					}
					else if (buf == "delete") {
						DeleteFileOnServer(&ftpClient, cmd);
					}
					else if (buf == "mdelete") {
						DeleteMultipleFileOnServer(&ftpClient, cmd, portClient);
					}
					else if (buf == "mkdir") {
						MakeDir(&ftpClient, cmd);
					}
					else if (buf == "rmdir") {
						DeleteEmptyFolder(&ftpClient, cmd);
					}
					else if (buf == "pwd") {
						DisplayCurrentWorkingDirectory(&ftpClient);
					}
					else if (buf == "style") { // Chuyển qua chuyển lại giữa pasv và actv
						ActiveOrPassive(&ftpClient);
					}
					else if (buf == "quit" || buf == "bye" || buf == "close" || buf == "disconnect") {
						Quit(&ftpClient);
						break;
					}
					else {
						cout << "invalid command" << endl;
					}
					buf = "";
				}				
			}
			else {
				cout << "Unable to connect to server" << endl;
			}
			ftpClient.controlSocket.Close();
			_getch();
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		wprintf(L"Fatal Error: GetModuleHandle failed\n");
		nRetCode = 1;
	}

	return nRetCode;
}

// stdafx.cpp : source file that includes just the standard includes
// $safeprojectname$.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

// Đầu vào: FtpClientContext
// Đầu ra: 0: đăng nhập thất bại, 1: đăng nhập thành công
// Chức năng: Đăng nhập vào server
bool Login(FtpClientContext* clnt)
{
	int i, codeftp;
	string strSend;

	// --- OPTS UTF8 ON --- (UTF8 luôn luôn bật nên không cần thiết, nếu muốn thì gửi kiểm tra cũng được)
	//string utf8;
	//utf8 = "OPTS UTF8 ON\r\n";
	//clnt.Send((char*)utf8.c_str(), utf8.length(), 0);
	//ReceivingMessage(clnt->controlSocket, clnt->buffer);
	//cout << clnt->buffer;
	//delete clnt->buffer;

	// --- Nhập username ---
	cout << "Username: ";
	cin >> strSend;
	cin.ignore();
	strSend = "USER " + strSend + "\r\n";
	clnt->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(clnt->controlSocket, clnt->buffer);
	cout << clnt->buffer;
	memset(clnt->buffer, 0, BUFSIZE);
	
	strSend.erase();
	cout << "Password: ";
	char ch = _getch();
	while (ch != ENTER)
	{
		if (ch == BACKSPACE && strSend.size() > 0)
			strSend.pop_back();
		else if (ch != BACKSPACE)
			strSend.push_back(ch);
		ch = _getch();
	}
	cout << endl;
	strSend = "PASS " + strSend + "\r\n";
	ftpSendCommand(clnt, strSend.c_str(), &codeftp);
	cout << clnt->buffer;
	memset(clnt->buffer, 0, BUFSIZE);

	// Kiểm tra response code
	// 530: Lỗi đăng nhập (sai mật khẩu/tài khoản)
	if (codeftp == 530 || FTP_REPLY_CODE_3YZ(codeftp))
		return 0;
	return 1;
}

// Đầu vào: FtpClientContext, loại lệnh ("dir" hay "ls"), thư mục gốc, port
// Đầu ra: Chuỗi data server gửi về, rỗng nếu thất bại trong giai đoạn nào đó
// Chức năng: Liệt kê các file và folder trên Server (lệnh ls)
string List(FtpClientContext* clnt, string type, string loc, int& port)
{
	int k = 0;
	string res;
	if (clnt->passiveMode == 0) {	// Active
		int codeftp = Active(clnt, port);
		memset(clnt->buffer, 0, BUFSIZE);
		clnt->dataSocket = CreateListen(port);
		if (clnt->dataSocket == INVALID_SOCKET) {
			wprintf(L"Accept failed with error: %ld\n", WSAGetLastError());
		}
		else {
			k = 1;
		}
	}
	else { // Passive
		// Gọi passive, trong đó tạo dataSocket, gửi lệnh pasv, lấy port và connect bằng CSocket data
		if (Passive(clnt->controlSocket, clnt->dataSocket) == 0) {
			cout << "Could not connect to server in passive" << endl;
		}
		else {
			k = 1;
		}
	}
	if (k == 1) {
		res = SendLIST_NLSTCommand(clnt, type, loc);
		cout << clnt->buffer;
	}
	closesocket(clnt->dataSocket);
	port++;
	return res;
}

// Đầu vào: FtpClientContext, loại lệnh ("dir" hay "ls"), thư mục gốc
// Đầu ra: Chuỗi string chứa data
// Chức năng: gửi lệnh LIST/NLST lên server, nhận data về (lưu ý cout buffer sau khi gọi hàm)
string SendLIST_NLSTCommand(FtpClientContext* clnt,  string type, string loc)
{
	int codeftp;
	string strSend, res;
	if (type == "dir")
		strSend = "LIST";
	else // "ls"
		strSend = "NLST";
	if (loc != "") {
		DeleteAllDoubleQuotationMarks(loc);
		strSend = strSend + ' ' + loc + "\r\n";
	}
	else
		strSend = strSend + "\r\n";
	clnt->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	// 150 open || 425 reject
	ReceivingMessage(clnt->controlSocket, clnt->buffer);
	cout << clnt->buffer;	
	sscanf(clnt->buffer, "%d", &codeftp);
	if (codeftp == 150) {
		// Accept connect (nếu là active)
		if (clnt->passiveMode == false)
			clnt->dataSocket = accept(clnt->dataSocket, NULL, NULL);

		// data (phòng khi quá dài)
		ReceivingData(&clnt->dataSocket, clnt->buffer);
		while (strlen(clnt->buffer) > 0) {
			res += clnt->buffer;
			memset(clnt->buffer, 0, BUFSIZE);
			ReceivingData(&clnt->dataSocket, clnt->buffer);
		}
		// 226 success || 425 fail
		ReceivingMessage(clnt->controlSocket, clnt->buffer);
	}
	return res;
}

// Đầu vào: FtpClientContext, tên file, port
// Đầu ra: Không có
// Chức năng: Download 1 file
void Get(FtpClientContext* clnt, string serfname, int& port)
{	
	int codeftp;
	string strSend, locfname;
	bool rn = 0; // key xác định có rename không

	// Chỉ có chữ get
	if (serfname == "") {
		rn = 1;
		cout << "Remote file: ";
		getline(cin, serfname);
		cout << "Local file: ";
		getline(cin, locfname);
	}
	else {
		locfname = "";
	}

	// Set path lại thành \\ nếu có
	int i, k = 0;
	for (i = 0; i < locfname.length(); i++) {
		// Nếu có 2 chấm thì là có path
		if (locfname[i] == ':') {
			k = 1;
		}
		if (locfname[i] == '/') {
			locfname[i] = '\\';
		}
	}

	for (i = 0; i < serfname.length(); i++) {
		if (serfname[i] == '/') {
			serfname[i] = '\\';
		}
	}

	// Khi không đổi tên (tức là không thể đổi thư mục)
	if (rn == 0) {
		// thì tên file sẽ down = path mặc định + tên của file trên server
		// Hàm nãy đã xóa hết dấu "
		locfname = directorLocal + GetFileNameOutOfPath(serfname);
	}
	else {
		// Nếu đổi tên thì tên file sẽ down xuống client = locfname đã nhập từ trước + xóa hết dấu "
		DeleteAllDoubleQuotationMarks(locfname);
		if (k == 0)
			locfname = directorLocal + locfname;
	}

	DeleteAllDoubleQuotationMarks(serfname);
		
	if (clnt->passiveMode == 0) { // Active
		int codeftp = Active(clnt, port);
		memset(clnt->buffer, 0, BUFSIZE);
		clnt->dataSocket = CreateListen(port);
		if (clnt->dataSocket == INVALID_SOCKET) {
			wprintf(L"Accept failed with error: %ld\n", WSAGetLastError());
			closesocket(clnt->dataSocket);
		}
		else {
			if (DownloadFile(clnt, serfname, locfname) == 1) {
				// Đóng kết nối, nhận tin báo hoàn thành gửi data của server
				closesocket(clnt->dataSocket);
				ReceivingMessage(clnt->controlSocket, clnt->buffer);
				cout << clnt->buffer;
			}
			else {
				closesocket(clnt->dataSocket);
				cout << "File not found or you don't have permission to download this file" << endl;
			}
		}
	}
	else {						  // Passive
		if (Passive(clnt->controlSocket, clnt->dataSocket) == 0) {
			cout << "Could not connect to server in passive" << endl;
			closesocket(clnt->dataSocket);
		}
		else {
			if (DownloadFile(clnt, serfname, locfname) == 1) {
				// Đóng kết nối, nhận tin báo hoàn thành gửi data của server
				closesocket(clnt->dataSocket);
				ReceivingMessage(clnt->controlSocket, clnt->buffer);
				cout << clnt->buffer;
			}
			else {
				closesocket(clnt->dataSocket);
				cout << "File not found or you don't have permission to download this file" << endl;
			}
		}
	}
	port++;
}

// Đầu vào: FtpClientContext, tên file
// Đầu ra: Không có
// Chức năng: Download 1 file (Hàm dùng trong hàm get)
bool DownloadFile(FtpClientContext* clnt, string serfname, string locfname)
{
	string strSend;
	int codeftp;
	// Gửi lệnh RETR
	strSend = "RETR " + serfname + "\r\n";
	clnt->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(clnt->controlSocket, clnt->buffer);
	cout << clnt->buffer;
	sscanf(clnt->buffer, "%d", &codeftp);
	memset(clnt->buffer, 0, BUFSIZE);
							// 150 = opening data channel
	if (codeftp == 150) {	// 550 = file not found
		// Accept connect (nếu là active)
		if (clnt->passiveMode == false)
			clnt->dataSocket = accept(clnt->dataSocket, NULL, NULL);			
		// Xóa hết dấu " để mở file trong client
		DeleteAllDoubleQuotationMarks(locfname);
		// Tạo vỏ file rồi nhét dữ liệu đã lấy ở download vào
		ofstream f;
		f.open(locfname.c_str(), ios_base::out | ios_base::binary | ios_base::trunc);

		while (recv(clnt->dataSocket, clnt->buffer, BUFSIZE, 0) > 0) {
			f.write(clnt->buffer, BUFSIZE);
			memset(clnt->buffer, 0, BUFSIZE);
		}
		f.close();
		return 1;
	}
	return 0;
}

// Đầu vào: FtpClientContext, tên file (đường dẫn + tên file trong máy client), port
// Đầu ra: Không có
// Chức năng: Upload 1 file
void Put(FtpClientContext* clnt, string file, int& port)
{
	string strSend, serfname;
	bool rn = 0; // key xác định có rename không

	// Chỉ có chữ put
	if (file == "") {
		rn = 1;
		cout << "Local file: ";
		getline(cin, file);
		//cin.ignore();
		cout << "Remote file: ";
		getline(cin, serfname);
	}
	else {
		serfname = "";
	}	

	// Set path của file trong client lại thành \\ nếu có (phòng luôn người dùng nhập sai [1 lúc dùng cả 2 dấu])
	int i, k = 0;
	for (i = 0; i < file.length(); i++) {
		// Nếu có 2 chấm thì là có path
		if (file[i] == ':') {
			k = 1;
		}
		if (file[i] == '/') {
			file[i] = '\\';
		}
	}
	if (k == 0)	// tức không nhập path, và mặc định là C:\\ /
		file = directorLocal + file;

	// Khi không đổi tên (tức là không thể đổi thư mục)
	if (rn == 0) {
		// thì tên file sẽ up lên server = tên của file trong client
		// Hàm nãy đã xóa hết dấu "
		serfname = GetFileNameOutOfPath(file);
	}
	else {
		// Nếu đổi tên thì tên file sẽ up lên server = serfname đã nhập từ trước + xóa hết dấu "
		DeleteAllDoubleQuotationMarks(serfname);
	}	

	if (clnt->passiveMode == 0) {	// Active ---------------------------------
		int codeftp = Active(clnt, port);
		cout << clnt->buffer;
		memset(clnt->buffer, 0, BUFSIZE);
		if (codeftp == 200) { // 200 = port command successful			
			clnt->dataSocket = CreateListen(port);
			if (clnt->dataSocket == INVALID_SOCKET) {
				wprintf(L"Accept failed with error: %ld\n", WSAGetLastError());
				closesocket(clnt->dataSocket);
			}
			else {
				if (StorFileToServer(clnt, file, serfname) == 0) {
					cout << "Error, file not found or couldn't open file" << endl;
					closesocket(clnt->dataSocket);
				}
				else {
					closesocket(clnt->dataSocket);
					ReceivingMessage(clnt->controlSocket, clnt->buffer);
					cout << clnt->buffer;
				}
			}
		}
	}
	else {	// Passive ------------------------------------------------------
		if (Passive(clnt->controlSocket, clnt->dataSocket) == 0) {
			cout << "Could not connect to server in passive" << endl;
			closesocket(clnt->dataSocket);
		}
		else {
			if (StorFileToServer(clnt, file, serfname) == 0) {
				cout << "Error, file not found or couldn't open file" << endl;
				closesocket(clnt->dataSocket);
			}
			else {
				closesocket(clnt->dataSocket);
				ReceivingMessage(clnt->controlSocket, clnt->buffer);
				cout << clnt->buffer;
			}
		}
	}
	port++;
}

// Đầu vào: FtpClientContext, tên file trong client, tên file trên server (tên của file sau khi up)
// Đầu ra: 1: thành công, 0: thất bại
// Chức năng: Upload file lên server
bool StorFileToServer(FtpClientContext* clnt, string file, string serfname)
{
	int codeftp;
	string strSend;
	// Gửi tới Server loại data của file (ASCII/IMAGE)
	char datatype;
	cout << "Please enter the data type of the file (A/I): ";
	cin >> datatype;
	cin.ignore();
	sprintf(clnt->buffer, "TYPE %c\r\n", datatype);
	clnt->controlSocket.Send(clnt->buffer, strlen(clnt->buffer), 0);
	ReceivingMessage(clnt->controlSocket, clnt->buffer);
	cout << clnt->buffer;

	strSend = "STOR " + serfname + "\r\n";
	clnt->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(clnt->controlSocket, clnt->buffer);
	cout << clnt->buffer;
	sscanf(clnt->buffer, "%d", &codeftp);
	memset(clnt->buffer, 0, BUFSIZE);

	if (codeftp == 150) {	// 550 Permission denied
	// Accept connect (nếu là active)
		if (clnt->passiveMode == false)
			clnt->dataSocket = accept(clnt->dataSocket, NULL, NULL);

		// Upload file lên server
		// Xóa hết dấu " để mở file trong client
		DeleteAllDoubleQuotationMarks(file);
		ifstream f;
		f.open(file.c_str(), ios_base::binary | ios_base::in);
		if (!f.fail()) {
			// lấy file's size
			f.seekg(0, f.end);
			int length = f.tellg();
			f.seekg(0, f.beg);

			while (length > BUFSIZE)
			{
				f.read(clnt->buffer, BUFSIZE);
				send(clnt->dataSocket, clnt->buffer, BUFSIZE, 0);
				length -= BUFSIZE;
			}
			f.read(clnt->buffer, length);
			send(clnt->dataSocket, (char*)clnt->buffer, length, 0);
			memset(clnt->buffer, 0, BUFSIZE);
			f.close();
			return 1;
		}
		f.close();		
	}
	return 0;
}

// Đầu vào: CSoket
// Đầu ra: Không có
// Chức năng: Download nhiều file
void MGet(FtpClientContext* clnt, string list, int& port)
{
	int i, j, count = 0, start;
	string strSend, buf, buf2, file, decide;
	char ch;
	
	// Nếu chưa nhập gì thì bây giờ nhập
	if (list == "") {
		cout << "Remote file: ";
		getline(cin, list);
	}
	// Set to type A
	strSend = "TYPE A\r\n";
	clnt->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(clnt->controlSocket, clnt->buffer);

	// Trong vòng for này, chúng ta sẽ lần lượt lấy từng cái tên trong string list ra, NLST nó
	// Nếu là file thì giữ lại trong string file, nếu là folder thì giữ lại tại cuối string list
	// Cuối cùng, khi không còn folder nào nữa thì list = rỗng và file = tên tất cả các file cần download
	for (i = 0; ; i++)
	{
		if (list[i] == '\"')
			count++;
		// Các tên có khoảng trắng sẽ có 2 dấu '\"' ở 2 đầu, vì vậy, nếu đang có 1 dấu '\"' thì bỏ qua các ' ' tiếp theo
		// nếu đã có 2 lần các dấu '\"' mà gặp ' ' thì đó là 1 cái tên
		if ((list[i] == ' ' || list[i] == '\0') && count % 2 == 0) {
			// Lấy được 1 cái tên
			buf = list.substr(0, i);
			// Reset count, i, xóa tên trong string list
			count = 0;
			list.erase(0, i + 1);
			i = -1; // do vòng lặp có i++
			// List mà có dấu * ở cuối thì không được (xóa dấu (/ hoặc \\) và dấu *)
			if (buf[buf.length() - 1] == '*')
				buf.erase(buf.length() - 2, 2);
			// Dùng lệnh ls cho cái tên vừa lấy được
			buf2 = List(clnt, "ls", buf, port);
			// Nếu như nhập tên đúng (thì mới list được)
			if (buf2 != "") {
				// Tại đây, trong buf2 sẽ có rất 1 hay nhiều tên file/folder và chúng ngăn cách nhau bởi \r\n
				start = 0;
				for (j = 0; j < buf2.length(); j++) {
					if (buf2[j] == '\r' && buf2[j + 1] == '\n') {
						buf = buf2.substr(start, j - start);
						j = j + 1; // vòng lặp có j++ nên trong vòng sau, file[j] sẽ là kí tự đầu tiên của dòng tiếp theo
						start = j + 1;
						// Nếu như là file: lưu vào file. Nếu như là folder: để lại vào list
						if (IsAFile(buf) == true)
							file = file + buf + "\r\n";
						else {
							if (list == "")
								list += buf;
							else
								list = list + ' ' + buf;
						}
					}
				}
			}
		}
		if (list == "")
			break;
	}
	// Bây giờ chúng ta đã có tất cả các file cần down
	// Lấy lại từng tên ra và gọi hàm get
	start = 0;
	for (j = 0; j < file.length(); j++) {
		if (file[j] == '\r' && file[j + 1] == '\n') {
			buf = file.substr(start, j - start);
			j = j + 1; // vòng lặp có j++ nên trong vòng sau, file[j] sẽ là kí tự đầu tiên của dòng tiếp theo
			start = j + 1;

			cout << "mget " + buf + "? ";
			ch = _getch();
			while (ch != ENTER) {
				decide.push_back(ch);
				cout << ch;
				ch = _getch();
			}
			cout << endl;
			if (decide == "yes" || decide == "Yes" || decide == "YES" || decide == "y" || decide == "Y" || decide == "") // decide = "" tức là chỉ nhập dấu enter
				Get(clnt, buf, port);
			decide = "";
		}
	}
}

// Đầu vào: string đầu vào chứa nhiều đường dẫn file
// Đầu ra: string còn lại sau khi đã lấy string đường dẫn đầu tiên tìm được
// Chức năng: Tách các đường dẫn file có trong chuỗi string đầu vào
string SeparateFilePaths(string &files)
{
	int n = files.length(), flag = 0;
	for (int i = 0; i < n; i++)
	{
		if (files[i] == '"')
		{
			flag++;
			if (flag == 2)
				flag = 0;
		}
		if (files[i] == ' ' && flag == 0)
		{
			string fileString;
			for (int j = 0; j <= i; j++)
				fileString.push_back(files[j]);
			files.erase(0, fileString.length());
			fileString.pop_back(); // Bỏ dấu cách cuối câu
			return fileString;
		}
		else if (i == n - 1)
		{
			string fileString;
			for (int j = 0; j <= i; j++)
				fileString.push_back(files[j]);
			files.erase(0, fileString.length());
			return fileString;
		}
	}
}

// Đầu vào: FtpClientContext, tên file (đường dẫn + tên file trong máy client)
// Đầu ra: Không có
// Chức năng: Upload nhiều file
void MPut(FtpClientContext* clnt, string files, int& port)
{
	int i, j, l, begin;
	bool b;
	string buffer, decide, star, path, temp;
	char ch;
	// Chỉ có chữ mput
	if (files == "")
	{
		cout << "Local file: ";
		getline(cin, files);
	}

	while (files != "")
	{
		buffer = SeparateFilePaths(files);
		// Xét xem buffer có dấu : không
		b = false;
		l = buffer.length();
		for (i = 0; i < l; i++) {
			if (buffer[i] == ':') {
				b = true;
			}
			if (buffer[i] == '/')
				buffer[i] = '\\';
		}
		DeleteAllDoubleQuotationMarks(buffer);
		if (b == false)
			buffer = directorLocal + buffer;
		// Nếu như có dấu '*' thì phải list
		// Dùng: + star lưu lại từ dấu * trở đi
		//		 + path lưu lại đường dẫn từ đầu cho đến dấu *
		b = false;
		l = buffer.length();
		for (i = l - 1; i > -1; i--) {
			if (buffer[i] == '*') {
				star = buffer.substr(i, l - i);
				path = buffer.substr(0, i);
				buffer = "";
				b = true;	// để đánh dấu đã có dấu *
				break;
			}
		}
		// Nếu như có dấu *
		if (b == true) {
			// Dùng tạm string decide để lưu tất cả các file/folder có trong thư mục có dấu * và chúng ngăn cách nhau bởi \n
			if (ListFileInClientFolder(path, decide) != EXIT_FAILURE) {
				l = decide.length();
				j = 0;
				// Nếu như star chỉ là dấu * => Lấy tất cả các file/folder
				if (star == "*") {
					for (i = 0; i < l; i++) {
						if (decide[i] == '\n') {
							buffer = buffer + path + decide.substr(j, i + 1);
							j = i + 1;	// chỉ số của kí tự đầu tiên của dòng tiếp theo
						}
					}
				}
				else { // Nếu như star là *.<đuôi file> => Lấy đúng các file đó
					// Xóa dấu * trong star để so sánh đuôi dễ dàng hơn
					star.erase(0, 1);
					begin = 0;
					for (i = 0; i < l; i++) {
						// Tìm đến dấu chấm, những gì không có dấu chấm thì là folder
						// Mỗi khi xuống dòng, lưu lại chỉ số bắt đầu
						if (decide[i] == '\n')
							begin = i + 1;
						if (decide[i] == '.') {
							for (j = i; j < l; j++) {
								if (decide[j] == '\n')
									break;
							}
							// Có được chỉ số bắt đầu và kết thúc của đuôi file
							temp = decide.substr(i, j - i);
							// So sánh với star, được thì vào buffer, ko thì tiếp tục
							if (temp == star)
								buffer = buffer + path + decide.substr(begin, j + 1 - begin);
							i = j;
							begin = i + 1;
						}
					}
				}
			}
		}
		// Đã có hết các file/folder cần thiết
		i = 0;
		while (buffer != "") {
			// Lấy lại path và tên của từng file rồi up (dùng string star)
			for (i = 0; i < l+1; i++) {
				if (buffer[i] == '\n' || buffer[i] == '\0') {
					star = buffer.substr(0, i);
					if (buffer[i] == '\0')
						buffer = "";
					else
						buffer.erase(0, i+1);
					break;
				}
			}
			decide = "";
			cout << "mput " << star << "? ";
			ch = _getch();
			while (ch != ENTER) {
				decide.push_back(ch);
				cout << ch;
				ch = _getch();
			}
			cout << endl;
			if (decide == "yes" || decide == "Yes" || decide == "YES" || decide == "y" || decide == "Y" || decide == "")
				Put(clnt, star, port);
		}
	}
}

// Đầu vào: đường dẫn cần liệt kê file/folder, chuỗi nhận các file/folder trong đó
// Đầu ra: 1: Không mở được đường dẫn, 0: Hoàn thành
// Chức năng: liệt kê file/folder trong mấy client
bool ListFileInClientFolder(string dirpath, string& allnames)
{
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(dirpath.c_str())) != NULL) {
		// Lấy tất cả file/folder trong dirpath
		while ((ent = readdir(dir)) != NULL) {
			allnames = allnames + ent->d_name + '\n';
		}
		closedir(dir);
		return 0;
	}
	else {
		// Không mở được dirpath
		perror("");
		return EXIT_FAILURE;
	}	
}

// Đầu vào: FtpClientContext, đường dẫn
// Đầu ra: không có
// Chức năng: Thay đổi đường dẫn trên Server (cd)
void ChangeDirector(FtpClientContext *context, string pathName)
{
	if (context == NULL) //ERROR_INVALID_PARAMETER
		return;
	string strSend;
	int replyCode;
	if (pathName != "")
	{
		DeleteAllDoubleQuotationMarks(pathName);
		strSend = "CWD " + pathName + "\r\n";
		ftpSendCommand(context, strSend.c_str(), NULL);
		cout << context->buffer;
	}
	else
	{
		cout << "Remote directory ";
		getline(cin, strSend);
		DeleteAllDoubleQuotationMarks(strSend);
		strSend = "CWD " + strSend + "\r\n";
		ftpSendCommand(context, strSend.c_str(), NULL);
		cout << context->buffer;
	}
}

// Đầu vào: pathname
// Đầu ra: không có
// Chức năng: thay đổi đường dẫn dưới client
void ChangeLocalDirector(string pathName)
{
	//Thao tác trên command line bằng lệnh system
	//Lệnh system chỉ thực hiện ngay trên dòng lệnh nên phải thực hiện đồng thời
	//vd: system("cd %homepath% && cd") không sử dụng system("cd %homepath%") system("cd")

	if (pathName != "")
		DeleteAllDoubleQuotationMarks(pathName);

	string strSend;
	//trường hợp chỉ có lệnh lcd: hiển thị local directory
	//set lại local diretory là %homepath%
	if (pathName == "")
	{
		cout << "Local directory now ";
		strSend = "cd " + directorLocal + " && cd";
		system(strSend.c_str());
	}
	//trường hợp có đường dẫn
	//Có 2 trường hợp: 1 là đường dẫn global, 2 là đường dẫn nằm trong chính folder
	else if (pathName != "")
	{
		//Trường hợp 1
		string temp = directorLocal + pathName;
		strSend = "cd " + temp;
		int code = system(strSend.c_str());
		//nếu thành công
		if (code == NULL)
		{
			directorLocal = directorLocal + pathName + "\\";
			strSend += " && cd";
			cout << "Local directory now ";
			system(strSend.c_str());
		}
		//trường hợp 2
		//nếu thành công
		else if (system((strSend = "cd " + pathName).c_str()) == NULL)
		{
			directorLocal = pathName + "\\";
			strSend += " && cd";
			cout << "Local directory now ";
			system(strSend.c_str());
		}
		//nếu cả 2 trường hợp đều sai thì đường dẫn bị sai
		else
			cout << pathName << ": File not found" << endl;
	}
}

// Đầu vào: FtpClientContext, fileName
// Đầu ra: không có
// Chức năng: xóa một file trên server
void DeleteFileOnServer(FtpClientContext *context, string fileName)
{
	char buff[BUFSIZE];
	DeleteAllDoubleQuotationMarks(fileName);
	string strSend = "DELE " + fileName + "\r\n";
	ftpSendCommand(context, strSend.c_str(), NULL);
	cout << context->buffer;
	memset(context->buffer, 0, BUFSIZE);
}

// Đầu vào: FtpClientContext, tên của file cần xóa, port client
// Đầu ra: không có
// Chức năng: xóa nhiều file trên server
void DeleteMultipleFileOnServer(FtpClientContext *context, string pathName, int& port)
{
	CSocket *clnt = &context->controlSocket;
	char strSend[50];
	vector<string> temp;
	int codeftp;
	char buff[BUFSIZE];
	if (pathName == "")
	{
		cout << "Remote files ";
		getline(cin, pathName);
	}

	/* Có 2 trường hợp
	TH1: xóa file theo danh sách tên mdelete file1 file2 ...
	TH2: xóa tất cả file có subdirector
	*/
	//trường hợp 1
	if (pathName != "*" && pathName != "")
	{
		//tách lần lượt từng tên file ra
		short idx = 0;
		short idx1 = 0;
		short length = pathName.length();
		while (1)
		{
			idx1 = pathName.find(" ", idx);
			if (idx1 == -1)
				break;
			temp.push_back(pathName.substr(idx, idx1 - idx));
			idx = idx1 + 1;
		}
		temp.push_back(pathName.substr(idx, pathName.length() - idx));

		//flag kiểm tra: nếu tất cả các file tồn tài: true và ngược lại
		bool flag = true;
		//chế độ truyền tập tin
		sprintf(strSend, "TYPE A\r\n");
		ftpSendCommand(context, strSend, NULL);
		cout << context->buffer;

		//Mảng lưu các data connect
		SOCKET *listen = new SOCKET[temp.size()];
		//kiểm tra từng tên file bằng cách gửi lệnh NSLT lên server
		for (int i = 0; i < temp.size(); i++)
		{
			//Nếu chế độ passive tắt
			if (context->passiveMode == false)
			{
				Active(context, port);
				listen[i] = CreateListen(port++);
			}
			else
			{
				Passive(context->controlSocket, listen[i]);
			}

			sprintf(strSend, "NLST %s\r\n", temp[i].c_str());
			ftpSendCommand(context, strSend, &codeftp);
			//Không tồn tại file
			if (codeftp != 150)
			{
				flag = false;
				break;
			}
		}
		//xóa file nếu flag = true
		if (flag == true)
		{
			char ch;
			string decide;
			for (int i = 0; i < temp.size(); i++)
			{
				cout << "mdelete " + temp[i] + "? ";
				ch = _getch();
				while (ch != 13)
				{
					cout << ch;
					decide.push_back(ch);
					ch = _getch();
				}
				cout << endl;
				if (decide[0] == 'y' || decide == "")
				{
					sprintf(strSend, "DELE %s\r\n", temp[i].c_str());
					ftpSendCommand(context, strSend, NULL);
					cout << context->buffer;
				}
			}
		}
		else
			cout << "Cannot find list of remote files.\r\n";
		//Đóng kết nối tại các SOCKET đã mở
		for (int i = 0; i < temp.size(); i++)
		{
			closesocket(listen[i]);
		}
	}
	//Trường hợp 2: xoá tất cả
	else if (pathName[0] == '*')
	{
		//mode Active
		SOCKET listen;
		if (context->passiveMode == false)
		{
			codeftp = Active(context, port);
			listen = CreateListen(port++);
		}
		else
		{
			Passive(context->controlSocket, listen);
		}

		//Kiểu truyền dữ liệu
		sprintf(strSend, "TYPE A\r\n");
		ftpSendCommand(context, strSend, NULL);
		cout << context->buffer;

		//Xóa tất cả file
		sprintf(strSend, "NLST %s\r\n", pathName.c_str());
		ftpSendCommand(context, strSend, &codeftp);

		if (context->passiveMode == false)
			listen = accept(listen, NULL, NULL);

		//Nhận thông báo từ server
		memset(buff, 0, BUFSIZE);
		ReceivingMessage(context->controlSocket, buff);

		//Nhận data từ server
		ReceivingData(&listen, buff);

		//Có 2 trường hợp:
		//TH1: xóa tất cả file *
		//TH2: xóa tất cả file theo dạng: *[loại file]
		string cmp = "";
		if (pathName != "*")
			cmp = pathName.substr(1, pathName.length() - 1);
		char *tok = strtok(buff, "\r\n");
		char ch;
		string decide;
		//Không có file nào để xóa nếu tok == NULL
		while (tok != NULL)
		{
			bool flag = true;
			//TH2 xóa theo dạng file nếu cmp != ""
			if (cmp != "")
			{
				if (strstr(tok, cmp.c_str()) == NULL)
					flag = false;
			}
			//Nếu là TH1 thì flag luôn = true
			if (flag == true)
			{
				sprintf(strSend, "DELE %s\r\n", tok);
				printf("mdelete %s? ", tok);
				ch = _getch();
				while (ch != ENTER)
				{
					cout << ch;
					decide.push_back(ch);
					ch = _getch();
				}
				cout << endl;
				if (decide[0] == 'y' || decide == "")
				{
					sprintf(strSend, "DELE %s\r\n", tok);
					ftpSendCommand(context, strSend, NULL);
					cout << context->buffer;
				}
			}
			tok = strtok(NULL, "\r\n");
		}
		//Đóng kết nối
		closesocket(listen);
	}
}

// Đầu vào: FtpClientContext, pathName
// Đầu ra: không có
// Chức năng: tạo thư mục trên server (mkdir)
void MakeDir(FtpClientContext *context, string pathName)
{
	if (context == NULL)
		return;  //ERROR_INVALID_PARAMETER
	char temp[BUFSIZE];
	string strSend;
	if (pathName != "")
	{
		int idx;
		idx = pathName.find(" ", 0);
		if (idx != -1)
			pathName = pathName.substr(0, idx);
		strSend = "MKD " + pathName + "\r\n";
		ftpSendCommand(context, strSend.c_str(), NULL);
		cout << context->buffer;
	}
	else
	{
		cout << "Remote directory ";
		scanf("%s", &temp);
		strSend = "MKD " + string(temp) + "\r\n";
		ftpSendCommand(context, strSend.c_str(), NULL);
		cout << context->buffer;
	}
}

// Đầu vào: FtpClientContext, chuỗi tên folder xóa
// Đầu ra: không có
// Chức năng: Xóa thư mục rỗng
void DeleteEmptyFolder(FtpClientContext *context, string folder)
{
	char temp[BUFSIZE];
	int codeftp;
	string strSend;
	if (folder == "")
	{
		cout << "Directory name ";
		getline(cin, folder);
	}
	DeleteAllDoubleQuotationMarks(folder);
	strSend = "XRMD " + folder + "\r\n";
	context->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(context->controlSocket, temp);
	cout << temp;
}

// Đầu vào: FtpClientContext
// Đầu ra: không có
// Chức năng: Hiển thị đường dẫn hiện tại trên server
void DisplayCurrentWorkingDirectory(FtpClientContext *client)
{
	char temp[BUFSIZE];
	string strSend;
	strSend = "XPWD\r\n";
	client->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(client->controlSocket, temp);
	cout << temp;
}

// Đầu vào: FtpClientContext
// Đầu ra: không có
// Chức năng: Thoát khỏi server
void Quit(FtpClientContext *client)
{
	char temp[BUFSIZE];
	string strSend;
	strSend = "QUIT\r\n";
	client->controlSocket.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(client->controlSocket, temp);
	cout << temp;
}

// Đầu vào: FtpClientContext
// Đầu ra: trả về codeftp
// Chức năng: Mở kết nối active
int Active(FtpClientContext* clnt, int port)
{
	int codeftp;
	char strSend[BUFSIZE];
	//Gửi lệnh PORT đến server
	sprintf(strSend, "PORT %d,%d,%d,%d,%d,%d\r\n", 127, 0, 0, 1, MSB(port), LSB(port));
	ftpSendCommand(clnt, strSend, &codeftp);
	return codeftp;
}

// Đầu vào: port
// Đầu ra: NULL: tạo thất bại, SOCKET: tạo thành công
// Chức năng: Tạo socket listen cho cơ chế active
SOCKET CreateListen(int port)
{
	SOCKET ListenSocket;
	SOCKADDR_IN Addr;
	Addr.sin_family = AF_INET;
	Addr.sin_port = htons(port);
	Addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET)
	{
		wprintf(L"INVALID SOCKET : %ld\n", WSAGetLastError());
		WSACleanup();
		return NULL;
	}
	if (bind(ListenSocket, (LPSOCKADDR)&Addr, sizeof(Addr)) == SOCKET_ERROR)
	{
		wprintf(L"bind failed with error: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return NULL;
	}
	if (listen(ListenSocket, 1) == SOCKET_ERROR) {
		wprintf(L"listen failed with error: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return NULL;
	}
	return ListenSocket;
}

// Đầu vào: FtpClientContext
// Đầu ra: 0: kết nối thất bại, 1: kết nối thành công
// Chức năng: Kết nối passive
bool Passive(CSocket& clnt, SOCKET& data)
{
	int i, port = 0, h[2], k = 1, count = 0;
	h[0] = 0; h[1] = 0;
	string strSend;
	char temp[BUFSIZE + 1];
	strSend = "PASV\r\n";
	clnt.Send((char*)strSend.c_str(), strSend.length(), 0);
	ReceivingMessage(clnt, temp);
	// Lấy h1 và h2 do server gửi về, còn p1 -> p4 thì như IP cũ
	for (i = strlen(temp); i > 0; i--) {
		if (temp[i - 1] == ',') {
			while (temp[i] != ')' && temp[i] != ',') {
				h[k] = (h[k] * 10) + (temp[i] - '0');
				i++; count++;
			}
			i = i - (count + 2); // dịch i về vị trí trước dấu phẩy (trong vòng while + 1 (dấu phẩy) + 1 (trước dấu phẩy))
			k--;
		}
		if (k == -1)
			break;
	}
	memset(temp, 0, BUFSIZE);
	port = h[0] * 256 + h[1];

	//Tạo data
	data = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in serverAddr;
	//connect tới port mới, socket này chỉ dùng trong chuyển data thôi
	char ServerName[64];
	sprintf(ServerName, "localhost");
	HOSTENT *pHostEnt;
	if (pHostEnt = gethostbyname(ServerName))
		memcpy(&serverAddr.sin_addr, pHostEnt->h_addr_list[0], pHostEnt->h_length);

	serverAddr.sin_family = AF_INET;
	//serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");	// IP server
	serverAddr.sin_port = htons((u_short)port);	// port
	if (connect(data, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		closesocket(data);
		WSACleanup();
		return 0;
	}
	return 1;
}

// Đầu vào: FtpClientContext
// Đầu ra: không có
// Chức năng: Chuyển đổi qua lại giữa active và passive
void ActiveOrPassive(FtpClientContext* clnt)
{
	int k = 0;
	char rep;
	if (clnt->passiveMode == 0)
		cout << "You're currently using active. Do you want to switch to passive? (Y/N): ";
	else
		cout << "You're currently using passive. Do you want to switch to active? (Y/N): ";
	cin >> rep;
	cin.ignore();
	if (rep == 'Y') {
		clnt->passiveMode = 1 - clnt->passiveMode;
		k = 1;
	}
	if (clnt->passiveMode == 0 && k == 1)
		cout << "You're now using active" << endl;
	else if (clnt->passiveMode == 1 && k == 1)
		cout << "You're now using passive" << endl;
}

// Đầu vào: CSocket, chuỗi nhận chuỗi kí tự server gửi về
// Đầu ra: Không có
// Chức năng: Nhận tin từ server, loại bỏ kí tự rỗng
uint8_t ReceivingMessage(CSocket& clnt, char* temp)
{
	memset(temp, 0, BUFSIZE);
	uint8_t error;
	error = clnt.Receive((char*)temp, BUFSIZE, 0);
	if (error == -1)
		return error;
	int i;
	for (i = 0; i < BUFSIZE; i++)
	{
		if (temp[i] < 0 || temp[i] > 127)
		{
			temp[i] = '\0';
			break;
		}
	}
	if (strlen(temp) >= BUFSIZE)
		temp[BUFSIZE] = '\0';
	return 1;
}

//Đầu vào: SOCKET, chuỗi server trả về
//Đầu ra: error
//Chức năng: nhận data từ server
uint8_t ReceivingData(SOCKET *clnt, char *temp)
{
	uint8_t error;
	memset(temp, 0, BUFSIZE);
	error = recv(*clnt, temp, BUFSIZE, 0);
	if (error == -1)
		return error;
	for (int i = 0; i < BUFSIZE; i++)
	{
		if (temp[i] < 0 || temp[i] > 127)
		{
			temp[i] = '\0';
			break;
		}
	}
	if (strlen(temp) >= BUFSIZE)
		temp[BUFSIZE] = '\0';
	return 1;
}

// Đầu vào: Đường dẫn
// Đầu ra: Tên file trong đường dẫn
// Chức năng: Lấy tên file ra khỏi đường dẫn
string GetFileNameOutOfPath(string path)
{
	// Chạy ngược từ cuối path, count dùng để đếm độ dài tên file
	string name;
	int i, l = path.length(), count = 0;
	for (i = l-1; i > 0; i--) {
		if (path[i] != '\"')
			count++;
		if (path[i - 1] == '\\' || path[i - 1] == '/') {
			if (path[i] == '\"')
				i++;
			while (count > 0) {
				name += path[i];
				i++;
				count--;
			}
			break;
		}
	}
	// Nếu count khác 0 tức là string file chỉ có đúng tên của file xuống, không có đường dẫn
	if (count != 0) {
		name = path;
	}
	return name;
}

// Đầu vào: Đường dẫn
// Đầu ra: Tên file trong đường dẫn
// Chức năng: Xóa tất cả dấu " (phục vụ cho mở file)
void DeleteAllDoubleQuotationMarks(string& objname)
{
	int i, l = objname.length();
	for (i = 0; i < l; i++) {
		if (objname[i] == '\"') {
			objname.erase(i, 1);
			l--;
		}
	}
}

// Đầu vào: FtpClientContext, command, reply
// Đầu ra: error code
// Chức năng: gửi command tới server, nhận tin trả lời của server
uint8_t ftpSendCommand(FtpClientContext *context, const char *command, int *replyCode)	
{
	size_t length = 0;
	char *p;

	if (command) {
		context->controlSocket.Send(command, strlen(command), 0);
		p = context->buffer;
		if (ReceivingMessage(context->controlSocket, p) == -1)
			return -1;		
		if (replyCode != NULL) {
			//Nhận reply code
			if (p[3] == ' ' || p[3] == '\0') {
				*replyCode = strtoul(p, NULL, 10);
			}
		}
	}
	//thành công
	return NO_ERROR;
}

// Đầu vào: Tên file/folder
// Đầu ra: 0: folder, 1: file
// Chức năng: Xác định đây là tên file hay folder
bool IsAFile(string name)
{
	int i, l = name.length();
	for (i = l - 1; i > -1; i--) {
		if (name[i] == '.')
			return true;
	}
	return false;
}

// Đầu vào: không có
// Đầu ra: không có
// Chức năng: Yêu cầu người dùng nhập đường dẫn mặc định
void EnterLocalDirectory()
{
	cout << "Please enter your local directory (this directory will contain your download files later on): ";
	getline(cin, directorLocal);
	DeleteAllDoubleQuotationMarks(directorLocal);
	int i;
	for (i = 0; i < directorLocal.length(); i++) {
		if (directorLocal[i] == '\\')
			break;
		if (directorLocal[i] == '/')
			directorLocal[i] = '\\';
	}
}
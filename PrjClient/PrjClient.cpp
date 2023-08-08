#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

#define SERVERIPV4  "127.0.0.1"
#define SERVERIPV6  "::1"
#define SERVERPORT  9000
#define ShellExecute
#define FILEBUFSIZE 512

#define BUFSIZE     256                    // 전송 메시지 전체 크기
#define MSGSIZE     (BUFSIZE-sizeof(int))  // 채팅 메시지 최대 길이

#define CHATTING    1000                   // 메시지 타입: 채팅
#define DRAWLINE    1001                   // 메시지 타입: 선 그리기
#define ID			1002				   // 메시지 타입 : ID 설정
#define	LIKE		1003				   // 메시지 타입 : 좋아요 수 
#define FILESEND	1004				   // 메시지 타입 : 파일 이름
#define ERASE	    1005				   // 메시지 타입 : 지우기
#define WM_DRAWIT   (WM_USER+1)            // 사용자 정의 윈도우 메시지
#define WM_ERASEIT   (WM_USER+2)           // 사용자 정의 윈도우 메시지
#define DRAWSQUARE  1006                   // 메시지 타입: 사각형
#define DRAWTRIANGLE  1007                 // 메시지 타입: 삼각형

// 공통 메시지 형식
// sizeof(COMM_MSG) == 256
struct COMM_MSG
{
	int  type;
	char dummy[MSGSIZE];
};

// 채팅 메시지 형식
// sizeof(CHAT_MSG) == 256
struct CHAT_MSG
{
	int  type;
	char buf[MSGSIZE];
};

// 선 그리기 메시지 형식
// sizeof(DRAWLINE_MSG) == 256
struct DRAWLINE_MSG
{
	int  type;
	int  color;
	int  x0, y0;
	int  x1, y1;
	char dummy[BUFSIZE - 6 * sizeof(int)];
};

// 아이디 지정 형식
struct CHAT_ID
{
	int  type;
	char buf[MSGSIZE];
};

// 좋아요 세기
struct LIKE_COUNT
{
	int  type;
	char countbuf[MSGSIZE];
};
int count = 0;

//파일 전송하기
struct FILE_MSG
{
	int  type;
	char fileNameBuf[MSGSIZE];
	char fileDetailBuf[MSGSIZE];
};

//그림 지우기
struct ERASE_MSG
{
	int type;
	int size;
};


//파일을 읽기 위해.
FILE* fpr;


static HINSTANCE     g_hInst;			// 응용 프로그램 인스턴스 핸들
static HWND          g_hDrawWnd;		// 그림을 그릴 윈도우
static HWND          g_hButtonSendMsg;	// '메시지 전송' 버튼
static HWND			 g_hButtonChangeId;	//'변경'버튼
static HWND			 g_hButtonLike;		//좋아요 버튼
static HWND          g_hEditStatus;		// 받은 메시지 출력
static char          g_ipaddr[64];	    // 서버 IP 주소
static u_short       g_port;			// 서버 포트 번호
static BOOL          g_isIPv6;			// IPv4 or IPv6 주소?
static HANDLE        g_hClientThread;	// 스레드 핸들
static HANDLE        g_hFileThread;		// 스레드 핸들
static volatile BOOL g_bStart;			// 통신 시작 여부
static SOCKET        g_sock;			// 클라이언트 소켓
static HANDLE        g_hReadEvent, g_hWriteEvent; // 이벤트 핸들
static CHAT_MSG      g_chatmsg;			// 채팅 메시지 저장
static CHAT_ID       g_chatid;			// 아이디 저장
static LIKE_COUNT    g_likecount;		//좋아요 수
static FILE_MSG		 g_filemsg;			// 파일 이름 저장
static HWND			 g_hButtonFile;		//파일 전송 버튼
static DRAWLINE_MSG  g_drawmsg;			// 선 그리기 메시지 저장
static HWND			 g_hButtonErase;	//지우기 버튼
static ERASE_MSG	 g_erasemsg;		// 선 지우기 메시지 저장
static int			 g_drawsize;		//선 사이즈
static int           g_drawcolor;		// 선 그리기 색상

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI WriteThread(LPVOID arg);


// 자식 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// 편집 컨트롤 출력 함수
void DisplayText(char* fmt, ...);
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags);
// 오류 출력 함수
void err_quit(char* msg);
void err_display(char* msg);

// 메인 함수
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// 이벤트 생성
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// 변수 초기화(일부)
	g_chatmsg.type = CHATTING;
	g_chatid.type = ID;
	g_filemsg.type = FILESEND;
	g_drawmsg.type = DRAWLINE;
	g_drawmsg.color = RGB(80, 80, 80);
	g_erasemsg.type = ERASE;
	g_erasemsg.size = 3;
	g_likecount.type = LIKE;

	// 대화상자 생성
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);


	// 이벤트 제거
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// 윈속 종료
	WSACleanup();
	return 0;
}

//좋아요 증가에 쓸 함수.
int countFun() {
	count += 1;
	return count;
}

//파일 전송에 사용할 함수
void fileDetail(char* msg) {
	// 데이터 통신에 사용할 변수
	int retval3;
	char strBuf[BUFSIZE];		// 파일에서 가져온 문자열을 저장할 버퍼
	int len;					//가변 데이터길이
	int count = 0;				//문자열이 총 몇개 있는지 세기 위해
	char* contents[BUFSIZE];	//파일 이름 입력받고 내용 받아오기
	fpr = fopen(msg, "r");		//파일을 열기 위해 fopen함수 사용. 읽기모드 r

	while (!feof(fpr)) //파일의 끝이 아니라면
	{
		contents[count] = fgets(strBuf, BUFSIZE, fpr);
		len = strlen(contents[count]);
		strncpy(g_filemsg.fileDetailBuf, contents[count], BUFSIZE);
		DisplayText("%s\n", g_filemsg.fileDetailBuf);	//화면에 출력하기

		retval3 = send(g_sock, (char*)&g_filemsg, BUFSIZE, 0);
		if (retval3 == SOCKET_ERROR) {
			err_display("send()");
			break;
		}
		count++;
	}
	
	if (fpr == NULL) {
		printf("파일 열기 실패");
	}
	return ;
}

// 대화상자 프로시저
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hButtonIsIPv6;
	static HWND hEditIPaddr;
	static HWND hEditPort;
	static HWND hButtonConnect;
	static HWND hEditMsg;
	static HWND hColorBlack;
	static HWND hColorLightGray;
	static HWND hColorDeepGray;
	static HWND hColorPink;
	static HWND hColorBlue;
	static HWND hRec;
	static HWND hTri;
	static HWND hID;
	static HWND hFILE;

	switch (uMsg) {		

	case WM_INITDIALOG:

		// 컨트롤 핸들 얻기
		hButtonIsIPv6 = GetDlgItem(hDlg, IDC_ISIPV6);

		hID = GetDlgItem(hDlg, IDC_ID);						// 새롭게 만든 ID 입력창
		g_hButtonChangeId = GetDlgItem(hDlg, IDC_CHANGEID); //ID 변경 버튼

		hFILE = GetDlgItem(hDlg, IDC_FILE);				// file 입력창
		g_hButtonFile = GetDlgItem(hDlg, IDC_FILESEND);	//파일 전송 버튼

		g_hButtonLike = GetDlgItem(hDlg, IDC_LIKE);		//좋아요 버튼

		g_hButtonErase = GetDlgItem(hDlg, IDC_ERASER);	// 지우기 버튼

		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditPort = GetDlgItem(hDlg, IDC_PORT);
		hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);
		g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		g_hEditStatus = GetDlgItem(hDlg, IDC_STATUS);

		// 색상 라디오 버튼
		hColorBlack = GetDlgItem(hDlg, IDC_COLORBLACK);
		hColorLightGray = GetDlgItem(hDlg, IDC_COLORLIGHTGRAY);
		hColorDeepGray = GetDlgItem(hDlg, IDC_COLORDEEPGRAY);
		hColorPink = GetDlgItem(hDlg, IDC_COLORPINK);
		hColorBlue = GetDlgItem(hDlg, IDC_COLORBLUE);
		// 도형 라디오 버튼
		hRec = GetDlgItem(hDlg, IDC_REC);
		hTri = GetDlgItem(hDlg, IDC_TRI);

		// 컨트롤 초기화
		SendMessage(hID, EM_SETLIMITTEXT, MSGSIZE, 0);
		SendMessage(hEditMsg, EM_SETLIMITTEXT, MSGSIZE, 0);
		SendMessage(hFILE, EM_SETLIMITTEXT, MSGSIZE, 0);
		EnableWindow(g_hButtonSendMsg, FALSE);
		SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
		SetDlgItemInt(hDlg, IDC_PORT, SERVERPORT, FALSE);
		SendMessage(hColorDeepGray, BM_SETCHECK, BST_CHECKED, 0);
		SendMessage(hColorPink, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hColorBlue, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hRec, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hTri, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hColorLightGray, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hColorBlack, BM_SETCHECK, BST_UNCHECKED, 0);

		// 윈도우 클래스 등록
		WNDCLASS wndclass;
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = g_hInst;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = "MyWndClass";
		if (!RegisterClass(&wndclass)) return 1;

		// 자식 윈도우 생성
		g_hDrawWnd = CreateWindow("MyWndClass", "그림 그릴 윈도우", WS_CHILD,
			450, 38, 425, 415, hDlg, (HMENU)NULL, g_hInst, NULL);
		if (g_hDrawWnd == NULL) return 1;
		ShowWindow(g_hDrawWnd, SW_SHOW);
		UpdateWindow(g_hDrawWnd);

		return TRUE;

		int result;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDC_ISIPV6:
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);
			if (g_isIPv6 == false)
				SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV4);
			else
				SetDlgItemText(hDlg, IDC_IPADDR, SERVERIPV6);
			return TRUE;

		case IDC_CONNECT:
			GetDlgItemText(hDlg, IDC_IPADDR, g_ipaddr, sizeof(g_ipaddr));
			g_port = GetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);
			g_isIPv6 = SendMessage(hButtonIsIPv6, BM_GETCHECK, 0, 0);

			// 소켓 통신 스레드 시작
			g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
			if (g_hClientThread == NULL) {
				MessageBox(hDlg, "클라이언트를 시작할 수 없습니다."
					"\r\n프로그램을 종료합니다.", "실패!", MB_ICONERROR);
				EndDialog(hDlg, 0);
			}
			else {
				EnableWindow(hButtonConnect, FALSE);
				while (g_bStart == FALSE); // 서버 접속 성공 기다림
				EnableWindow(hButtonIsIPv6, FALSE);
				EnableWindow(hEditIPaddr, FALSE);
				EnableWindow(hEditPort, FALSE);
				EnableWindow(g_hButtonSendMsg, TRUE);
				SetFocus(hID);
			}
			return TRUE;

		case IDC_CHANGEID:
			WaitForSingleObject(g_hReadEvent, INFINITE);
			GetDlgItemText(hDlg, IDC_ID, g_chatid.buf, MSGSIZE);
			SetEvent(g_hWriteEvent);
			SendMessage(hID, EM_SETSEL, 0, -1);
			SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			return TRUE;

		case IDC_LIKE: // 좋아요 버튼 눌렀을 때

			GetDlgItemText(hDlg, IDC_ID, g_chatid.buf, MSGSIZE); // 사용자 아이디 가져오기
			result = countFun();								 // 숫자 증가 함수 호출 후 저장
			sprintf(g_likecount.countbuf, " %s님이 이 그림을 좋아합니다. ->  %d liked \n",
				g_chatid.buf, result);							 // g_likecount 구조체 안의 버퍼에 저장하기
			int retval2;
			retval2 = send(g_sock, (char*)&g_likecount, BUFSIZE, 0);//보내기
			return TRUE;

		case IDC_SENDMSG:
			// 읽기 완료를 기다림
			WaitForSingleObject(g_hReadEvent, INFINITE);
			GetDlgItemText(hDlg, IDC_MSG, g_chatmsg.buf, MSGSIZE);
			SetEvent(g_hWriteEvent);
			// 쓰기 완료를 알림
			// 입력된 텍스트 전체를 선택 표시
			//여기서 메세지 send 해보기!!

			SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			return TRUE;

		case IDC_ERASER: // 지우개 버튼 눌렀을 때

			g_drawmsg.color = RGB(255, 255, 255);		// 흰색으로 펜 변경
			g_erasemsg.size = 20;						// 펜 사이즈 변경
			
			int retval6;
			retval6 = send(g_sock, (char*)&g_erasemsg, BUFSIZE, 0); //데이터 전송
			return TRUE;


		case IDC_FILESEND:

			//파일 전송 버튼 클릭 시
			GetDlgItemText(hDlg, IDC_FILE, g_filemsg.fileNameBuf, MSGSIZE); //파일 이름 가져오기
			fileDetail((char*)&g_filemsg.fileNameBuf); // 파일 이름 파라미터로 전송
		
			SendMessage(hFILE, EM_SETSEL, 0, -1);
			return TRUE;
		
		//색상 변경 라디오 버튼
		case IDC_COLORBLACK:
			g_drawmsg.color = RGB(0, 0, 0);
			return TRUE;

		case IDC_COLORLIGHTGRAY:
			g_drawmsg.color = RGB(160, 160, 160);
			return TRUE;

		case IDC_COLORDEEPGRAY:
			g_drawmsg.color = RGB(80, 80, 80);
			return TRUE;

		case IDC_COLORPINK:
			g_drawmsg.color = RGB(221, 173, 173);
			return TRUE;

		case IDC_COLORBLUE:
			g_drawmsg.color = RGB(167, 191, 227);
			return TRUE;
		
		// 도형 라디오 버튼
		case IDC_REC:
			// 사각형
			g_drawmsg.type = DRAWSQUARE;
			return TRUE;

		case IDC_TRI:
			// 삼각형
			g_drawmsg.type = DRAWTRIANGLE;
			return TRUE;

		case IDCANCEL:
			if (MessageBox(hDlg, "정말로 종료하시겠습니까?",
				"질문", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				closesocket(g_sock);
				EndDialog(hDlg, IDCANCEL);
			}
			return TRUE;

		}
		return FALSE;
	}

	return FALSE;
}


// 소켓 통신 스레드 함수
DWORD WINAPI ClientMain(LPVOID arg)
{
	int retval;

	if (g_isIPv6 == false) {
		// socket()
		g_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
		serveraddr.sin_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
	}
	else {
		// socket()
		g_sock = socket(AF_INET6, SOCK_STREAM, 0);
		if (g_sock == INVALID_SOCKET) err_quit("socket()");

		// connect()
		SOCKADDR_IN6 serveraddr;
		ZeroMemory(&serveraddr, sizeof(serveraddr));
		serveraddr.sin6_family = AF_INET6;
		int addrlen = sizeof(serveraddr);
		WSAStringToAddress(g_ipaddr, AF_INET6, NULL,
			(SOCKADDR*)&serveraddr, &addrlen);
		serveraddr.sin6_port = htons(g_port);
		retval = connect(g_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
		if (retval == SOCKET_ERROR) err_quit("connect()");
	}
	MessageBox(NULL, "서버에 접속했습니다.", "성공!", MB_ICONINFORMATION);

	// 읽기 & 쓰기 스레드 생성
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	//hThread[2] = CreateThread(NULL, 0, LikeThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) {
		MessageBox(NULL, "스레드를 시작할 수 없습니다."
			"\r\n프로그램을 종료합니다.",
			"실패!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// 스레드 종료 대기
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);

	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	g_bStart = FALSE;

	MessageBox(NULL, "서버가 접속을 끊었습니다", "알림", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sock);
	return 0;
}

// 데이터 받기
DWORD WINAPI ReadThread(LPVOID arg)
{
	int retval;
	int retval4;
	COMM_MSG comm_msg;
	CHAT_MSG* chat_msg;
	CHAT_ID* chat_id;
	FILE_MSG* file_msg;
	DRAWLINE_MSG* draw_msg;
	LIKE_COUNT* like_count;
	ERASE_MSG* erase_msg;
	char buf[FILEBUFSIZE + 1];
	while (1) {
		retval = recvn(g_sock, (char*)&comm_msg, BUFSIZE, 0); // 데이터 받기

		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}

		if (comm_msg.type == ID) {
			chat_id = (CHAT_ID*)&comm_msg;
			DisplayText("[%s 님] ", chat_id->buf);
		}

		if (comm_msg.type == ERASE) {
			erase_msg = (ERASE_MSG*)&comm_msg;
			g_drawsize = erase_msg->size;
			SendMessage(g_hDrawWnd, WM_ERASEIT, NULL, NULL);
		}

		if (comm_msg.type == DRAWSQUARE) {
			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));
		}

		if (comm_msg.type == DRAWTRIANGLE) {
			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));
		}

		if (comm_msg.type == CHATTING) {
			chat_msg = (CHAT_MSG*)&comm_msg;
			chat_id = (CHAT_ID*)&comm_msg;
			DisplayText("%s\r\n", chat_msg->buf);
		}

		if (comm_msg.type == LIKE) {

			like_count = (LIKE_COUNT*)&comm_msg;
			DisplayText("%s\r\n", like_count->countbuf);
		}

		if (comm_msg.type == FILESEND) {
			file_msg = (FILE_MSG*)&comm_msg;
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			else {
			//	DisplayText("%s\r\n", file_msg->fileDetailBuf);
			}
		}

		if (comm_msg.type == DRAWLINE) {
			draw_msg = (DRAWLINE_MSG*)&comm_msg;
			g_drawcolor = draw_msg->color;
			SendMessage(g_hDrawWnd, WM_DRAWIT,
				MAKEWPARAM(draw_msg->x0, draw_msg->y0),
				MAKELPARAM(draw_msg->x1, draw_msg->y1));

		}

	}
	return 0;
}

// 데이터 보내기
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;

	// 서버와 데이터 통신
	while (1) {
		// 쓰기 완료 기다리기
		WaitForSingleObject(g_hWriteEvent, INFINITE);
		if (strlen(g_chatid.buf) == 0) {
			// '변경' 버튼 활성화
			EnableWindow(g_hButtonChangeId, TRUE);
			// 읽기 완료 알리기
			SetEvent(g_hReadEvent);
			continue;
		}
		// 문자열 길이가 0이면 보내지 않음
		if (strlen(g_chatmsg.buf) == 0) {
			// '메시지 전송' 버튼 활성화
			EnableWindow(g_hButtonSendMsg, TRUE);
			// 읽기 완료 알리기
			SetEvent(g_hReadEvent);
			continue;
		}
		;
		// 데이터 보내기
		retval = send(g_sock, (char*)&g_chatid, BUFSIZE, 0);
		retval = send(g_sock, (char*)&g_chatmsg, BUFSIZE, 0);

		if (retval == SOCKET_ERROR) {
			break;
		}
		EnableWindow(g_hButtonChangeId, TRUE);
		// '메시지 전송' 버튼 활성화
		EnableWindow(g_hButtonSendMsg, TRUE);
		// 읽기 완료 알리기
		SetEvent(g_hReadEvent);
	}

	return 0;
}

// 자식 윈도우 프로시저
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HDC hDC;
	int cx, cy;
	PAINTSTRUCT ps;
	RECT rect;
	HPEN hPen, hOldPen;
	static HBITMAP hBitmap;
	static HDC hDCMem;
	static HDC hDCMem2;
	static int x0, y0;
	static int x1, y1;
	static BOOL bDrawing = FALSE;
	HBITMAP hImage, hOldBitmap;
	BITMAP bit;
	int bx, by;

	switch (uMsg) {
	case WM_CREATE:
		hDC = GetDC(hWnd);

		// 화면을 저장할 비트맵 생성
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// 메모리 DC 생성
		hDCMem = CreateCompatibleDC(hDC);

		// 비트맵 선택 후 메모리 DC 화면을 흰색으로 칠함
		SelectObject(hDCMem, hBitmap);
		SelectObject(hDCMem, GetStockObject(WHITE_BRUSH));
		SelectObject(hDCMem, GetStockObject(WHITE_PEN));
		Rectangle(hDCMem, 0, 0, cx, cy);

		ReleaseDC(hWnd, hDC);
		return 0;

	case WM_LBUTTONDOWN:
		
			x0 = LOWORD(lParam);
			y0 = HIWORD(lParam);
			bDrawing = TRUE;
		
		return 0;

	case WM_MOUSEMOVE:
		if (bDrawing && g_bStart && g_drawmsg.type == DRAWLINE) {
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);

			// 선 그리기 메시지 보내기
			g_drawmsg.x0 = x0;
			g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x1;
			g_drawmsg.y1 = y1;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);
			send(g_sock, (char*)&g_erasemsg, BUFSIZE, 0);

			x0 = x1;
			y0 = y1;
		}
		
		return 0;

	case WM_LBUTTONUP:
		g_erasemsg.size = 3;
		g_drawsize = g_erasemsg.size;
		if (g_drawmsg.type == DRAWTRIANGLE) {
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);

			// 선 그리기 메시지 보내기
			g_drawmsg.x0 = x0;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1;	g_drawmsg.y1 = y1;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);

			g_drawmsg.x0 = x0;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);	g_drawmsg.y1 = y0;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);

			g_drawmsg.x0 = x1;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x0 + ((x1 - x0) / 2);	g_drawmsg.y1 = y0;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);
			g_drawmsg.type = DRAWLINE;
		}

		if (g_drawmsg.type == DRAWSQUARE) {
			x1 = LOWORD(lParam);
			y1 = HIWORD(lParam);

			// 선 그리기 메시지 보내기
			g_drawmsg.x0 = x0;	g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x0;	g_drawmsg.y1 = y1;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);

			g_drawmsg.x0 = x0;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1;	g_drawmsg.y1 = y1;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);

			g_drawmsg.x0 = x1;	g_drawmsg.y0 = y1;
			g_drawmsg.x1 = x1;	g_drawmsg.y1 = y0;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);

			g_drawmsg.x0 = x1;	g_drawmsg.y0 = y0;
			g_drawmsg.x1 = x0;	g_drawmsg.y1 = y0;
			send(g_sock, (char*)&g_drawmsg, BUFSIZE, 0);
			g_drawmsg.type = DRAWLINE;
		}
		int retval6;
		retval6 = send(g_sock, (char*)&g_erasemsg, BUFSIZE, 0);
		bDrawing = FALSE;
		return 0;

	case WM_DRAWIT:
		hDC = GetDC(hWnd);
		hPen = CreatePen(PS_SOLID, g_drawsize, g_drawcolor);

		// 화면에 그리기
		hOldPen = (HPEN)SelectObject(hDC, hPen);
		MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam), NULL);
		LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
		SelectObject(hDC, hOldPen);

		// 메모리 비트맵에 그리기
		hOldPen = (HPEN)SelectObject(hDCMem, hPen);
		MoveToEx(hDCMem, LOWORD(wParam), HIWORD(wParam), NULL);
		LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
		SelectObject(hDC, hOldPen);
		DeleteObject(hPen);
		ReleaseDC(hWnd, hDC);
		return 0;

	case WM_ERASEIT:
		g_drawsize = g_erasemsg.size;
		return 0;


	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);
		// 메모리 비트맵에 저장된 그림을 화면에 전송
		GetClientRect(hWnd, &rect);
		BitBlt(hDC, 0, 0, rect.right - rect.left,
			rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);
		EndPaint(hWnd, &ps);
		return 0;

	case WM_DESTROY:
		DeleteObject(hBitmap);
		DeleteDC(hDCMem);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 에디트 컨트롤에 문자열 출력
void DisplayText(char* fmt, ...)
{
	va_list arg;
	va_start(arg, fmt);

	char cbuf[5000];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(g_hEditStatus);
	SendMessage(g_hEditStatus, EM_SETSEL, nLength, nLength);
	SendMessage(g_hEditStatus, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char* buf, int len, int flags)
{
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}
	return (len - left);
}

// 소켓 함수 오류 출력 후 종료
void err_quit(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 소켓 함수 오류 출력
void err_display(char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}
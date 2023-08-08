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

#define BUFSIZE     256                    // ���� �޽��� ��ü ũ��
#define MSGSIZE     (BUFSIZE-sizeof(int))  // ä�� �޽��� �ִ� ����

#define CHATTING    1000                   // �޽��� Ÿ��: ä��
#define DRAWLINE    1001                   // �޽��� Ÿ��: �� �׸���
#define ID			1002				   // �޽��� Ÿ�� : ID ����
#define	LIKE		1003				   // �޽��� Ÿ�� : ���ƿ� �� 
#define FILESEND	1004				   // �޽��� Ÿ�� : ���� �̸�
#define ERASE	    1005				   // �޽��� Ÿ�� : �����
#define WM_DRAWIT   (WM_USER+1)            // ����� ���� ������ �޽���
#define WM_ERASEIT   (WM_USER+2)           // ����� ���� ������ �޽���
#define DRAWSQUARE  1006                   // �޽��� Ÿ��: �簢��
#define DRAWTRIANGLE  1007                 // �޽��� Ÿ��: �ﰢ��

// ���� �޽��� ����
// sizeof(COMM_MSG) == 256
struct COMM_MSG
{
	int  type;
	char dummy[MSGSIZE];
};

// ä�� �޽��� ����
// sizeof(CHAT_MSG) == 256
struct CHAT_MSG
{
	int  type;
	char buf[MSGSIZE];
};

// �� �׸��� �޽��� ����
// sizeof(DRAWLINE_MSG) == 256
struct DRAWLINE_MSG
{
	int  type;
	int  color;
	int  x0, y0;
	int  x1, y1;
	char dummy[BUFSIZE - 6 * sizeof(int)];
};

// ���̵� ���� ����
struct CHAT_ID
{
	int  type;
	char buf[MSGSIZE];
};

// ���ƿ� ����
struct LIKE_COUNT
{
	int  type;
	char countbuf[MSGSIZE];
};
int count = 0;

//���� �����ϱ�
struct FILE_MSG
{
	int  type;
	char fileNameBuf[MSGSIZE];
	char fileDetailBuf[MSGSIZE];
};

//�׸� �����
struct ERASE_MSG
{
	int type;
	int size;
};


//������ �б� ����.
FILE* fpr;


static HINSTANCE     g_hInst;			// ���� ���α׷� �ν��Ͻ� �ڵ�
static HWND          g_hDrawWnd;		// �׸��� �׸� ������
static HWND          g_hButtonSendMsg;	// '�޽��� ����' ��ư
static HWND			 g_hButtonChangeId;	//'����'��ư
static HWND			 g_hButtonLike;		//���ƿ� ��ư
static HWND          g_hEditStatus;		// ���� �޽��� ���
static char          g_ipaddr[64];	    // ���� IP �ּ�
static u_short       g_port;			// ���� ��Ʈ ��ȣ
static BOOL          g_isIPv6;			// IPv4 or IPv6 �ּ�?
static HANDLE        g_hClientThread;	// ������ �ڵ�
static HANDLE        g_hFileThread;		// ������ �ڵ�
static volatile BOOL g_bStart;			// ��� ���� ����
static SOCKET        g_sock;			// Ŭ���̾�Ʈ ����
static HANDLE        g_hReadEvent, g_hWriteEvent; // �̺�Ʈ �ڵ�
static CHAT_MSG      g_chatmsg;			// ä�� �޽��� ����
static CHAT_ID       g_chatid;			// ���̵� ����
static LIKE_COUNT    g_likecount;		//���ƿ� ��
static FILE_MSG		 g_filemsg;			// ���� �̸� ����
static HWND			 g_hButtonFile;		//���� ���� ��ư
static DRAWLINE_MSG  g_drawmsg;			// �� �׸��� �޽��� ����
static HWND			 g_hButtonErase;	//����� ��ư
static ERASE_MSG	 g_erasemsg;		// �� ����� �޽��� ����
static int			 g_drawsize;		//�� ������
static int           g_drawcolor;		// �� �׸��� ����

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);

// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI WriteThread(LPVOID arg);


// �ڽ� ������ ���ν���
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char* fmt, ...);
// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags);
// ���� ��� �Լ�
void err_quit(char* msg);
void err_display(char* msg);

// ���� �Լ�
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// �̺�Ʈ ����
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// ���� �ʱ�ȭ(�Ϻ�)
	g_chatmsg.type = CHATTING;
	g_chatid.type = ID;
	g_filemsg.type = FILESEND;
	g_drawmsg.type = DRAWLINE;
	g_drawmsg.color = RGB(80, 80, 80);
	g_erasemsg.type = ERASE;
	g_erasemsg.size = 3;
	g_likecount.type = LIKE;

	// ��ȭ���� ����
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);


	// �̺�Ʈ ����
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// ���� ����
	WSACleanup();
	return 0;
}

//���ƿ� ������ �� �Լ�.
int countFun() {
	count += 1;
	return count;
}

//���� ���ۿ� ����� �Լ�
void fileDetail(char* msg) {
	// ������ ��ſ� ����� ����
	int retval3;
	char strBuf[BUFSIZE];		// ���Ͽ��� ������ ���ڿ��� ������ ����
	int len;					//���� �����ͱ���
	int count = 0;				//���ڿ��� �� � �ִ��� ���� ����
	char* contents[BUFSIZE];	//���� �̸� �Է¹ް� ���� �޾ƿ���
	fpr = fopen(msg, "r");		//������ ���� ���� fopen�Լ� ���. �б��� r

	while (!feof(fpr)) //������ ���� �ƴ϶��
	{
		contents[count] = fgets(strBuf, BUFSIZE, fpr);
		len = strlen(contents[count]);
		strncpy(g_filemsg.fileDetailBuf, contents[count], BUFSIZE);
		DisplayText("%s\n", g_filemsg.fileDetailBuf);	//ȭ�鿡 ����ϱ�

		retval3 = send(g_sock, (char*)&g_filemsg, BUFSIZE, 0);
		if (retval3 == SOCKET_ERROR) {
			err_display("send()");
			break;
		}
		count++;
	}
	
	if (fpr == NULL) {
		printf("���� ���� ����");
	}
	return ;
}

// ��ȭ���� ���ν���
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

		// ��Ʈ�� �ڵ� ���
		hButtonIsIPv6 = GetDlgItem(hDlg, IDC_ISIPV6);

		hID = GetDlgItem(hDlg, IDC_ID);						// ���Ӱ� ���� ID �Է�â
		g_hButtonChangeId = GetDlgItem(hDlg, IDC_CHANGEID); //ID ���� ��ư

		hFILE = GetDlgItem(hDlg, IDC_FILE);				// file �Է�â
		g_hButtonFile = GetDlgItem(hDlg, IDC_FILESEND);	//���� ���� ��ư

		g_hButtonLike = GetDlgItem(hDlg, IDC_LIKE);		//���ƿ� ��ư

		g_hButtonErase = GetDlgItem(hDlg, IDC_ERASER);	// ����� ��ư

		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditPort = GetDlgItem(hDlg, IDC_PORT);
		hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);
		g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		g_hEditStatus = GetDlgItem(hDlg, IDC_STATUS);

		// ���� ���� ��ư
		hColorBlack = GetDlgItem(hDlg, IDC_COLORBLACK);
		hColorLightGray = GetDlgItem(hDlg, IDC_COLORLIGHTGRAY);
		hColorDeepGray = GetDlgItem(hDlg, IDC_COLORDEEPGRAY);
		hColorPink = GetDlgItem(hDlg, IDC_COLORPINK);
		hColorBlue = GetDlgItem(hDlg, IDC_COLORBLUE);
		// ���� ���� ��ư
		hRec = GetDlgItem(hDlg, IDC_REC);
		hTri = GetDlgItem(hDlg, IDC_TRI);

		// ��Ʈ�� �ʱ�ȭ
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

		// ������ Ŭ���� ���
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

		// �ڽ� ������ ����
		g_hDrawWnd = CreateWindow("MyWndClass", "�׸� �׸� ������", WS_CHILD,
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

			// ���� ��� ������ ����
			g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
			if (g_hClientThread == NULL) {
				MessageBox(hDlg, "Ŭ���̾�Ʈ�� ������ �� �����ϴ�."
					"\r\n���α׷��� �����մϴ�.", "����!", MB_ICONERROR);
				EndDialog(hDlg, 0);
			}
			else {
				EnableWindow(hButtonConnect, FALSE);
				while (g_bStart == FALSE); // ���� ���� ���� ��ٸ�
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

		case IDC_LIKE: // ���ƿ� ��ư ������ ��

			GetDlgItemText(hDlg, IDC_ID, g_chatid.buf, MSGSIZE); // ����� ���̵� ��������
			result = countFun();								 // ���� ���� �Լ� ȣ�� �� ����
			sprintf(g_likecount.countbuf, " %s���� �� �׸��� �����մϴ�. ->  %d liked \n",
				g_chatid.buf, result);							 // g_likecount ����ü ���� ���ۿ� �����ϱ�
			int retval2;
			retval2 = send(g_sock, (char*)&g_likecount, BUFSIZE, 0);//������
			return TRUE;

		case IDC_SENDMSG:
			// �б� �ϷḦ ��ٸ�
			WaitForSingleObject(g_hReadEvent, INFINITE);
			GetDlgItemText(hDlg, IDC_MSG, g_chatmsg.buf, MSGSIZE);
			SetEvent(g_hWriteEvent);
			// ���� �ϷḦ �˸�
			// �Էµ� �ؽ�Ʈ ��ü�� ���� ǥ��
			//���⼭ �޼��� send �غ���!!

			SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			return TRUE;

		case IDC_ERASER: // ���찳 ��ư ������ ��

			g_drawmsg.color = RGB(255, 255, 255);		// ������� �� ����
			g_erasemsg.size = 20;						// �� ������ ����
			
			int retval6;
			retval6 = send(g_sock, (char*)&g_erasemsg, BUFSIZE, 0); //������ ����
			return TRUE;


		case IDC_FILESEND:

			//���� ���� ��ư Ŭ�� ��
			GetDlgItemText(hDlg, IDC_FILE, g_filemsg.fileNameBuf, MSGSIZE); //���� �̸� ��������
			fileDetail((char*)&g_filemsg.fileNameBuf); // ���� �̸� �Ķ���ͷ� ����
		
			SendMessage(hFILE, EM_SETSEL, 0, -1);
			return TRUE;
		
		//���� ���� ���� ��ư
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
		
		// ���� ���� ��ư
		case IDC_REC:
			// �簢��
			g_drawmsg.type = DRAWSQUARE;
			return TRUE;

		case IDC_TRI:
			// �ﰢ��
			g_drawmsg.type = DRAWTRIANGLE;
			return TRUE;

		case IDCANCEL:
			if (MessageBox(hDlg, "������ �����Ͻðڽ��ϱ�?",
				"����", MB_YESNO | MB_ICONQUESTION) == IDYES)
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


// ���� ��� ������ �Լ�
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
	MessageBox(NULL, "������ �����߽��ϴ�.", "����!", MB_ICONINFORMATION);

	// �б� & ���� ������ ����
	HANDLE hThread[2];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	//hThread[2] = CreateThread(NULL, 0, LikeThread, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL) {
		MessageBox(NULL, "�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			"����!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// ������ ���� ���
	retval = WaitForMultipleObjects(2, hThread, FALSE, INFINITE);

	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[0], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	g_bStart = FALSE;

	MessageBox(NULL, "������ ������ �������ϴ�", "�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);

	closesocket(g_sock);
	return 0;
}

// ������ �ޱ�
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
		retval = recvn(g_sock, (char*)&comm_msg, BUFSIZE, 0); // ������ �ޱ�

		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}

		if (comm_msg.type == ID) {
			chat_id = (CHAT_ID*)&comm_msg;
			DisplayText("[%s ��] ", chat_id->buf);
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

// ������ ������
DWORD WINAPI WriteThread(LPVOID arg)
{
	int retval;

	// ������ ������ ���
	while (1) {
		// ���� �Ϸ� ��ٸ���
		WaitForSingleObject(g_hWriteEvent, INFINITE);
		if (strlen(g_chatid.buf) == 0) {
			// '����' ��ư Ȱ��ȭ
			EnableWindow(g_hButtonChangeId, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}
		// ���ڿ� ���̰� 0�̸� ������ ����
		if (strlen(g_chatmsg.buf) == 0) {
			// '�޽��� ����' ��ư Ȱ��ȭ
			EnableWindow(g_hButtonSendMsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}
		;
		// ������ ������
		retval = send(g_sock, (char*)&g_chatid, BUFSIZE, 0);
		retval = send(g_sock, (char*)&g_chatmsg, BUFSIZE, 0);

		if (retval == SOCKET_ERROR) {
			break;
		}
		EnableWindow(g_hButtonChangeId, TRUE);
		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadEvent);
	}

	return 0;
}

// �ڽ� ������ ���ν���
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

		// ȭ���� ������ ��Ʈ�� ����
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// �޸� DC ����
		hDCMem = CreateCompatibleDC(hDC);

		// ��Ʈ�� ���� �� �޸� DC ȭ���� ������� ĥ��
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

			// �� �׸��� �޽��� ������
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

			// �� �׸��� �޽��� ������
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

			// �� �׸��� �޽��� ������
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

		// ȭ�鿡 �׸���
		hOldPen = (HPEN)SelectObject(hDC, hPen);
		MoveToEx(hDC, LOWORD(wParam), HIWORD(wParam), NULL);
		LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
		SelectObject(hDC, hOldPen);

		// �޸� ��Ʈ�ʿ� �׸���
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
		// �޸� ��Ʈ�ʿ� ����� �׸��� ȭ�鿡 ����
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

// ����Ʈ ��Ʈ�ѿ� ���ڿ� ���
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

// ����� ���� ������ ���� �Լ�
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

// ���� �Լ� ���� ��� �� ����
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

// ���� �Լ� ���� ���
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

// MFCApplication1Dlg.cpp: file di implementazione
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include "afxdialogex.h"

#include <iostream>
#include <fstream>
#include <string.h>
#include <sstream>
#include <stdio.h>      /* printf, fgets */
#include <stdlib.h>     /* atoi */
#include <math.h>       /* round, floor, ceil, trunc */
#include <time.h>
#include <algorithm>
#include <vector>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>  // for wcscpy_s, wcscat_s
#include <cstdlib>  // for _countof
#include <errno.h>  // for return values

//#include "AxisExample.h"
//#include "AxisExampleDlg.h"
#include "afxwin.h"
#include "include\Axis.h"
#include "afxdialogex.h"


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

//#include <unistd.h>

int icalled = 0;
int ifirst_connect = 0;
int  daqfinished = 0;
using namespace std;
int iok = 0;



#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

WSADATA wsaData;
int iResult;

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

// Create CAxis object pointer
//---------------------------------------------------------------------------
//printf(" Create Axis pointers ...");
CAxis* g_RobotJ1 = MPI_CreateAxis("X", eUT_ROTARY, eDT_D2);
//CAxis* g_RobotJ2 = MPI_CreateAxis("Y", eUT_LINEAR, eDT_D2);


struct addrinfo* result = NULL;
struct addrinfo hints;

int iSendResult;
char recvbuf[DEFAULT_BUFLEN];
int recvbuflen = DEFAULT_BUFLEN;
char buffer[DEFAULT_BUFLEN];
char sendbuffer[DEFAULT_BUFLEN];
double  oldrealx ;
double  oldrealy ;
bool bHomex, bHomey;



#ifdef _DEBUG
#define new DEBUG_NEW
#endif

char* mystrsep(char** stringp, const char* delim)
{
	char* start = *stringp, * p = start ? strpbrk(start, delim) : NULL;

	if (!p) {
		*stringp = NULL;
	}
	else {
		*p = 0;
		*stringp = p + 1;
	}

	return start;
}

int createsocket() {

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	printf(" initialised winsock \n");

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	printf("created socket to connect to server \n");

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	printf(" set up tcp listen socket \n");
	freeaddrinfo(result);
	printf(" after freeaddrinfo \n");

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}


	printf(" waiting to accept a client socket");
	// Accept a client socket
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// printf("got client socket \n");

	printf(" accepted client socket \n");
	// No longer need server socket
	closesocket(ListenSocket);
	
	// operations done correctly
	return 0;

}



// finestra di dialogo CAboutDlg utilizzata per visualizzare le informazioni sull'applicazione.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dati della finestra di dialogo
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // Supporto DDX/DDV


// Implementazione
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

CMFCApplication1Dlg::CMFCApplication1Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFCAPPLICATION1_DIALOG, pParent)
	, value1(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCApplication1Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, AFX_TIMER_ID_VISIBILITY_TIMER, value1);
}


BEGIN_MESSAGE_MAP(CMFCApplication1Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CMFCApplication1Dlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CMFCApplication1Dlg::OnBnClickedButton2)
	ON_EN_CHANGE(IDC_EDIT1, &CMFCApplication1Dlg::OnEnChangeEdit1)
	ON_EN_CHANGE(IDC_EDIT2, &CMFCApplication1Dlg::OnEnChangeEdit2)
	ON_STN_CLICKED(AFX_TIMER_ID_VISIBILITY_TIMER, &CMFCApplication1Dlg::OnStnClickedTimerIdVisibilityTimer)
	ON_WM_TIMER()
	
	ON_BN_CLICKED(IDOK, &CMFCApplication1Dlg::OnBnClickedOk)
	ON_EN_CHANGE(IDC_EDIT3, &CMFCApplication1Dlg::OnEnChangeEdit3)
	ON_EN_CHANGE(IDC_EDIT4, &CMFCApplication1Dlg::OnEnChangeEdit4)
	ON_BN_CLICKED(IDCANCEL, &CMFCApplication1Dlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// Gestori di messaggi di CMFCApplication1Dlg

BOOL CMFCApplication1Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Aggiungere la voce di menu "Informazioni su..." al menu di sistema.

	// IDM_ABOUTBOX deve trovarsi tra i comandi di sistema.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Impostare l'icona per questa finestra di dialogo.  Il framework non esegue questa operazione automaticamente
	//  se la finestra principale dell'applicazione non è una finestra di dialogo.
	SetIcon(m_hIcon, TRUE);			// Impostare icona grande.
	SetIcon(m_hIcon, FALSE);		// Impostare icona piccola.

	// TODO: aggiungere qui inizializzazione aggiuntiva.
	SetTimer(1, 1000, 0);

	return TRUE;  // restituisce TRUE a meno che non venga impostato lo stato attivo su un controllo.
}

void CMFCApplication1Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// Se si aggiunge alla finestra di dialogo un pulsante di riduzione a icona, per trascinare l'icona sarà necessario
//  il codice sottostante.  Per le applicazioni MFC che utilizzano il modello documento/visualizzazione,
//  questa operazione viene eseguita automaticamente dal framework.

void CMFCApplication1Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // contesto di dispositivo per il disegno

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Centrare l'icona nel rettangolo client.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Disegnare l'icona
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// Il sistema chiama questa funzione per ottenere la visualizzazione del cursore durante il trascinamento
//  della finestra ridotta a icona.
HCURSOR CMFCApplication1Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CMFCApplication1Dlg::OnBnClickedButton1()
{
	
}

void CMFCApplication1Dlg::OnBnClickedButton2()
{
	// TODO: aggiungere qui il codice del gestore di notifiche del controllo

}


void CMFCApplication1Dlg::OnEnChangeEdit1()
{
	// TODO:  Se si tratta di un controllo RICHEDIT, il controllo non
	// invierà questa notifica a meno che non si esegua l'override della funzione CDialogEx::OnInitDialog()
	// e venga eseguita la chiamata a CRichEditCtrl().SetEventMask()
	// con il flag ENM_CHANGE introdotto dall'operatore OR nella maschera.

	// TODO:  Aggiungere qui il codice del gestore di notifiche del controllo

}


void CMFCApplication1Dlg::OnEnChangeEdit2()
{
	// TODO:  Se si tratta di un controllo RICHEDIT, il controllo non
	// invierà questa notifica a meno che non si esegua l'override della funzione CDialogEx::OnInitDialog()
	// e venga eseguita la chiamata a CRichEditCtrl().SetEventMask()
	// con il flag ENM_CHANGE introdotto dall'operatore OR nella maschera.

	// TODO:  Aggiungere qui il codice del gestore di notifiche del controllo
}



void CMFCApplication1Dlg::OnStnClickedTimerIdVisibilityTimer()
{
	// TODO: aggiungere qui il codice del gestore di notifiche del controllo
}

void CMFCApplication1Dlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default

	int nRetCode = 0;
	int iretx = 0;
	int irety = 0;

	

	if (ifirst_connect == 0) {

		// then initialize ethercat communication

		nRetCode = MPI_ConnectMegaulink(true);
		printf(" Connecting to MegaUlink ... \n");
		//Sleep(5000);
		
		ifirst_connect = 1;
		printf(" Connected to MegaUlink \n");
		if (nRetCode == ERR_NO_ERROR) {
			//MessageWin("Success Communication...");
			printf("Success Communication ethercat \n");
			// in degrees !!
			iretx = MPI_InitialAxis(g_RobotJ1, 5000., 1000., 1000., 50000., 100, 10, 500000., 0.);
			printf("InitialAxis code %d ", iretx);
			//irety = MPI_InitialAxis(g_RobotJ2, 50., 100., 100., 2000., 150, 2., 2000., 0.);
			// ifirst_connect = 1;

		} else {
			printf("error connecting ethercat - cannot continue\n");
			//assert(nRetCode == ERR_NO_ERROR);
			iretx = 1;
			irety = 1;
			// return;
		}

	

	} else {

	}

	if (icalled == 0) {
		int ireturn = createsocket();
		if (ireturn == 0) {
			printf(" server socket created \n");
		}
		else {
			printf(" error creating server socket - cannot continue ! \n");
			return;
		}
	}
	
	CTime CurrentTime = CTime::GetCurrentTime();

	int iHours = CurrentTime.GetHour();
	int iMinutes = CurrentTime.GetMinute();
	int iSeconds = CurrentTime.GetSecond();
	//int iTicks = CurrentTime.GetTickCount();

	CString strHours, strMinutes, strSeconds, strTicks;

	if (iHours < 10)
		strHours.Format(_T("0%d"), iHours);
	else
		strHours.Format(_T("%d"), iHours);

	if (iMinutes < 10)
		strMinutes.Format(_T("0%d"), iMinutes);
	else
		strMinutes.Format(_T("%d"), iMinutes);

	if (iSeconds < 10)
		strSeconds.Format(_T("0%d"), iSeconds);
	else
		strSeconds.Format(_T("%d"), iSeconds);

	//strTicks.Format(_T("%d"), iTicks);

	value1.Format(_T("%s:%s:%s"), strHours, strMinutes, strSeconds);
	SetDlgItemText(AFX_TIMER_ID_VISIBILITY_TIMER, value1);

	// update timer window
	UpdateData(FALSE);

	bool alarm = true;
	bool debug = true;

	int line_number = 0;
	
	int ic = icalled;
	CString daqstatus;
	CString ncall;
	CString positions;
	CString realposition;
	int iended = 0;
	int iretmovex = 0;
	int iretmovey = 0;

	if(iok==0) {
		int iloop = icalled;
		printf(" inside do loop %d \n", iloop);

		daqstatus.Format(_T("waiting to receive data from DAQ  \n"));
		SetDlgItemText(IDC_EDIT3, daqstatus);

		ncall.Format(_T("icall = %d \n"), ic);
		SetDlgItemText(IDC_EDIT2, ncall);

		icalled++;
		iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		iloop++;

		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);
			strncpy_s(buffer, recvbuf, iResult);
			printf(" buffer = %s \n", buffer);
			char* linec;
			linec = buffer;
			CString cbuffer = CString(buffer);
		
			positions.Format(_T("buffer = %s \n"), cbuffer);
			SetDlgItemText(IDC_EDIT1, positions);

			char* tok;
			char* command= "      ";
			int tokcount = 0;
			double realx = 0.;
			double realy = 0.;
			double xx;
			double yy;
			/*
			int inumber;
			int ix;
			int iy;
			int ipos;
			double hv;
			int icall;
			*/

			// buffer = command ; x ; y 

			while ((tok = mystrsep(&linec, ";")) != NULL) {
				if (tokcount == 0)  command = tok;
				if (tokcount == 1) {
					xx = atof(tok);
					//realx = (ix * 2.12) + 1.56;
				}
				if (tokcount == 2) {
					yy = atof(tok);
					//realy = (iy * 2.12) + 1.56;
				}

				/* original version
				if (tokcount == 0) icall = atoi(tok);
				if (tokcount == 1) inumber = atoi(tok);
				if (tokcount == 2) {
					ipos = atoi(tok);
					//realx = (ix * 2.12) + 1.56;
				}
				if (tokcount == 3) {
					ix = atoi(tok);
					//realx = (ix * 2.12) + 1.56;
				}
				if (tokcount == 4) {
					iy = atoi(tok);
					//realy = (iy * 2.12) + 1.56;
				}
				if (tokcount == 5) {
					realx = atof(tok);
				}
				if (tokcount == 6) {
						realy = atof(tok);
				}
				if (tokcount == 7) hv = atof(tok);
				*/

				tokcount++;
				//printf(" line %d - token %d = %s \n",line_number,tokcount,tok);
			}

			printf("command is %s \n", command);


			if (strcmp(command,"gohome ")==0) {
				realx = 0.;
				realy = 0.;
				// this means go to home position
				realposition.Format(_T("Position to home : %7.3f , %7.3f\n"), realx, realy);
				SetDlgItemText(IDC_EDIT4, realposition);

				// send to 0,0
				iretmovex = MPI_MoveAbsolute(g_RobotJ1, realx);
				//iretmovey = MPI_MoveAbsolute(g_RobotJ2, realy);
				printf("Absolute moving  x to %0.3f cm. - code = %3d ..\n", realx, iretmovex);
				printf("Absolute moving  y to %0.3f cm. - code = %3d ..\n", realy, iretmovey);

				// CHECK THE MOTOR HAS STOPPED
				bool is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ1, &is_moving);
				}
				printf(" motor movement has stopped \n");

				// set home for motor x
				MPI_StartHome(g_RobotJ1);
				// MessageWin("Home process is started", g_RobotJ1);
				printf(" Home process started for motor x \n");

				MPI_WaitHomeOver(g_RobotJ1, 25000);
				// MessageWin("Home process is completed", g_RobotJ1);

				printf(" Home process is completed for motor x \n");

				/*
				// set home for motor y
				MPI_StartHome(g_RobotJ2);
				// MessageWin("Home process is started", g_RobotJ2);
				printf(" Home process started for motor y \n");
				

				MPI_WaitHomeOver(g_RobotJ2, 25000);
				// MessageWin("Home process is completed", g_RobotJ2);
				printf(" Home process is completed for motor y \n");

				*/

				// then verify they have arrived 
				MPI_GetFeedbackPos(g_RobotJ1, &realx);
				//MPI_GetFeedbackPos(g_RobotJ2, &realy);

				printf(" feedback positions x = %0.3f ; y = %0.3f cm \n", realx, realy);

				oldrealx = 0.0;
				oldrealy = 0.0;

				// set at start bHomex to true
				bHomex = true;
				bHomey = true;

				sprintf_s(sendbuffer, 100, "homed ; %7.3f ; %7.3f \n", realx, realy);
				//int sendlenght=
				// Echo the buffer back to the sender
				iSendResult = send(ClientSocket, sendbuffer, iResult, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					daqstatus.Format(_T("error sending data to DAQ - closing the socket  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);

					iok = 1;
					return;
				} else {

					printf("Bytes sent: %d\n", iSendResult);
					daqstatus.Format(_T("transmitted data to DAQ  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);
				}

				realposition.Format(_T("Moved to home position : %7.3f , %7.3f\n"), realx, realy);
				SetDlgItemText(IDC_EDIT4, realposition);

				// Sleep(4000);


			} 
			
			if (strcmp(command,"goabs ")==0) {
				bool is_moving = true;

				// translate crystal number to position
				realx = (xx * 2.12) + 1.56;
				realy = (yy * 2.12) + 1.56;
				// positions are in cm -> translate to degrees as needed by HIWIN
				double realxdeg = 288000. * realx / 80.;
				double realydeg = 288000. * realy / 80.;
		
				// move to real position 
				realposition.Format(_T("Absolute Position moved to : %7.3f , %7.3f\n"), realx, realy);
				SetDlgItemText(IDC_EDIT4, realposition);

				// check if x is at home pos
				// MPI_IsHomed(g_RobotJ1, &bHomex);
				iretmovex = MPI_MoveAbsolute(g_RobotJ1, realxdeg);
				printf("Absolute moving  x to %0.3f cm. - code = %3d ..\n", realx,iretmovex);

				// CHECK THE MOTOR HAS STOPPED
				is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ1, &is_moving);
				}
				printf(" motor X movement has stopped \n");

				//wait till operation completed
				// Sleep(10000);

				/*
				// check if y is at home pos
				// MPI_IsHomed(g_RobotJ2, &bHomey);
				iretmovey = MPI_MoveAbsolute(g_RobotJ2, realydeg);
				printf("Absolute moving  y to %0.3f mm. - code = %3d ..\n", realy,iretmovey);

				// CHECK THE MOTOR HAS STOPPED
				is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ2, &is_moving);
				}
				printf(" motor Y movement has stopped \n");


				*/

				// then verify they have arrived 
				MPI_GetFeedbackPos(g_RobotJ1, &realxdeg);
				//MPI_GetFeedbackPos(g_RobotJ2, &realy);

				// and translate back to cm
				realx = realxdeg * 80. / 288000.;
				realy = realydeg * 80. / 288000.;
				printf(" feedback positions x = %0.3f ; y = %0.3f cm \n", realx, realy);
				
				oldrealx = realx;
				oldrealy = realy;

				sprintf_s(sendbuffer, 100, "absmv ; %7.3f ; %7.3f ; %3d ; %3d  \n", realx, realy, iretmovex, iretmovey);
				int lenbuff = strlen(sendbuffer);
				// Echo the buffer back to the sender
				iSendResult = send(ClientSocket, sendbuffer, lenbuff, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					daqstatus.Format(_T("error sending data to DAQ - closing the socket  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);

					iok = 1;
					return;
				} else {

					printf("Bytes sent: %d\n", iSendResult);
					daqstatus.Format(_T("transmitted data to DAQ  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);
				}

			}
	
			if (strcmp(command, "close ") == 0) {
				printf(" closing connection \n");
				printf("closing socket \n");
				closesocket(ClientSocket);

				// send motor to 0,0
				// send to 0,0
				iretmovex = MPI_MoveAbsolute(g_RobotJ1, realx);
				//iretmovey = MPI_MoveAbsolute(g_RobotJ2, realy);
				printf("Absolute moving  x to %0.3f cm. - code = %3d ..\n", realx, iretmovex);
				printf("Absolute moving  y to %0.3f cm. - code = %3d ..\n", realy, iretmovey);

				
				// CHECK THE MOTOR HAS STOPPED
				bool is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ1,&is_moving);
				}
				printf(" motor X movement has stopped \n");

				// wait till movement complete
				//Sleep(10000);


				// set home for motor x
				MPI_StartHome(g_RobotJ1);
				// MessageWin("Home process is started", g_RobotJ1);
				printf(" Home process started for motor x \n");
				MPI_WaitHomeOver(g_RobotJ1, 25000);
				// MessageWin("Home process is completed", g_RobotJ1);
				printf(" Home process is completed for motor x \n");

				/*
				// set home for motor y
				MPI_StartHome(g_RobotJ2);
				// MessageWin("Home process is started", g_RobotJ2);
				printf(" Home process started for motor y \n");
				MPI_WaitHomeOver(g_RobotJ2, 25000);
				// MessageWin("Home process is completed", g_RobotJ2);
				printf(" Home process is completed for motor y \n");
				*/

				// then verify they have arrived 
				MPI_GetFeedbackPos(g_RobotJ1, &realx);
				//MPI_GetFeedbackPos(g_RobotJ2, &realy);

				printf(" Home feedback positions x = %0.3f ; y = %0.3f cm \n", realx, realy);

				realposition.Format(_T("Closed connection - waiting for new data from client \n"));
				SetDlgItemText(IDC_EDIT3, realposition);
				realposition.Format(_T("Moved motors to home position \n"));
				SetDlgItemText(IDC_EDIT4, realposition);


				WSACleanup();

				// and reset all counters and restart
				icalled = 0;
				iok = 0;
				ifirst_connect = 1;

			}

			if (strcmp(command, "gorel ") == 0) {
				bool is_moving = true;

				// do not translate this :it's already new relative position
				realx = xx;
				realy = yy;
				// positions are in cm -> translate to degrees as needed by HIWIN
				double realxdeg = 288000. * realx / 80.;
				double realydeg = 288000. * realy / 80.;

				// move to real position 
				realposition.Format(_T("Relative Position to move to : %7.3f , %7.3f\n"), realx, realy);
				SetDlgItemText(IDC_EDIT4, realposition);

				double diffx = realx;
				double diffxdeg = realxdeg;
				iretmovex = MPI_MoveRelative(g_RobotJ1, diffxdeg);
				// and recalculate position
				realx = oldrealx + diffx;
				printf("Relative moving x by %0.3f to %0.3f mm. - code = %3d ..\n", diffx, realx, iretmovex);


				// CHECK THE MOTOR HAS STOPPED
				is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ1, &is_moving);
				}
				printf(" motor X movement has stopped \n");

				// wait at least till operation completed
				// Sleep(10000);


				double diffy = realy;
				double diffydeg = realydeg;
				realy = oldrealy + diffy;

				/*
				iretmovey = MPI_MoveRelative(g_RobotJ2, diffydeg);
				// and recalculate position
				printf("Relative moving y by %0.3f to %0.3f mm. - code = %3d ..\n", diffy, realy, iretmovey);
 
				// CHECK THE MOTOR HAS STOPPED
				is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ2, &is_moving);
				}
				printf(" motor Y movement has stopped \n");


				*/

				// then verify they have arrived 
				MPI_GetFeedbackPos(g_RobotJ1, &realxdeg);
				//MPI_GetFeedbackPos(g_RobotJ2, &realydeg);
				
				// and translate back to cm
				realx = realxdeg * 80. / 288000.;
				realy = realydeg * 80. / 288000.;
				printf(" feedback positions x = %0.3f ; y = %0.3f cm \n", realx, realy);


				oldrealx = realx;
				oldrealy = realy;

				sprintf_s(sendbuffer, 100, "relmv ; %7.3f ; %7.3f ; %3d ; %3d  \n", diffx, diffy,iretmovex, iretmovey);
				int lenbuff = strlen(sendbuffer);
				// Echo the buffer back to the sender
				iSendResult = send(ClientSocket, sendbuffer, lenbuff, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					daqstatus.Format(_T("error sending data to DAQ - closing the socket  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);

					iok = 1;
					return;
				}
				else {

					printf("Bytes sent: %d\n", iSendResult);
					daqstatus.Format(_T("transmitted data to DAQ  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);
				}

			}

			if (strcmp(command, "whereis ") == 0) {
				// move to real position 
				realposition.Format(_T("Whereis command received \n"));
				SetDlgItemText(IDC_EDIT4, realposition);

				double newrealx = 0.;
				double newrealy = 0.;
				int iretmovex = 0;
				int iretmovey = 0;
				// then verify where they are
				iretmovex = MPI_GetFeedbackPos(g_RobotJ1, &newrealx);
				//iretmovey = MPI_GetFeedbackPos(g_RobotJ2, &newrealy);

				// and convert to cm
				// and translate back to cm
				newrealx = newrealx * 80. / 288000.;
				newrealy = newrealy * 80. / 288000.;
				printf(" feedback positions x = %0.3f ; y = %0.3f cm \n", newrealx, newrealy);

				// Sleep(2000);

				printf("whereis : %7.3f , %7.3f \n", newrealx, newrealy);

				sprintf_s(sendbuffer, 100, "where ; %7.3f ; %7.3f ; %3d ; %3d \n", newrealx, newrealy, iretmovex, iretmovey);
				int lenbuff = strlen(sendbuffer);
				// Echo the buffer back to the sender
				iSendResult = send(ClientSocket, sendbuffer, lenbuff, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					daqstatus.Format(_T("error sending data to DAQ - closing the socket  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);

					iok = 1;
					return;
				} else {

					printf("Bytes sent: %d\n", iSendResult);
					daqstatus.Format(_T("transmitted data to DAQ  \n"));
					SetDlgItemText(IDC_EDIT3, daqstatus);
				}

			}

			// update timer window
			UpdateData(FALSE);



		} else if (iResult == 0) {
			
			//if (iok == 0) {
				printf("Resetting everything - Waiting for data from client ... \n");
				daqstatus.Format(_T("Reset everything - Waiting for data from client ... \n"));
				SetDlgItemText(IDC_EDIT3, daqstatus);
				
				
				// and home motors
				double realx = 0.;
				double realy = 0.;
				// this means go to home position
				realposition.Format(_T("Position to home : %7.3f , %7.3f\n"), realx, realy);
				SetDlgItemText(IDC_EDIT4, realposition);

				// send to 0,0
				iretmovex = MPI_MoveAbsolute(g_RobotJ1, realx);
				//iretmovey = MPI_MoveAbsolute(g_RobotJ2, realy);
				printf("Absolute moving  x to %0.3f cm. - code = %3d ..\n", realx, iretmovex);
				printf("Absolute moving  y to %0.3f cm. - code = %3d ..\n", realy, iretmovey);

				// CHECK THE MOTOR HAS STOPPED
				bool is_moving = true;
				while (is_moving) {
					Sleep(1000);
					iretmovex = MPI_IsMoving(g_RobotJ1, &is_moving);
				}			
				
				// set home for motor x
				MPI_StartHome(g_RobotJ1);
				// MessageWin("Home process is started", g_RobotJ1);
				printf(" Home process started for motor x \n");
				Sleep(2000);
				MPI_WaitHomeOver(g_RobotJ1, 25000);
				// MessageWin("Home process is completed", g_RobotJ1);
				printf(" Home process is completed for motor x \n");

				/*
				// set home for motor y
				MPI_StartHome(g_RobotJ2);
				// MessageWin("Home process is started", g_RobotJ2);
				printf(" Home process started for motor y \n");
				Sleep(2000);
				MPI_WaitHomeOver(g_RobotJ2, 25000);
				// MessageWin("Home process is completed", g_RobotJ2);
				printf(" Home process is completed for motor y \n");
				*/

				realposition.Format(_T("Closed connection - waiting for new data from client \n"));
				SetDlgItemText(IDC_EDIT3, realposition);
				realposition.Format(_T("Reset connection - Moved motors to home position \n"));
				SetDlgItemText(IDC_EDIT4, realposition);


				// destroy axis pointers
				printf("Destroy axis pointers \n");
				MPI_DestroyAxis(g_RobotJ1);
				//MPI_DestroyAxis(g_RobotJ2);

				printf("Disconncting MegaUlink \n");
				// and close connection to MegaUlink
				nRetCode = MPI_Disconnect();
				if (nRetCode == ERR_NO_ERROR) {
					//MessageWin("Success Communication...");
					printf("Success Disconnecting ethercat \n");
					iretx = 0;
					irety = 0;
				} else {
					printf("error disconnecting ethercat \n");
					//assert(nRetCode == ERR_NO_ERROR);
					iretx = 1;
					irety = 1;
				}

				// reset everything and restart
				WSACleanup();

				// and reset all counters and restart
				icalled = 0;
				iok = 0;
				ifirst_connect = 0;
		
			//} 	
	
		} else {
			printf("recv failed with error: %d\n", WSAGetLastError());
			printf("closing socket and exiting \n");
			closesocket(ClientSocket);
			WSACleanup();
			daqstatus.Format(_T("recv from daq error - closing socket  \n"));
			SetDlgItemText(IDC_EDIT3, daqstatus);
			iok = 1;
			return;
			
		}  //end if iResult


	}   

	
	CDialogEx::OnTimer(nIDEvent);

	// and set motors to home position
}



void CMFCApplication1Dlg::OnBnClickedOk()
{
	// TODO: aggiungere qui il codice del gestore di notifiche del controllo
	CDialogEx::OnOK();
	
	printf("closing socket and exiting \n");
	closesocket(ClientSocket);
	printf("doing cleanup \n");
	WSACleanup();
	iResult = -10;

}


void CMFCApplication1Dlg::OnEnChangeEdit3()
{
	// TODO:  Se si tratta di un controllo RICHEDIT, il controllo non
	// invierà questa notifica a meno che non si esegua l'override della funzione CDialogEx::OnInitDialog()
	// e venga eseguita la chiamata a CRichEditCtrl().SetEventMask()
	// con il flag ENM_CHANGE introdotto dall'operatore OR nella maschera.

	// TODO:  Aggiungere qui il codice del gestore di notifiche del controllo
}




void CMFCApplication1Dlg::OnEnChangeEdit4()
{
	// TODO:  Se si tratta di un controllo RICHEDIT, il controllo non
	// invierà questa notifica a meno che non si esegua l'override della funzione CDialogEx::OnInitDialog()
	// e venga eseguita la chiamata a CRichEditCtrl().SetEventMask()
	// con il flag ENM_CHANGE introdotto dall'operatore OR nella maschera.

	// TODO:  Aggiungere qui il codice del gestore di notifiche del controllo
}


void CMFCApplication1Dlg::OnBnClickedCancel()
{
	// TODO: aggiungere qui il codice del gestore di notifiche del controllo
	CDialogEx::OnCancel();
}

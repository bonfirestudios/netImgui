//=================================================================================================
// Application.cpp
//
// Entry point of this application. 
// Initialize the app & window, and run the mainloop with messages processing
//=================================================================================================
#include "stdafx.h"
#include <array>
#include <chrono>
#include "../resource.h"
#include "../DirectX/DirectX11.h"
#include "ServerNetworking.h"
#include "ServerInfoTab.h"
#include "RemoteClient.h"

#define MAX_LOADSTRING		100
#define CLIENT_MAX			8
#define CMD_CLIENTFIRST_ID	1000
#define CMD_CLIENTLAST_ID	(CMD_CLIENTFIRST_ID + CLIENT_MAX - 1)

// Global Variables:
HINSTANCE							ghApplication;					// Current instance
HWND								ghMainWindow;					// Main Windows
int32_t								gActiveClient = -1;				// Currently selected remote Client for input & display
WCHAR								szTitle[MAX_LOADSTRING];		// The title bar text
WCHAR								szWindowClass[MAX_LOADSTRING];	// the main window class name
std::array<ClientRemote,CLIENT_MAX> gClients;						// Table of all potentially connected clients to this server
InputUpdate							gAppInput;
LPTSTR								gMouseCursor		= 0;
bool								gMouseCursorOwner	= false;
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void UpdateActiveClient(int NewClientId)
{
	if( NewClientId != gActiveClient )
	{
		char zName[96];
		if( gActiveClient != -1 )
		{
			ClientRemote& Client = gClients[gActiveClient];
			sprintf_s(zName, "    %s    ", Client.mName);
			ModifyMenuA(GetMenu(ghMainWindow), (UINT)Client.mMenuId, MF_BYCOMMAND|MF_ENABLED, Client.mMenuId, zName);
			Client.mbIsActive = false;
		}
		
		gActiveClient = NewClientId;
		if( gActiveClient != -1 )
		{
			ClientRemote& Client = gClients[gActiveClient];
			sprintf_s(zName, "--[ %s ]--", Client.mName);
			ModifyMenuA(GetMenu(ghMainWindow), (UINT)Client.mMenuId, MF_BYCOMMAND|MF_GRAYED, Client.mMenuId, zName);
			Client.mbIsActive = true;
		}		
		DrawMenuBar(ghMainWindow);
	}
}

void AddRemoteClient(int32_t NewClientIndex)
{	
	gClients[NewClientIndex].mMenuId = CMD_CLIENTFIRST_ID + NewClientIndex;
	AppendMenuA(GetMenu(ghMainWindow), MF_STRING, gClients[NewClientIndex].mMenuId, "");
	UpdateActiveClient(NewClientIndex);
}

void RemoveRemoteClient(int32_t OldClientIndex)
{
	if( gClients[OldClientIndex].mMenuId != 0 )
	{
		RemoveMenu(GetMenu(ghMainWindow), (UINT)gClients[OldClientIndex].mMenuId, MF_BYCOMMAND);
		gClients[OldClientIndex].Reset();
		if( OldClientIndex == gActiveClient )
			UpdateActiveClient(0);
	}
}

BOOL Startup(HINSTANCE hInstance, int nCmdShow)
{
    // Initialize global strings	
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NETIMGUIAPP, szWindowClass, MAX_LOADSTRING);
    WNDCLASSEXW wcex;
    wcex.cbSize			= sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NETIMGUIAPP));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_NETIMGUIAPP);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
	if( !RegisterClassExW(&wcex) )
		return false;
	
	ghApplication		= hInstance;
	ghMainWindow		= CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, ghApplication, nullptr);

	if (!ghMainWindow)
		return false;

	if( !dx::Startup(ghMainWindow) )
		return false;

	memset(&gAppInput, 0, sizeof(gAppInput));
	ShowWindow(ghMainWindow, nCmdShow);
	UpdateWindow(ghMainWindow);
	return true;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//=================================================================================================
// WNDPROC
// Process windows messages
// Mostly insterested in active 'tab' change and inputs ('char', 'Mousewheel') that needs
// to be forwarded to remote imgui client
//=================================================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if( hWnd != ghMainWindow )
		return DefWindowProc(hWnd, message, wParam, lParam);

    switch (message)
    {    
    case WM_DESTROY:		PostQuitMessage(0); break;
    case WM_MOUSEWHEEL:		gAppInput.mMouseWheelVertPos	+= (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA; return true;
    case WM_MOUSEHWHEEL:    gAppInput.mMouseWheelHorizPos	+= (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA; return true;
	case WM_COMMAND:
    {
		// Parse the menu selections:
        int wmId = LOWORD(wParam);
		if( wmId >= CMD_CLIENTFIRST_ID && wmId <= CMD_CLIENTLAST_ID )
		{
			UpdateActiveClient(wmId - CMD_CLIENTFIRST_ID);
		}
		else
		{
			switch (wmId)
			{
			case IDM_ABOUT:	DialogBox(ghApplication, MAKEINTRESOURCE(IDD_ABOUTBOX), ghMainWindow, About);	break;
			case IDM_EXIT:	DestroyWindow(ghMainWindow); break;
			default: return DefWindowProc(ghMainWindow, message, wParam, lParam);
			}
		}
    }
    break;
	case WM_SETCURSOR:
	{
		gMouseCursorOwner = LOWORD(lParam) == HTCLIENT;
		if( gMouseCursorOwner )
			return true;	// Eat cursor command to prevent change
		gMouseCursor = 0;		
	}
	break;
    case WM_CHAR:
	{
        if (wParam > 0 && wParam < 0x10000 && gAppInput.mKeyCount < sizeof(gAppInput.mKeys)/sizeof(gAppInput.mKeys[0]) )
			gAppInput.mKeys[ gAppInput.mKeyCount++ ] = (unsigned short)wParam;        
        return true;
	}
    default: return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

//=================================================================================================
// wWINMAIN
// Main program loop
// Startup, run Main Loop (while active) then Shutdown
//=================================================================================================
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	//-------------------------------------------------------------------------
    // Perform application initialization:
	//-------------------------------------------------------------------------
    if (	!Startup (hInstance, nCmdShow) || 
			!NetworkServer::Startup(&gClients[0], static_cast<uint32_t>(gClients.size()), NetImgui::kDefaultServerPort ) ||
			!ServerInfoTab_Startup(NetImgui::kDefaultServerPort) )
    {
		return FALSE;
    }

	//-------------------------------------------------------------------------
    // Main message loop:
	//-------------------------------------------------------------------------
	MSG msg;
    ZeroMemory(&msg, sizeof(msg));
	auto lastTime = std::chrono::high_resolution_clock::now();	
    while (msg.message != WM_QUIT)
    {
		// Process windows messages 
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))		
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
			continue;
        }
		
		// Update UI for new connected or disconnected Clients
		for(uint32_t ClientIdx(0); ClientIdx<static_cast<uint32_t>(gClients.size()); ClientIdx++)
		{
			auto& Client = gClients[ClientIdx];
			if( Client.mbConnected == true && Client.mMenuId == 0 && Client.mName[0] != 0 )
				AddRemoteClient(ClientIdx);
			if( Client.mbConnected == false && Client.mMenuId != 0 )
				RemoveRemoteClient(ClientIdx);
		}

		// Render the Imgui client currently active
		auto currentTime = std::chrono::high_resolution_clock::now();
		if( std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count() > 4 )
		{
			// Redraw the ImGui Server tab with infos.
			// This tab works exactly like a remote client connected to this server
			ServerInfoTab_Draw();	

			// Gather keyboard/mouse status and collected infos from window message loop, 
			// and send theses Input to active Client (selected tab).
			// Note: Client will only emit DrawData when they receive this input command first
			gClients[gActiveClient].UpdateInputToSend(ghMainWindow, gAppInput);

			// Render the last received ImGui DrawData from active client
			auto pDrawCmd = gClients[gActiveClient].GetDrawFrame();
			dx::Render( gClients[gActiveClient].mvTextures, pDrawCmd);
			
			// Update the mouse cursor to reflect the one ImGui expect			
			if( pDrawCmd )
			{
				LPTSTR wantedCursor = IDC_ARROW;
				switch (pDrawCmd->mMouseCursor)
				{
				case ImGuiMouseCursor_Arrow:        wantedCursor = IDC_ARROW; break;
				case ImGuiMouseCursor_TextInput:    wantedCursor = IDC_IBEAM; break;
				case ImGuiMouseCursor_ResizeAll:    wantedCursor = IDC_SIZEALL; break;
				case ImGuiMouseCursor_ResizeEW:     wantedCursor = IDC_SIZEWE; break;
				case ImGuiMouseCursor_ResizeNS:     wantedCursor = IDC_SIZENS; break;
				case ImGuiMouseCursor_ResizeNESW:   wantedCursor = IDC_SIZENESW; break;
				case ImGuiMouseCursor_ResizeNWSE:   wantedCursor = IDC_SIZENWSE; break;
				case ImGuiMouseCursor_Hand:         wantedCursor = IDC_HAND; break;
				case ImGuiMouseCursor_NotAllowed:   wantedCursor = IDC_NO; break;
				default:							wantedCursor = IDC_ARROW; break;
				}
		
				if( gMouseCursorOwner && gMouseCursor != wantedCursor )
				{
					gMouseCursor = wantedCursor;
					::SetCursor(::LoadCursor(NULL, gMouseCursor));
				}
			}
			
			lastTime = currentTime;
		}
		
    }

	//-------------------------------------------------------------------------
	// Release all resources
	//-------------------------------------------------------------------------
	ServerInfoTab_Shutdown();
	NetworkServer::Shutdown();
	dx::Shutdown();

	for(auto& client : gClients)
		client.Reset();

    return (int) msg.wParam;
}
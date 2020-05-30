//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//===========================================================================//

#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <direct.h>
#include "sys_utils.h"
#include <io.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <tier0/dbg.h>

#ifdef SendMessage
#undef SendMessage
#endif
																
#include "filesystem.h"
#include "GameUI_Interface.h"
#include "string.h"
#include "tier0/icommandline.h"

// interface to engine
#include "EngineInterface.h"

#include "VGuiSystemModuleLoader.h"
#include "bitmap/tgaloader.h"

#include "GameConsole.h"
#include "LoadingDialog.h"
#include "CDKeyEntryDialog.h"
#include "ModInfo.h"
#include "game/client/IGameClientExports.h"
#include "materialsystem/imaterialsystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ixboxsystem.h"
#include "iachievementmgr.h"
#include "IGameUIFuncs.h"
#include "ienginevgui.h"
#include "video/ivideoservices.h"

/*
#include "VMainMenu.h"
#include "VInGameMainMenu.h"
#include "VGenericConfirmation.h"
#include "VFooterPanel.h"
*/

// vgui2 interface
// note that GameUI project uses ..\vgui2\include, not ..\utils\vgui\include
#include "vgui/Cursor.h"
#include "tier1/KeyValues.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "vgui/ISystem.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/PHandle.h"
#include "tier3/tier3.h"
#include "matsys_controls/matsyscontrols.h"
#include "steam/steam_api.h"
//#include "protocol.h"
#include "game/server/iplayerinfo.h"
#include "avi/iavi.h"

#include <vgui/IInput.h>

/*
#include "BaseModPanel.h"
#include "basemodui.h"
typedef BaseModUI::CBaseModPanel UI_BASE_PANEL_CLASS;
inline UI_BASE_PANEL_CLASS & GetBasePanel() { return UI_BASE_PANEL_CLASS::GetSingleton(); }
inline UI_BASE_PANEL_CLASS & ConstructGetBasePanel() { return * new UI_BASE_PANEL_CLASS(); }
using namespace BaseModUI;
*/

// #include "BasePanel.h"
IBasePanel* gBasePanel = NULL;
inline IBasePanel* GetBasePanel() { return gBasePanel; }
// inline UI_BASE_PANEL_CLASS & ConstructGetBasePanel() { return *new CBasePanel(); }

ConVar of_pausegame( "of_pausegame", "0", FCVAR_ARCHIVE | FCVAR_REPLICATED, "If set, pauses whenever you open the in game menu." );;

ConVar ui_scaling("ui_scaling", "0", FCVAR_REPLICATED | FCVAR_NOTIFY, "Scales VGUI elements with different screen resolutions.");

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif // _X360

#include "tier0/dbg.h"
#include "engine/IEngineSound.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef GAMEUI_EMBEDDED
IVEngineClient *engine = NULL;
IGameUIFuncs *gameuifuncs = NULL;
CGlobalVarsBase *gpGlobals = NULL;
IEngineSound *enginesound = NULL;
ISoundEmitterSystemBase *soundemitterbase = NULL;
IXboxSystem *xboxsystem = NULL;

static CSteamAPIContext g_SteamAPIContext;
CSteamAPIContext *steamapicontext = &g_SteamAPIContext;
#endif

IEngineVGui *enginevguifuncs = NULL;
#ifdef _X360
IXOnline  *xonline = NULL;			// 360 only
#endif
vgui::ISurface *enginesurfacefuncs = NULL;
IAchievementMgr *achievementmgr = NULL;

class CGameUI;
CGameUI *g_pGameUI = NULL;

class CLoadingDialog;
vgui::DHANDLE<CLoadingDialog> g_hLoadingDialog;
vgui::VPANEL g_hLoadingBackgroundDialog = NULL;

static CGameUI g_GameUI;
static WHANDLE g_hMutex = NULL;
static WHANDLE g_hWaitMutex = NULL;


static IGameClientExports *g_pGameClientExports = NULL;
IGameClientExports *GameClientExports()
{
	return g_pGameClientExports;
}

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CGameUI &GameUI()
{
	return g_GameUI;
}

//-----------------------------------------------------------------------------
// Purpose: hack function to give the module loader access to the main panel handle
//			only used in VguiSystemModuleLoader
//-----------------------------------------------------------------------------
vgui::VPANEL GetGameUIBasePanel()
{
	if (!GetBasePanel())
	{
		// Assert(0);
		return 0;
	}
	return GetBasePanel()->GetVguiPanel().GetVPanel();
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameUI, IGameUI, GAMEUI_INTERFACE_VERSION, g_GameUI);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameUI::CGameUI()
{
	g_pGameUI = this;
	m_bTryingToLoadFriends = false;
	m_iFriendsLoadPauseFrames = 0;
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_iGameQueryPort = 0;
	m_bActivatedUI = false;
	m_szPreviousStatusText[0] = 0;
	m_bIsConsoleUI = false;
	m_bHasSavedThisMenuSession = false;
	m_bOpenProgressOnStart = false;
	m_iPlayGameStartupSound = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUI::~CGameUI()
{
	g_pGameUI = NULL;
}

//#pragma message(FILE_LINE_STRING " !!FIXME!! replace all this with Sys_LoadGameModule")
void *GetGameInterface(const char *dll, const char *name)
{
	const char *pGameDir = CommandLine()->ParmValue("-game", "hl2");
	pGameDir = VarArgs("%s/bin/%s", pGameDir, dll);
	CSysModule *module = Sys_LoadModule(pGameDir);
	CreateInterfaceFn factory = Sys_GetFactory(module);
	return factory(name, nullptr);
}
	
KeyValues* gBackgroundSettings;
KeyValues* BackgroundSettings()
{
	return gBackgroundSettings;
}

void InitBackgroundSettings()
{
	if( gBackgroundSettings )
	{
		gBackgroundSettings->deleteThis();
	}
	gBackgroundSettings = new KeyValues( "MenuBackgrounds" );
	gBackgroundSettings->LoadFromFile( g_pFullFileSystem, "scripts/menu_backgrounds.txt" );
}

//-----------------------------------------------------------------------------
// Purpose: Initialization
//-----------------------------------------------------------------------------
void CGameUI::Initialize( CreateInterfaceFn factory )
{
	MEM_ALLOC_CREDIT();
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	ConVar_Register( FCVAR_CLIENTDLL );
	ConnectTier3Libraries( &factory, 1 );

	//#pragma message(FILE_LINE_STRING " !!FIXME!!")
	gpGlobals = ((IPlayerInfoManager *)GetGameInterface("server.dll", INTERFACEVERSION_PLAYERINFOMANAGER))->GetGlobalVars();

	gameuifuncs = (IGameUIFuncs *)factory(VENGINE_GAMEUIFUNCS_VERSION, NULL);
	soundemitterbase = (ISoundEmitterSystemBase *)factory(SOUNDEMITTERSYSTEM_INTERFACE_VERSION, NULL);
	xboxsystem = (IXboxSystem *)factory(XBOXSYSTEM_INTERFACE_VERSION, NULL);

	enginesound = (IEngineSound *)factory(IENGINESOUND_CLIENT_INTERFACE_VERSION, NULL);
	engine = (IVEngineClient *)factory( VENGINE_CLIENT_INTERFACE_VERSION, NULL );

	g_pVideo = (IVideoServices *)factory(VIDEO_SERVICES_INTERFACE_VERSION, NULL);

#if !defined _X360 && !defined NO_STEAM
	SteamAPI_InitSafe();
	steamapicontext->Init();
#endif

	CGameUIConVarRef var( "gameui_xbox" );
	m_bIsConsoleUI = var.IsValid() && var.GetBool();

	vgui::VGui_InitInterfacesList( "GameUI", &factory, 1 );
	vgui::VGui_InitMatSysInterfacesList( "GameUI", &factory, 1 );

	// load localization file
	g_pVGuiLocalize->AddFile( "Resource/gameui_%language%.txt", "GAME", true );

	// load mod info
	ModInfo().LoadCurrentGameInfo();

	// load localization file for kb_act.lst
	g_pVGuiLocalize->AddFile( "Resource/valve_%language%.txt", "GAME", true );

	//bool bFailed = false;
	enginevguifuncs = (IEngineVGui *)factory( VENGINE_VGUI_VERSION, NULL );
	enginesurfacefuncs = (vgui::ISurface *)factory(VGUI_SURFACE_INTERFACE_VERSION, NULL);
	gameuifuncs = (IGameUIFuncs *)factory( VENGINE_GAMEUIFUNCS_VERSION, NULL );
	xboxsystem = (IXboxSystem *)factory( XBOXSYSTEM_INTERFACE_VERSION, NULL );
#ifdef _X360
	xonline = (IXOnline *)factory( XONLINE_INTERFACE_VERSION, NULL );
#endif
	if (!enginesurfacefuncs || !gameuifuncs || !enginevguifuncs || !xboxsystem)
	{
		Warning( "CGameUI::Initialize() failed to get necessary interfaces\n" );
	}
	
	InitBackgroundSettings();
}

void CGameUI::PostInit()
{
	if ( IsX360() )
	{
		enginesound->PrecacheSound( "player/suit_denydevice.wav", true, true );

		enginesound->PrecacheSound( "UI/buttonclick.wav", true, true );
		enginesound->PrecacheSound( "UI/buttonrollover.wav", true, true );
		enginesound->PrecacheSound( "UI/buttonclickrelease.wav", true, true );
	}

#if 0
	// to know once client dlls have been loaded
	BaseModUI::CUIGameData::Get()->OnGameUIPostInit();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Sets the specified panel as the background panel for the loading
//		dialog.  If NULL, default background is used.  If you set a panel,
//		it should be full-screen with an opaque background, and must be a VGUI popup.
//-----------------------------------------------------------------------------
void CGameUI::SetLoadingBackgroundDialog( vgui::VPANEL panel )
{
	g_hLoadingBackgroundDialog = panel;
}

//-----------------------------------------------------------------------------
// Purpose: connects to client interfaces
//-----------------------------------------------------------------------------
void CGameUI::Connect( CreateInterfaceFn gameFactory )
{
	g_pGameClientExports = (IGameClientExports *)gameFactory(GAMECLIENTEXPORTS_INTERFACE_VERSION, NULL);

	achievementmgr = engine->GetAchievementMgr();

	if (!g_pGameClientExports)
	{
		Warning("CGameUI::Initialize() failed to get necessary interfaces\n");
	}

	m_GameFactory = gameFactory;
}

#ifdef _WIN32
//-----------------------------------------------------------------------------
// Purpose: Callback function; sends platform Shutdown message to specified window
//-----------------------------------------------------------------------------
int __stdcall SendShutdownMsgFunc(WHANDLE hwnd, int lparam)
{
	Sys_PostMessage(hwnd, Sys_RegisterWindowMessage("ShutdownValvePlatform"), 0, 1);
	return 1;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Searches for GameStartup*.mp3 files in the sound/ui folder and plays one
//-----------------------------------------------------------------------------
void CGameUI::PlayGameStartupSound()
{
#if defined( LEFT4DEAD )
	// L4D not using this path, L4D UI now handling with background menu movies
	return;
#endif

	if ( IsX360() )
		return;

	if ( CommandLine()->FindParm( "-nostartupsound" ) )
		return;

	FileFindHandle_t fh;

	CUtlVector<char *> fileNames;

	char path[ 512 ];
	Q_snprintf( path, sizeof( path ), "sound/ui/gamestartup*.mp3" );
	Q_FixSlashes( path );

	char const *fn = g_pFullFileSystem->FindFirstEx( path, "MOD", &fh );
	if ( fn )
	{
		do
		{
			char ext[ 10 ];
			Q_ExtractFileExtension( fn, ext, sizeof( ext ) );

			if ( !Q_stricmp( ext, "mp3" ) )
			{
				char temp[ 512 ];
				Q_snprintf( temp, sizeof( temp ), "ui/%s", fn );

				char *found = new char[ strlen( temp ) + 1 ];
				Q_strncpy( found, temp, strlen( temp ) + 1 );

				Q_FixSlashes( found );
				fileNames.AddToTail( found );
			}
	
			fn = g_pFullFileSystem->FindNext( fh );

		} while ( fn );

		g_pFullFileSystem->FindClose( fh );
	}

	// did we find any?
	if ( fileNames.Count() > 0 )
	{
#if defined( WIN32 ) && !defined( _X360 )
		SYSTEMTIME SystemTime;
		GetSystemTime( &SystemTime );
		int index = SystemTime.wMilliseconds % fileNames.Count();

		if ( fileNames.IsValidIndex( index ) && fileNames[index] )
		{
			char found[ 512 ];

			// escape chars "*#" make it stream, and be affected by snd_musicvolume
			Q_snprintf( found, sizeof( found ), "play *#%s", fileNames[index] );

			engine->ClientCmd_Unrestricted( found );
		}
#endif

		fileNames.PurgeAndDeleteElements();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called to setup the game UI
//-----------------------------------------------------------------------------
void CGameUI::Start()
{
	// determine Steam location for configuration
	if ( !FindPlatformDirectory( m_szPlatformDir, sizeof( m_szPlatformDir ) ) )
		return;

	if ( IsPC() )
	{
		// setup config file directory
		char szConfigDir[512];
		Q_strncpy( szConfigDir, m_szPlatformDir, sizeof( szConfigDir ) );
		Q_strncat( szConfigDir, "config", sizeof( szConfigDir ), COPY_ALL_CHARACTERS );

		DevMsg( "[GameUI] Steam config directory: %s\n", szConfigDir );

		g_pFullFileSystem->AddSearchPath(szConfigDir, "CONFIG");
		g_pFullFileSystem->CreateDirHierarchy("", "CONFIG");

		// user dialog configuration
		vgui::system()->SetUserConfigFile("InGameDialogConfig.vdf", "CONFIG");

		g_pFullFileSystem->AddSearchPath( "platform", "PLATFORM" );
	}

	// localization
	g_pVGuiLocalize->AddFile( "Resource/platform_%language%.txt");
	g_pVGuiLocalize->AddFile( "Resource/vgui_%language%.txt");
#ifdef _WIN32
	Sys_SetLastError( SYS_NO_ERROR );

	g_hMutex = Sys_CreateMutex( "ValvePlatformUIMutex" );
	g_hWaitMutex = Sys_CreateMutex( "ValvePlatformWaitMutex" );
	if ( g_hMutex == NULL || g_hWaitMutex == NULL || Sys_GetLastError() == SYS_ERROR_INVALID_HANDLE )
	{
		// error, can't get handle to mutex
		if (g_hMutex)
		{
			Sys_ReleaseMutex(g_hMutex);
		}
		if (g_hWaitMutex)
		{
			Sys_ReleaseMutex(g_hWaitMutex);
		}
		g_hMutex = NULL;
		g_hWaitMutex = NULL;
		Warning("Steam Error: Could not access Steam, bad mutex\n");
		return;
	}
	unsigned int waitResult = Sys_WaitForSingleObject(g_hMutex, 0);
	if (!(waitResult == SYS_WAIT_OBJECT_0 || waitResult == SYS_WAIT_ABANDONED))
	{
		// mutex locked, need to deactivate Steam (so we have the Friends/ServerBrowser data files)
		// get the wait mutex, so that Steam.exe knows that we're trying to acquire ValveTrackerMutex
		waitResult = Sys_WaitForSingleObject(g_hWaitMutex, 0);
		if (waitResult == SYS_WAIT_OBJECT_0 || waitResult == SYS_WAIT_ABANDONED)
		{
			Sys_EnumWindows(SendShutdownMsgFunc, 1);
		}
	}
#endif // _WIN32

	// Delay playing the startup music until two frames
	// this allows cbuf commands that occur on the first frame that may start a map
	m_iPlayGameStartupSound = 2;

	// now we are set up to check every frame to see if we can friends/server browser
	m_bTryingToLoadFriends = true;
	m_iFriendsLoadPauseFrames = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Validates the user has a cdkey in the registry
//-----------------------------------------------------------------------------
void CGameUI::ValidateCDKey() // Remove this function?
{
}

#if defined(POSIX)
// based off game/shared/of/util/os_utils.cpp (momentum mod)
bool linux_platformdir_helper( char *buf, int size)
{
	FILE *f = fopen("/proc/self/maps", "r");
	if (!f) return false;

	while (!feof(f))
	{
		if (!fgets(buf, size, f)) break;

		char *tmp = strrchr(buf, '\n');
		if (tmp) *tmp = '\0';

		char *mapname = strchr(buf, '/');
		if (!mapname) continue;

		if (strcmp(basename(mapname), "hl2_linux") == 0)
		{
			fclose(f);
			memmove(buf, mapname, strlen(mapname)+1);
			return true;
		}
	}

	fclose(f);
	buf[0] = '\0';
	return false;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Finds which directory the platform resides in
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CGameUI::FindPlatformDirectory(char *platformDir, int bufferSize)
{
	platformDir[0] = '\0';

	if ( platformDir[0] == '\0' )
	{
		// we're not under steam, so setup using path relative to game
		if ( IsPC() )
		{
#if defined(_WIN32)
			if (::GetModuleFileName((HINSTANCE)GetModuleHandle(NULL), platformDir, bufferSize))
			{
				char *lastslash = strrchr(platformDir, '\\'); // this should be just before the filename
#elif defined(POSIX)
			if ( linux_platformdir_helper(platformDir, bufferSize) )
			{
				char *lastslash = strrchr(platformDir, '/'); // this should be just before the filename
#else
#error "GameUI: Mac OSX Support is for people who look in os_utils.cpp for inspiration!"
#endif
				if ( lastslash )
				{
					*lastslash = 0;
					Q_strncat(platformDir, "/platform/", bufferSize, COPY_ALL_CHARACTERS );
					return true;
				}
			}
		}
		else
		{
			// xbox fetches the platform path from exisiting platform search path
			// path to executeable is not correct for xbox remote configuration
			if ( g_pFullFileSystem->GetSearchPath( "PLATFORM", false, platformDir, bufferSize ) )
			{
				char *pSeperator = strchr( platformDir, ';' );
				if ( pSeperator )
					*pSeperator = '\0';
				return true;
			}
		}

		Warning( "Unable to determine platform directory\n" );
		return false;
	}

	return (platformDir[0] != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Called to Shutdown the game UI system
//-----------------------------------------------------------------------------
void CGameUI::Shutdown()
{
	// notify all the modules of Shutdown
	g_VModuleLoader.ShutdownPlatformModules();

	// unload the modules them from memory
	g_VModuleLoader.UnloadPlatformModules();

	ModInfo().FreeModInfo();
	
#ifdef _WIN32
	// release platform mutex
	// close the mutex
	if (g_hMutex)
	{
		Sys_ReleaseMutex(g_hMutex);
	}
	if (g_hWaitMutex)
	{
		Sys_ReleaseMutex(g_hWaitMutex);
	}
#endif

	steamapicontext->Clear();
#ifndef _X360
	// SteamAPI_Shutdown(); << Steam shutdown is controlled by engine
#endif
	
	ConVar_Unregister();
	DisconnectTier3Libraries();
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to activate the gameUI
//-----------------------------------------------------------------------------
void CGameUI::ActivateGameUI()
{
	engine->ExecuteClientCmd("gameui_activate");
}

//-----------------------------------------------------------------------------
// Purpose: just wraps an engine call to hide the gameUI
//-----------------------------------------------------------------------------
void CGameUI::HideGameUI()
{
	engine->ExecuteClientCmd("gameui_hide");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::PreventEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_preventescape");
}

//-----------------------------------------------------------------------------
// Purpose: Toggle allowing the engine to hide the game UI with the escape key
//-----------------------------------------------------------------------------
void CGameUI::AllowEngineHideGameUI()
{
	engine->ExecuteClientCmd("gameui_allowescape");
}

//-----------------------------------------------------------------------------
// Purpose: Activate the game UI
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIActivated()
{
	//bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = true;

	// pause the server in case it is pausable
	if ( of_pausegame.GetBool() )
		engine->ClientCmd_Unrestricted( "setpause nomsg" );

	SetSavedThisMenuSession( false );

	IBasePanel* ui = GetBasePanel();
	if (ui)
	{
		bool bNeedActivation = true;
		if (ui->GetVguiPanel().IsVisible())
		{
			// Already visible, maybe don't need activation
			if (!IsInLevel() && IsInBackgroundLevel())
				bNeedActivation = false;
		}
		if (bNeedActivation)
		{
			ui->OnGameUIActivated();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hides the game ui, in whatever state it's in
//-----------------------------------------------------------------------------
void CGameUI::OnGameUIHidden()
{
	//bool bWasActive = m_bActivatedUI;
	m_bActivatedUI = false;

	// unpause the game when leaving the UI
	engine->ClientCmd_Unrestricted( "unpause nomsg" );

	IBasePanel* ui = GetBasePanel();
	if (ui)
	{
		ui->OnGameUIHidden();
	}
}

//-----------------------------------------------------------------------------
// Purpose: paints all the vgui elements
//-----------------------------------------------------------------------------
void CGameUI::RunFrame()
{
	if ( IsX360() && m_bOpenProgressOnStart )
	{
		StartProgressBar();
		m_bOpenProgressOnStart = false;
	}

	IBasePanel* ui = GetBasePanel();

	int wide, tall;
#if defined( TOOLFRAMEWORK_VGUI_REFACTOR )
	// resize the background panel to the screen size
	vgui::VPANEL clientDllPanel = enginevguifuncs->GetPanel( PANEL_ROOT );

	int x, y;
	vgui::ipanel()->GetPos( clientDllPanel, x, y );
	vgui::ipanel()->GetSize( clientDllPanel, wide, tall );
	staticPanel->SetBounds( x, y, wide,tall );
#else
	vgui::surface()->GetScreenSize(wide, tall);

	if (ui)
	{
		ui->GetVguiPanel().SetSize(wide, tall);
	}
#endif

	// Run frames
	g_VModuleLoader.RunFrame();

	if (ui)
	{
		ui->RunFrame();
	}

	// Play the start-up music the first time we run frame
	if ( IsPC() && m_iPlayGameStartupSound > 0 )
	{
		m_iPlayGameStartupSound--;
		if ( !m_iPlayGameStartupSound )
		{
			PlayGameStartupSound();
		}
	}

	// On POSIX and Mac OSX just load it without any care for race conditions
	// hackhack: posix steam gameui usage is rude and racey
	// -nopey

	if ( IsPC() && m_bTryingToLoadFriends && m_iFriendsLoadPauseFrames-- < 1
#ifdef _WIN32
		&& g_hMutex && g_hWaitMutex
#endif
	){
		// try and load Steam platform files
#ifdef _WIN32
		unsigned int waitResult = Sys_WaitForSingleObject(g_hMutex, 0);
		if (waitResult == SYS_WAIT_OBJECT_0 || waitResult == SYS_WAIT_ABANDONED)
#endif
		{
			// we got the mutex, so load Friends/Serverbrowser
			// clear the loading flag
			m_bTryingToLoadFriends = false;
			g_VModuleLoader.LoadPlatformModules(&m_GameFactory, 1, false);

#ifdef _WIN32
			// release the wait mutex
			Sys_ReleaseMutex(g_hWaitMutex);
#endif

			// notify the game of our game name
			const char *fullGamePath = engine->GetGameDirectory();
			const char *pathSep = strrchr( fullGamePath, '/' );
			if ( !pathSep )
			{
				pathSep = strrchr( fullGamePath, '\\' );
			}
			if ( pathSep )
			{
				KeyValues *pKV = new KeyValues("ActiveGameName" );
				pKV->SetString( "name", pathSep + 1 );
				pKV->SetInt( "appid", engine->GetAppID() );
				KeyValues *modinfo = new KeyValues("ModInfo");
				if ( modinfo->LoadFromFile( g_pFullFileSystem, "gameinfo.txt" ) )
				{
					pKV->SetString( "game", modinfo->GetString( "game", "" ) );
				}
				modinfo->deleteThis();
				
				g_VModuleLoader.PostMessageToAllModules( pKV );
			}

			// notify the ui of a game connect if we're already in a game
			if (m_iGameIP)
			{
				SendConnectedToGameMessage();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OLD_OnConnectToServer(const char *game, int IP, int port)
{
	// Nobody should use this anymore because the query port and the connection port can be different.
	// Use OnConnectToServer2 instead.
	Assert( false );
	OnConnectToServer2( game, IP, port, port );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game connects to a server
//-----------------------------------------------------------------------------
void CGameUI::OnConnectToServer2(const char *game, int IP, int connectionPort, int queryPort)
{
	m_iGameIP = IP;
	m_iGameConnectionPort = connectionPort;
	m_iGameQueryPort = queryPort;

	SendConnectedToGameMessage();
}


void CGameUI::SendConnectedToGameMessage()
{
	MEM_ALLOC_CREDIT();
	KeyValues *kv = new KeyValues( "ConnectedToGame" );
	kv->SetInt( "ip", m_iGameIP );
	kv->SetInt( "connectionport", m_iGameConnectionPort );
	kv->SetInt( "queryport", m_iGameQueryPort );

	g_VModuleLoader.PostMessageToAllModules( kv );
}



//-----------------------------------------------------------------------------
// Purpose: Called when the game disconnects from a server
//-----------------------------------------------------------------------------
void CGameUI::OnDisconnectFromServer( uint8 eSteamLoginFailure )
{
	m_iGameIP = 0;
	m_iGameConnectionPort = 0;
	m_iGameQueryPort = 0;

	if ( g_hLoadingBackgroundDialog )
	{
		vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, new KeyValues("DisconnectedFromGame"), NULL );
	}

	g_VModuleLoader.PostMessageToAllModules(new KeyValues("DisconnectedFromGame"));

#if 0
	if ( eSteamLoginFailure == STEAMLOGINFAILURE_NOSTEAMLOGIN )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayNoSteamConnectionError();
		}
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_VACBANNED )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayVACBannedError();
		}
	}
	else if ( eSteamLoginFailure == STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE )
	{
		if ( g_hLoadingDialog )
		{
			g_hLoadingDialog->DisplayLoggedInElsewhereError();
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: activates the loading dialog on level load start
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingStarted( bool bShowProgressDialog )
{
	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "LoadingStarted" ) );


	IBasePanel* ui = GetBasePanel();
	if (ui)
	{
		ui->OnLevelLoadingStarted(NULL, bShowProgressDialog);
	}

	ShowLoadingBackgroundDialog();

	if ( bShowProgressDialog )
	{
		StartProgressBar();
	}

	// Don't play the start game sound if this happens before we get to the first frame
	m_iPlayGameStartupSound = 0;
}

//-----------------------------------------------------------------------------
// Purpose: closes any level load dialog
//-----------------------------------------------------------------------------
void CGameUI::OnLevelLoadingFinished(bool bError, const char *failureReason, const char *extendedReason)
{
	StopProgressBar( bError, failureReason, extendedReason );

	// notify all the modules
	g_VModuleLoader.PostMessageToAllModules( new KeyValues( "LoadingFinished" ) );

	// Need to call this function in the Base mod Panel to let it know that we've finished loading the level
	// This should fix the loading screen not disappearing.

	IBasePanel* ui = GetBasePanel();
	if (ui)
	{
		ui->OnLevelLoadingFinished(new KeyValues("LoadingFinished"));
	}

	HideLoadingBackgroundDialog();


}

//-----------------------------------------------------------------------------
// Purpose: Updates progress bar
// Output : Returns true if screen should be redrawn
//-----------------------------------------------------------------------------
bool CGameUI::UpdateProgressBar(float progress, const char *statusText)
{
	IBasePanel * ui = GetBasePanel();
	if (ui)
	{
		return ui->UpdateProgressBar(progress, statusText);
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetProgressLevelName( const char *levelName )
{
	MEM_ALLOC_CREDIT();
	if ( g_hLoadingBackgroundDialog )
	{
		KeyValues *pKV = new KeyValues( "ProgressLevelName" );
		pKV->SetString( "levelName", levelName );
		vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, pKV, NULL );
	}

	if ( g_hLoadingDialog.Get() )
	{
		// TODO: g_hLoadingDialog->SetLevelName( levelName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::StartProgressBar()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the screen should be updated
//-----------------------------------------------------------------------------
bool CGameUI::ContinueProgressBar( float progressFraction )
{
	if (!g_hLoadingDialog.Get())
		return false;

#if 0
	g_hLoadingDialog->Activate();
	return g_hLoadingDialog->SetProgressPoint(progressFraction);
#endif
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: stops progress bar, displays error if necessary
//-----------------------------------------------------------------------------
void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
	if ( bError )
	{
		// kill the FUCKING modules so they stop eating precious input
		GameConsole().Hide();
		g_VModuleLoader.DeactivateModule("Servers");

		bool bDatatable = false;
		if ( failureReason )
		{
			if ( Q_strstr( failureReason, "class tables" ) )
			{
				bDatatable = true;
			}
		}

#if 0
		GenericConfirmation* confirmation = 
			static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, NULL, true ) );

		if ( !confirmation )
			return;

		GenericConfirmation::Data_t data;

		data.pWindowTitle = "#GameUI_Disconnected";
		if ( bDatatable )
			data.pMessageText = "#GameUI_ErrorOutdatedBinaries";
		else
			data.pMessageText = failureReason;

		data.bOkButtonEnabled = true;

		confirmation->SetUsageData(data);
#endif

		// none of this shit works to bring the dialog in focus immediately without the user clicking on it
		/*
		confirmation->SetZPos(999);
		confirmation->OnMousePressed( MOUSE_LEFT );
		confirmation->RequestFocus();
		confirmation->MoveToFront();
		vgui::ipanel()->MoveToFront(confirmation->GetVPanel());
		vgui::input()->SetMouseCapture(confirmation->GetVPanel());
		*/
	}

	// obsolete code

	/*
	if (!g_hLoadingDialog.Get())
		return;

	if ( !IsX360() && bError )
	{
		// turn the dialog to error display mode
		g_hLoadingDialog->DisplayGenericError(failureReason, extendedReason);
	}
	else
	{
		// close loading dialog
		g_hLoadingDialog->Close();
		g_hLoadingDialog = NULL;
	}
	*/
	// should update the background to be in a transition here
}

//-----------------------------------------------------------------------------
// Purpose: sets loading info text
//-----------------------------------------------------------------------------
bool CGameUI::SetProgressBarStatusText(const char *statusText)
{
	if (!g_hLoadingDialog.Get())
		return false;

	if (!statusText)
		return false;

	if (!stricmp(statusText, m_szPreviousStatusText))
		return false;

#if 0
	g_hLoadingDialog->SetStatusText(statusText);
#endif
	Q_strncpy(m_szPreviousStatusText, statusText, sizeof(m_szPreviousStatusText));
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBar(float progress /* range [0..1] */)
{
	if (!g_hLoadingDialog.Get())
		return;

#if 0
	g_hLoadingDialog->SetSecondaryProgress(progress);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUI::SetSecondaryProgressBarText(const char *statusText)
{
	if (!g_hLoadingDialog.Get())
		return;

#if 0
	g_hLoadingDialog->SetSecondaryProgressText(statusText);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns prev settings
//-----------------------------------------------------------------------------
bool CGameUI::SetShowProgressText( bool show )
{
	if (!g_hLoadingDialog.Get())
		return false;

#if 0
	return g_hLoadingDialog->SetShowProgressText( show );
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if we're currently playing the game
//-----------------------------------------------------------------------------
bool CGameUI::IsInLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && !engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're at the main menu and a background level is loaded
//-----------------------------------------------------------------------------
bool CGameUI::IsInBackgroundLevel()
{
	const char *levelName = engine->GetLevelName();
	if (levelName && levelName[0] && engine->IsLevelMainMenuBackground())
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're in a multiplayer game
//-----------------------------------------------------------------------------
bool CGameUI::IsInMultiplayer()
{
	return (IsInLevel() && engine->GetMaxClients() > 1);
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we're console ui
//-----------------------------------------------------------------------------
bool CGameUI::IsConsoleUI()
{
	return m_bIsConsoleUI;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if we've saved without closing the menu
//-----------------------------------------------------------------------------
bool CGameUI::HasSavedThisMenuSession()
{
	return m_bHasSavedThisMenuSession;
}

void CGameUI::SetSavedThisMenuSession( bool bState )
{
	m_bHasSavedThisMenuSession = bState;
}

//-----------------------------------------------------------------------------
// Purpose: Makes the loading background dialog visible, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::ShowLoadingBackgroundDialog()
{
	if ( g_hLoadingBackgroundDialog )
	{
		IBasePanel* ui = GetBasePanel();
		if (ui)
		{
			vgui::VPANEL panel = ui->GetVguiPanel().GetVPanel();

			vgui::ipanel()->SetParent(g_hLoadingBackgroundDialog, panel);
			vgui::ipanel()->MoveToFront(g_hLoadingBackgroundDialog);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Hides the loading background dialog, if one has been set
//-----------------------------------------------------------------------------
void CGameUI::HideLoadingBackgroundDialog()
{
	if ( g_hLoadingBackgroundDialog )
	{
		if ( engine->IsInGame() )
		{
			vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, new KeyValues( "LoadedIntoGame" ), NULL );
		}
		else
		{
			vgui::ipanel()->SetVisible( g_hLoadingBackgroundDialog, false );
			vgui::ipanel()->MoveToBack( g_hLoadingBackgroundDialog );
		}

		vgui::ivgui()->PostMessage( g_hLoadingBackgroundDialog, new KeyValues("HideAsLoadingPanel"), NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether a loading background dialog has been set
//-----------------------------------------------------------------------------
bool CGameUI::HasLoadingBackgroundDialog()
{
	return ( NULL != g_hLoadingBackgroundDialog );
}

//-----------------------------------------------------------------------------

void CGameUI::NeedConnectionProblemWaitScreen()
{
#if 0
	BaseModUI::CUIGameData::Get()->NeedConnectionProblemWaitScreen();
#endif
}

void CGameUI::ShowPasswordUI( char const *pchCurrentPW )
{
#if 0
	BaseModUI::CUIGameData::Get()->ShowPasswordUI( pchCurrentPW );
#endif
}

//-----------------------------------------------------------------------------
void CGameUI::SetProgressOnStart()
{
	m_bOpenProgressOnStart = true;
}

#if defined( _X360 ) && defined( _DEMO )
void CGameUI::OnDemoTimeout()
{
	GetBasePanel().OnDemoTimeout();
}
#endif

#ifndef GAMEUI_EMBEDDED
//-----------------------------------------------------------------------------
// Purpose: Performs a var args printf into a static return buffer
// Input  : *format - 
//			... - 
// Output : char
//-----------------------------------------------------------------------------
char *VarArgs(const char *format, ...)
{
	va_list		argptr;
	static char		string[1024];

	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	return string;
}

void GetHudSize(int& w, int &h)
{
	vgui::surface()->GetScreenSize(w, h);
}

//-----------------------------------------------------------------------------
// Purpose: ScreenHeight returns the height of the screen, in pixels
// Output : int
//-----------------------------------------------------------------------------
int ScreenHeight(void)
{
	int w, h;
	GetHudSize(w, h);
	return h;
}

//-----------------------------------------------------------------------------
// Purpose: ScreenWidth returns the width of the screen, in pixels
// Output : int
//-----------------------------------------------------------------------------
int ScreenWidth(void)
{
	int w, h;
	GetHudSize(w, h);
	return w;
}

void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	Q_strncpy( tempString, pString, sizeof(tempString) );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

void UTIL_StringToColor32( color32 *color, const char *pString )
{
	int tmp[4];
	UTIL_StringToIntArray( tmp, 4, pString );
	color->r = tmp[0];
	color->g = tmp[1];
	color->b = tmp[2];
	color->a = tmp[3];
}
#endif

#define STUB_GAMEUI_FUNC(name, ret, val, ...) \
	ret CGameUI::name(__VA_ARGS__) \
	{ \
		DebuggerBreak(); \
		return val; \
	}

STUB_GAMEUI_FUNC(ShowNewGameDialog, void, , int chapter);
STUB_GAMEUI_FUNC(SessionNotification, void, , const int notification, const int param);
STUB_GAMEUI_FUNC(SystemNotification, void, , const int notification);
STUB_GAMEUI_FUNC(ShowMessageDialog, void, , const uint nType, vgui::Panel *pOwner);
STUB_GAMEUI_FUNC(UpdatePlayerInfo, void, , uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost);
STUB_GAMEUI_FUNC(SessionSearchResult, void, , int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping);
STUB_GAMEUI_FUNC(OnCreditsFinished, void, , );
STUB_GAMEUI_FUNC(BonusMapUnlock, void, , const char *pchFileName, const char *pchMapName);
STUB_GAMEUI_FUNC(BonusMapComplete, void, , const char *pchFileName, const char *pchMapName);
STUB_GAMEUI_FUNC(BonusMapChallengeUpdate, void, , const char *pchFileName, const char *pchMapName, const char *pchChallengeName, int iBest);
STUB_GAMEUI_FUNC(BonusMapChallengeNames, void, , char *pchFileName, char *pchMapName, char *pchChallengeName);
STUB_GAMEUI_FUNC(BonusMapChallengeObjectives, void, , int &iBronze, int &iSilver, int &iGold);
STUB_GAMEUI_FUNC(BonusMapDatabaseSave, void, , );
STUB_GAMEUI_FUNC(BonusMapNumAdvancedCompleted, int, 0, );
STUB_GAMEUI_FUNC(BonusMapNumMedals, void, , int piNumMedals[3]);
STUB_GAMEUI_FUNC(ValidateStorageDevice, bool, false, int *pStorageDeviceValidated);
void CGameUI::OnConfirmQuit()
{
#if 0
	if( !engine->IsInGame() )
	{
		MainMenu *pMainMenu = static_cast< MainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_MAINMENU ) );
		if ( pMainMenu )
		{
			pMainMenu->OnCommand( "QuitGame" );
		}
	}
	else
	{
		InGameMainMenu *pInGameMainMenu = static_cast< InGameMainMenu* >( CBaseModPanel::GetSingleton().GetWindow( WT_INGAMEMAINMENU ) );
		if ( pInGameMainMenu )
		{
			pInGameMainMenu->OnCommand( "QuitGame" );
		}
		
	}
#endif
}
STUB_GAMEUI_FUNC(IsMainMenuVisible, bool, false, );
STUB_GAMEUI_FUNC(SetMainMenuOverride, void, , vgui::VPANEL panel);

STUB_GAMEUI_FUNC(SendMainMenuCommand, void, , const char *pszCommand);

void CGameUI::SetBasePanel(IBasePanel* basePanel)
{
	gBasePanel = basePanel;

	// setup base panel
	// vgui::Panel& factoryBasePanel = basePanel->GetVguiPanel(); // ConstructGetBasePanel().GetVguiPanel(); // explicit singleton instantiation

	// factoryBasePanel.SetBounds(0, 0, 640, 480);
	// factoryBasePanel.SetPaintBorderEnabled(false);
	// factoryBasePanel.SetPaintBackgroundEnabled(true);
	// factoryBasePanel.SetPaintEnabled(true);
	// factoryBasePanel.SetVisible(true);
	// 
	// factoryBasePanel.SetMouseInputEnabled(IsPC());
	// // factoryBasePanel.SetKeyBoardInputEnabled( IsPC() );
	// factoryBasePanel.SetKeyBoardInputEnabled(true);

	// vgui::VPANEL rootpanel = enginevguifuncs->GetPanel(PANEL_GAMEUIDLL);
	// factoryBasePanel.SetParent(rootpanel);
}
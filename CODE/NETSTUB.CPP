/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
**	NETSTUB.CPP -- PORT-CREATED. Inert stand-ins for the multiplayer layer.
**
**	The single-player port does not build the network files (IPX, null modem,
**	TCP/IP, WChat DDE, statistics -- see the DROP list in port/worklist.py), but
**	the single-player code still refers to them: the main menu offers network
**	games, the game loop polls the connection managers, INIT.CPP reads spawn
**	options. Everything here reports "no network, nothing connected, nothing
**	received", so those paths fall through to single-player behaviour.
**
**	The original files are untouched. When multiplayer is ported, delete this
**	file and put those files back in the build; every definition here has its
**	real counterpart named beside it.
*/

#include	"function.h"
#include	"tcpip.h"			// TcpipManagerClass, Server
#include	"dde.h"				// Instance_Class
#include	"ipx95.h"			// IPX_*95 function pointers
#include	"wincomm.h"			// WinModemClass, SerialPort (WIN32LIB/INCLUDE)

/*
**	Globals (STATS.CPP, INTERNET.CPP, TCPIP.CPP, NULLDLG.CPP, WINCOMM.CPP).
*/
bool					ConnectionLost = false;
bool					GameStatisticsPacketSent = false;
void *				PacketLater = NULL;
char					PlanetWestwoodIPAddress[IP_ADDRESS_MAX] = "";
bool					PlanetWestwoodIsHost = false;
bool					Server = false;
bool					SpawnedFromWChat = false;
WinModemClass *	SerialPort = NULL;
TimerClass			GameTimer;
// STATS.CPP defines this as `bool`, but its one linked reader, CCDDE.CPP, declares
// it `extern BOOL` and reads four bytes. Defined at the reader's width so it can
// never read past the object.
BOOL					GameTimerInUse = FALSE;

/*
**	Globals the dropped files defined for everyone: the window's show command
**	(INTERNET.CPP), and whether this is Windows NT (IPX95.CPP; STARTUP.CPP sets
**	it from GetVersion, which reports Windows 95).
*/
int					ShowCommand = 0;
bool					WindowsNT = false;

/*
**	Free functions.
*/
void Check_From_WChat(char *)								{}			// INTERNET.CPP
bool Spawn_WChat(bool)										{return false;}
int Read_Game_Options(char *)								{return 0;}
void Register_Game_Start_Time(void)						{}
void Register_Game_End_Time(void)						{}
void Send_Statistics_Packet(void)						{}
bool Remote_Connect(void)									{return false;}	// TCPIP.CPP
bool Server_Remote_Connect(void)							{return false;}
bool Client_Remote_Connect(void)							{return false;}
bool Init_Network(void)										{return false;}	// NETDLG.CPP
void Shutdown_Network(void)								{}
void Unload_IPX_Dll(void)									{}				// IPX95.CPP
bool Process_Global_Packet(GlobalPacketType *, IPXAddressClass *)	{return false;}
void Destroy_Connection(int, int)						{}
void Net_Reconnect_Dialog(int, int, int, unsigned long)	{}
int Com_Scenario_Dialog(bool)								{return 0;}			// NULLDLG.CPP
int Com_Show_Scenario_Dialog(void)						{return 0;}
GameType Select_Serial_Dialog(void)						{return GAME_NORMAL;}
void Destroy_Null_Connection(int, int)					{}
int Reconnect_Modem(void)									{return 0;}
void Modem_Signoff(void)									{}
void Shutdown_Modem(void)									{}

/*
**	Kept with its real body (NETDLG.CPP): a plain CRC of a player name, which
**	does not depend on any network being present.
*/
unsigned long Compute_Name_CRC(char *name)
{
	char buf[80];
	unsigned long crc = 0L;
	int i;

	strcpy (buf, name);
	strupr (buf);

	for (i = 0; i < strlen(buf); i++) {
		Add_CRC (&crc, (unsigned long)buf[i]);
	}

	return (crc);
}

/*
**	IPX (IPX.CPP / IPX95.CPP). Any non-zero result is failure; deliberately not
**	IPXERR_SOCKET_ERROR, which makes IPXCONN.CPP close and retry.
*/
int IPX_Open_Socket(unsigned short)						{return -1;}
int IPX_Close_Socket(unsigned short)					{return 0;}
int IPX_Get_Local_Target(unsigned char *, unsigned char *, unsigned short, unsigned char *)	{return -1;}

static void __stdcall	wwport_ipx_shut_down(void)																{}
static int  __stdcall	wwport_ipx_send(unsigned char *, unsigned char *, int, unsigned char *, unsigned char *)	{return 0;}
static int  __stdcall	wwport_ipx_broadcast(unsigned char *, int)												{return 0;}
static BOOL __stdcall	wwport_ipx_listen(void)																	{return FALSE;}

IPXShutDown95Type			IPX_Shut_Down95			= wwport_ipx_shut_down;
IPXSendPacket95Type		IPX_Send_Packet95			= wwport_ipx_send;
IPXBroadcastPacket95Type	IPX_Broadcast_Packet95	= wwport_ipx_broadcast;
IPXStartListening95Type	IPX_Start_Listening95	= wwport_ipx_listen;

/*
**	IPXManagerClass (IPXMGR.CPP). A manager with IPX absent and no connections.
**	Every virtual is defined here because the vtable is emitted with them.
*/
IPXManagerClass::IPXManagerClass(int glb_maxlen, int pvt_maxlen, int glb_num_packets,
	int pvt_num_packets, unsigned short socket, unsigned short product_id)
{
	IPXStatus = 0;
	Listening = 0;
	RealMemAllocd = 0;
	Glb_MaxPacketLen = glb_maxlen;
	Glb_NumPackets = glb_num_packets;
	Pvt_MaxPacketLen = pvt_maxlen;
	Pvt_NumPackets = pvt_num_packets;
	ProductID = product_id;
	Socket = socket;
	ConnectionNum = 0;
	for (int i = 0; i < CONNECT_MAX; i++) Connection[i] = NULL;
	NumConnections = 0;
	GlobalChannel = NULL;
	CurConnection = 0;
	RetryDelta = MaxRetries = Timeout = 0;
	SendOverflows = ReceiveOverflows = BadConnection = 0;
}
IPXManagerClass::~IPXManagerClass()											{}
int IPXManagerClass::Is_IPX(void)												{return 0;}
void IPXManagerClass::Set_Socket(unsigned short socket)					{Socket = socket;}
void IPXManagerClass::Set_Timing(unsigned long, unsigned long, unsigned long)	{}
int IPXManagerClass::Create_Connection(int, char *, IPXAddressClass *)	{return 0;}
int IPXManagerClass::Delete_Connection(int)									{return 0;}
int IPXManagerClass::Num_Connections(void)									{return 0;}
int IPXManagerClass::Connection_ID(int)										{return CONNECTION_NONE;}
int IPXManagerClass::Connection_Index(int)									{return CONNECTION_NONE;}
char * IPXManagerClass::Connection_Name(int)									{return NULL;}
IPXAddressClass * IPXManagerClass::Connection_Address(int)				{return NULL;}
int IPXManagerClass::Send_Global_Message(void *, int, int, IPXAddressClass *)	{return 0;}
int IPXManagerClass::Get_Global_Message(void *, int *, IPXAddressClass *, unsigned short *)	{return 0;}
int IPXManagerClass::Send_Private_Message(void *, int, int, int)		{return 0;}
int IPXManagerClass::Get_Private_Message(void *, int *, int *)			{return 0;}
int IPXManagerClass::Service(void)												{return 0;}
int IPXManagerClass::Global_Num_Send(void)									{return 0;}
int IPXManagerClass::Global_Num_Receive(void)								{return 0;}
int IPXManagerClass::Private_Num_Send(int)									{return 0;}
int IPXManagerClass::Private_Num_Receive(int)								{return 0;}
unsigned long IPXManagerClass::Response_Time(void)							{return 0;}
void IPXManagerClass::Reset_Response_Time(void)								{}
void IPXManagerClass::Configure_Debug(int, int, int, char **, int, int)	{}
void IPXManagerClass::Mono_Debug_Print(int, int)							{}

/*
**	NullModemClass (NULLMGR.CPP). A serial manager with no port open.
*/
NullModemClass::NullModemClass(int numsend, int numreceive, int maxlen, unsigned short magicnum)
{
	BuildBuf = NULL;
	MaxLen = maxlen;
	EchoBuf = NULL;
	EchoSize = EchoCount = 0;
	Connection = NULL;
	NumConnections = 0;
	PortHandle = NULL;
	NumSend = numsend;
	NumReceive = numreceive;
	MagicNum = magicnum;
	RXBuf = NULL;
	RXSize = RXCount = 0;
	RetryDelta = MaxRetries = Timeout = 0;
	SendOverflows = ReceiveOverflows = CRCErrors = 0;
}
NullModemClass::~NullModemClass()												{}
int NullModemClass::Num_Connections(void)										{return 0;}
int NullModemClass::Init_Send_Queue(void)										{return 0;}
void NullModemClass::Set_Timing(unsigned long, unsigned long, unsigned long)	{}
int NullModemClass::Send_Message(void *, int, int)							{return 0;}
int NullModemClass::Get_Message(void *, int *)								{return 0;}
int NullModemClass::Service(void)												{return 0;}
int NullModemClass::Num_Send(void)												{return 0;}
int NullModemClass::Num_Receive(void)											{return 0;}
unsigned long NullModemClass::Response_Time(void)							{return 0;}
void NullModemClass::Reset_Response_Time(void)								{}
void NullModemClass::Configure_Debug(int, int, int, char **, int, int)	{}
#ifdef CHEAT_KEYS
void NullModemClass::Mono_Debug_Print(int, int)								{}
#endif

/*
**	WinModemClass (WIN32LIB/WINCOMM, dropped from the library build).
*/
void WinModemClass::Write_To_Serial_Port(unsigned char *, int)		{}

/*
**	TcpipManagerClass (TCPIP.CPP). Never initialised, never connected.
*/
TcpipManagerClass Winsock;

TcpipManagerClass::TcpipManagerClass(void)
{
	WinsockInitialised = FALSE;
	IsServer = FALSE;
	Connected = FALSE;
	ConnectStatus = NOT_CONNECTING;
	UseUDP = FALSE;
	HostAddress[0] = 0;
	TXBufferHead = TXBufferTail = RXBufferHead = RXBufferTail = 0;
}
TcpipManagerClass::~TcpipManagerClass(void)									{}
BOOL TcpipManagerClass::Init(void)												{return FALSE;}
void TcpipManagerClass::Start_Server(void)									{}
void TcpipManagerClass::Start_Client(void)									{}
int TcpipManagerClass::Read(void *, int)										{return 0;}
void TcpipManagerClass::Write(void *, int)									{}
void TcpipManagerClass::Close(void)												{}
void TcpipManagerClass::Set_Host_Address(char *)							{}
void TcpipManagerClass::Message_Handler(HWND, UINT, UINT, LONG)			{}

/*
**	Instance_Class (DDE.CPP): the DDE link to Westwood Chat. CCDDE.CPP builds one
**	at startup and calls Test_Server_Running() as the "is Red Alert already
**	running?" check -- it must answer FALSE or the game believes it is a second
**	copy.
*/
Instance_Class::Instance_Class(LPSTR, LPSTR)									{}
Instance_Class::~Instance_Class(void)											{}
BOOL Instance_Class::Poke_Server(LPBYTE, DWORD)								{return FALSE;}
BOOL Instance_Class::Register_Server(BOOL CALLBACK (*)(LPBYTE, long))	{return FALSE;}
BOOL Instance_Class::Test_Server_Running(HSZ)								{return FALSE;}
BOOL Instance_Class::Enable_Callback(BOOL)									{return FALSE;}
BOOL Instance_Class::Open_Poke_Connection(HSZ)								{return FALSE;}
BOOL Instance_Class::Close_Poke_Connection(void)							{return FALSE;}

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
**	Globals (STATS.CPP, INTERNET.CPP, TCPIP.CPP, NULLMGR.CPP, WINCOMM.CPP).
*/
bool					ConnectionLost = false;
bool					GameStatisticsPacketSent = false;
void *				PacketLater = NULL;
char					PlanetWestwoodIPAddress[IP_ADDRESS_MAX] = "";
bool					PlanetWestwoodIsHost = false;
bool					Server = false;
bool					SpawnedFromWChat = false;
WinModemClass *	SerialPort = NULL;
char					ModemRXString[80];		// last modem response; the skirmish dialog shows it if set
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

/*
**	Kept with their real contents, because the skirmish dialog (NULLDLG.CPP)
**	uses them and neither needs a network.
**
**	EngMisStr (NETDLG.CPP): the English descriptions of the expansion
**	multiplayer maps. Com_Scenario_Dialog lists a map named here only if its
**	expansion is installed. Copied as it stands in NETDLG.CPP; the German and
**	French tables come back with that file.
*/
#if !defined(ENGLISH)
#error "EngMisStr: only the ENGLISH table is copied here; take this build's table from NETDLG.CPP"
#endif
#ifdef ENGLISH
char const *EngMisStr[] = {
	"Coastal Influence (Med)",
	"Middle Mayhem (Sm)",
	"Equal Opportunity (Sm)",
	"Marooned II (Med)",
	"Keep off the Grass (Sm)",
	"Isle of Fury (Lg)",
	"Ivory Wastelands (Sm)",
	"Shallow Grave (Med)",
	"North By Northwest (Lg)",
	"First Come, First Serve (Sm)",
	"Island Hoppers (Sm)",
	"Raraku (Lg)",
	"Central Conflict (Lg)",
	"Combat Alley (Med)",
	"Island Wars (Lg)",
	"Desolation (Lg)",
	"No Escape (Med)",
	"No Man's Land (Med)",
	"Normandy (Med)",
	"Pond Skirmish (Med)",
	"Ridge War (Med)",
	"A Path Beyond (Lg)",
	"Dugout Isle (Med)",
	"Treasure Isle (Med)",

	"Africa (Lg)",
	"Alaska Anarchy (Lg)",
	"All that Glitters... (Lg)",
	"Apre's Peace (Lg)",
	"Antartica (Lg)",
	"Armourgarden (Lg)",
	"Austraila (Med)",
	"Barrier to Entry (Lg)",
	"Bavarian Blast (Med)",
	"Be Shore (Med)",
	"Bearing Straits (Med)",
	"Blow Holes (Lg)",
	"Bonsai (Sm)",
	"Brother Stalin (Lg)",
	"Bullseye (Lg)",
	"C&C (Med)",
	"Camos Canyon (Med)",
	"Camos Coves (Lg)",
	"Camos Cross (Lg)",
	"Camos Crossing (Sm)",
	"Central Arena (Lg)",
	"Canyon River (Med)",
	"Crossroads (Sm)",
	"Czech Mate (Lg)",
	"Dday (Med)",
	"Disaster Central (Lg)",
	"Docklands (Med)",
	"East Coast (Med)",
	"Eastern Seaboard (Lg)",
	"Finger Lake (Lg)",
	"Fjords (Med)",
	"Floodlands (Lg)",
	"Forest under fire (Lg)",
	"Four Corners (Lg)",
	"Frostbit Fjords (Lg)",
	"Glenboig (Sm)",
	"Hell Frozen Over (Lg)",
	"India (Lg)",
	"Indirect Fire (Lg)",
	"Island Wars II (Lg)",
	"Italy (Lg)",
	"Kabalo (Lg)",
	"King of the Hills (Lg)",
	"Lake Divide (Med)",
	"Lakelands (Lg)",
	"Land Ladder (Lg)",
	"Lotsa Lakes (Lg)",
	"Lunar Battlefield (Lg Special)",
	"Malibu Fields (Med)",
	"Marshland (Med)",
	"MyLai Delta (Med)",
	"Natural Harbor (Med)",
	"No Way Out (Lg)",
	"Normandy Landing (Lg)",
	"Ore Wars (Med)",
	"Oz (Lg)",
	"Pilgrim Fathers II (Lg)",
	"Pip's Ice Tea (Med)",
	"Polar Panic (Lg)",
	"Ponds (Med)",
	"Putney (Lg)",
	"Return to Zion (Lg)",
	"Ring of Land (Lg)",
	"River Basin (Lg)",
	"River Delta (Med)",
	"River Islands (Med)",
	"River Maze (Sm)",
	"Rivers (Sm)",
	"Run the Gauntlet (Med)",
	"Scappa Flow (Lg)",
	"Siberian Slaughter (Lg)",
	"Sleepy Valley (Sm)",
	"Snake River (Lg)",
	"Snow Wars (Lg)",
	"Snowball fight (Lg)",
	"Snowy Island (Lg)",
	"So Near So Far (Sm)",
	"South America (Lg)",
	"Spring Line (Lg)",
	"Star (Lg)",
	"Straighter & Narrower (Sm)",
	"TerrainSpotting (Sm)",
	"The Bay (Lg)",
	"The Garden (Lg)",
	"The Great Lakes (Med)",
	"The Ice Arena (Lg)",
	"The Lake District (Lg)",
	"The Linked lands (Lg)",
	"The Mississippi (Med)",
	"The Sticky Bit (Lg)",
	"The Valley (Med)",
	"The Woods Today (Lg)",
	"Things to Come (Lg)",
	"Tiger Core (Sm)",
	"To the Core (Sm)",
	"Tournament Hills (Lg)",
	"Tropical Storm (Med)",
	"Tundra Trouble (Lg)",
	"Uk (Med)",
	"Undiscovered Country (Sm)",
	"United States (Med)",
	"Volcano (Sm)",
	"Wastelands (Lg)",
	"Water Works (Sm)",
	"World Map (Med)",
	"Zambezi (Lg)",

	"A Pattern of Islands (Lg 8 players)",
	"Arena Valley Extreme (Mega 8 players)",
	"Around the Rim (Sm 4 players)",
	"Ashes to Ashes (Lg 6 players)",
	"Artic Wasteland (Mega 8 players)",
	"Badajoz (Med 4 players)",
	"Baptism of Fire (Lg 6 players)",
	"Big Fish, Small Pond (Lg 6 players)",
	"Blue Lakes (Lg 8 players)",
	"Booby Traps (Mega 8 players)",
	"Bridgehead (Lg 6 players)",
	"Butterfly Bay (Lg 6 players)",
	"Central Conflict Extreme (Mega 8 players)",
	"Circles of Death (Mega 8 players)",
	"Cold Front (Med 6 players)",
	"Cold Pass (Med 4 players)",
	"Combat Zones (Mega 8 players)",
	"Conflict Cove (Sm 4 players)",
	"Culloden Moor (Med 8 players)",
	"Damnation Alley (Mega 8 players)",
	"Death Valley (Mega 8 players)",
	"Deep Six (Mega 8 players)",
	"Destruction Derby (Mega 8 players)",
	"Diamonds Aren't Forever (Mega 8 players)",
	"Elysium (Sm 4 players)",
	"Equal Shares (Lg 4 players)",
	"Frost Bitten (Mega 8 players)",
	"Frozen Valley (Med 6 players)",
	"Gettysburg (Sm 4 players)",
	"Glacial Valley (Sm 4 players)",
	"Gold Coast (Med 6 players)",
	"Gold Rush (Lg 4 players)",
	"Habitat (Lg 4 players)",
	"Hades Frozen Over (Sm 4 players)",
	"Hamburger Hill (Mega 8 players)",
	"Hastings (Sm 4 players)",
	"Hell's Pass (Med 6 players)",
	"Holy Grounds (Mega 8 players)",
	"Ice Bergs (Med 6 players)",
	"Ice Station (Lg 6 players)",
	"Ice Queen (Lg 4 players)",
	"In the Sun (Med 6 players)",
	"Innocents? (Mega 8 players)",
	"Islands (Med 8 players)",
	"Island Plateau (Lg 4 players)",
	"Island Wars Extreme (Mega 8 players)",
	"Kananga (Med 6 players)",
	"King of the Hills Extreme (Mega 8 players)",
	"Lake Land (Lg 8 players)",
	"Land Locked (Lg 8 players)",
	"Lanes (Med 8 players)",
	"Leipzip (Sm 4 players)",
	"Meander (Lg 8 players)",
	"Mekong (Med 8 players)",
	"Middle Ground (Med 8 players)",
	"Naval Conquests (Mega 8 players)",
	"On your Marks (Med 4 players)",
	"Open Warfare (Mega 8 players)",
	"Ore Gardens (Lg 8 players)",
	"Potholes (Mega 8 players)",
	"Puddles (Med 4 players)",
	"Random Violence (Mega 8 players)",
	"Revenge (Med 8 players)",
	"Rias (Med 8 players)",
	"River Crossing (Sm 4 players)",
	"River Rampage (Mega 8 players)",
	"River Rapids (Lg 6 players)",
	"Rivers Wild (Mega 8 players)",
	"Rorkes Drift (Lg 4 players)",
	"Seaside (Med 4 players)",
	"Shades (Med 8 players)",
	"Smuggler's Cove (Lg 6 players)",
	"Snow Garden (Sm 2 players)",
	"Stalingrad (Sm 4 players)",
	"Sticks & Stones (Med 4 players)",
	"Strathearn Valley (Lg 6 players)",
	"Super Bridgehead (Mega 8 players)",
	"Super Mekong (Mega 8 players)",
	"Super Ore Gardens (Mega 8 players)",
	"Switch (Med 4 players)",
	"The Berg (Mega 8 players)",
	"The Boyne (Med 4 players)",
	"The Bulge (Sm 4 players)",
	"The Cauldron (Lg 6 players)",
	"The Finger (Lg 6 players)",
	"The Hills Have Eyes (Mega 8 players)",
	"The Keyes (Med 6 players)",
	"The Lakes (Med 8 players)",
	"The Neck (Med 6 players)",
	"The Web (Lg 6 players)",
	"To the Core (Lg 4 players)",
	"Trafalgar (Lg 4 players)",
	"Twin Rivers (Sm 4 players)",
	"Umtumbo Gorge (Lg 4 players)",
	"Watch Your Step Extreme (Mega 8 players)",
	"Waterfalls (Lg 8 players)",
	"Waterloo Revisited (Lg 6 players)",
	"Water Werks (Mega 8 players)",
	"Warlord's Lake (Sm 4 players)",
	"Zama (Sm 4 players)",

	NULL
};
#endif

/*
**	bSpecialAftermathScenario (WOL_GSUP.CPP): Aftermath maps with Aftermath-only
**	units placed in them. Westwood defined it in a Westwood Online file, which
**	every shipped build included; this port does not build WOL.
*/
bool bSpecialAftermathScenario( const char* szScenarioDescription )
{
	//	Returns true if szScenarioDescription matches one of the descriptions for Aftermath multiplayer
	//	scenarios that have special Aftermath-only units *embedded* within them.
	if( strcmp( szScenarioDescription, "Booby Traps (Mega 8 players)" ) == 0 ||
		strcmp( szScenarioDescription, "Central Conflict Extreme (Mega 8 players)" ) == 0 ||
		strcmp( szScenarioDescription, "Circles of Death (Mega 8 players)" ) == 0 ||
		strcmp( szScenarioDescription, "Holy Grounds (Mega 8 players)" ) == 0 ||
		strcmp( szScenarioDescription, "Island Wars Extreme (Mega 8 players)" ) == 0 ||
		strcmp( szScenarioDescription, "King of the Hills Extreme (Mega 8 players)" ) == 0 ||
		strcmp( szScenarioDescription, "The Hills Have Eyes (Mega 8 players)" ) == 0 )
		return true;
	return false;
}

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
int NullModemClass::Init(int, int, char *, int, char, int, int, int)		{return 0;}		// 0 = error
void NullModemClass::Shutdown(void)												{}
int NullModemClass::Delete_Connection(void)									{return 1;}
int NullModemClass::Detect_Modem(SerialSettingsType *, int)				{return 0;}		// no modem found
DialStatusType NullModemClass::Dial_Modem(char *, DialMethodType, int)	{return DIAL_ERROR;}
DialStatusType NullModemClass::Answer_Modem(int)							{return DIAL_ERROR;}
int NullModemClass::Hangup_Modem(void)											{return 0;}
void NullModemClass::Setup_Modem_Echo(void (*)(char))						{}
void NullModemClass::Remove_Modem_Echo(void)									{}
void NullModemClass::Print_EchoBuf(void)										{}
void NullModemClass::Reset_EchoBuf(void)										{}
int NullModemClass::Change_IRQ_Priority(int)									{return 0;}
int NullModemClass::Get_Modem_Status(void)									{return 0;}		// no carrier
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

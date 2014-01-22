// This code is part of Pcap_DNSProxy
// Copyright (C) 2012-2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Pcap_DNSProxy_Base.h"

//Code define
#define RETURN_ERROR      -1
#define PACKET_MAXSIZE    2048                      //Maximum length of packets(2048 bytes)
#define THREAD_MAXNUM     128                       //Threads number
#define THREAD_PARTNUM    4                         //Parts of Threads, also define localhost sockets number: 0 is IPv6/UDP, 1 is IPv4/UDP, 2 is IPv6/TCP, 3 is IPv4/TCP
#define LOCALSERVERNAME   _T("PcapDNSProxyService") //Name of local server service
#define SYSTEM_SOCKET     UINT_PTR                  //System Socket defined, not the same in x86(unsigned int) and x64(unsigned __int64) platform
#define TIME_OUT          1000                      //Time out is 1000ms/1s

//Socket Data structure
typedef struct _socket_data_
{
	SYSTEM_SOCKET         Socket;
	sockaddr_storage      Sockaddr;
	int                   AddrLen;
}SOCKET_Data, SOCKET_DATA;

//Configuration class
class Configuration {
public:
//Base block(Public)
	bool            PrintError;
	size_t          Hosts;
	SYSTEM_SOCKET   LocalSocket[THREAD_PARTNUM];
	struct _dns_target_ {
		bool        IPv4;
		in_addr     IPv4Target;
		bool        IPv6;
		in6_addr    IPv6Target;
	}DNSTarget;
	bool            ServerMode;
	bool            TCPMode;
//Extend Test block(Public)
	struct _hoplimit_options_ {
		size_t      IPv4TTL;
		size_t      IPv6HopLimit;
		size_t      HopLimitFluctuation;
	}HopLimitOptions;
	bool            IPv4Options;
	struct _icmp_options_ {
		USHORT      ICMPID;
		USHORT      ICMPSequence;
		size_t      ICMPSpeed;
	}ICMPOptions;
	bool            TCPOptions;
	bool            DNSOptions;
	bool            Blacklist;
//Data block(Public)
	struct _domaintest_options_ {
		bool        DomainTestCheck;
		PSTR        DomainTest;
		USHORT      DomainTestID;
		size_t      DomainTestSpeed;
	}DomainTestOptions;
	struct _paddingdata_options_ {
		PSTR        PaddingData;
		size_t      PaddingDataLength;
	}PaddingDataOptions;
	struct _localhostserver_options_ {
		PSTR        LocalhostServer;
		size_t      LocalhostServerLength;
	}LocalhostServerOptions;
	::Configuration();
	~Configuration();

	SSIZE_T ReadParameter();
	SSIZE_T ReadHosts();
};

//Hosts list
class HostsTable {
public:
	size_t           Protocol;
	PSTR             Response;
	size_t           ResponseLength;
	size_t           ResponseNum;
	std::regex       Pattern;

	HostsTable();
};

//System&Request port list
class PortTable {
public:
	SOCKET_DATA      *RecvData;    //System receive sockets/Addresses records
	PUSHORT          SendPort;    //Request ports records
	PortTable();
	~PortTable();

	SSIZE_T PortTable::MatchToSend(const PSTR Buffer, const USHORT RequestPort, const size_t Length);
};

//PrintError.cpp
SSIZE_T __stdcall PrintError(const size_t Type, const PWSTR Message, const SSIZE_T Code, const size_t Line);

//Protocol.cpp
ULONG __stdcall GetFCS(const PSTR Buffer, const size_t Length);
USHORT __stdcall GetChecksum(const USHORT *Buffer, size_t Length);
USHORT __stdcall ICMPv6Checksum(const PSTR Buffer, const size_t Length);
USHORT __stdcall UDPChecksum(const PSTR Buffer, const size_t Length, const size_t Protocol);
size_t __stdcall CharToDNSQuery(const PSTR FName, PSTR TName);
size_t __stdcall DNSQueryToChar(const PSTR TName, PSTR FName);
bool __stdcall GetLocalAddress(sockaddr_storage &LocalAddr, const int Protocol);
SSIZE_T __stdcall LocalAddressToPTR(std::string &Result, const size_t Protocol);
void __stdcall RamdomDomain(PSTR Domain, const size_t Length);

//Configuration.cpp
bool __stdcall ReadEncoding(size_t &Encoding, PSTR Buffer, std::string &sBuffer, const size_t Length);
inline void __stdcall CleanupHostsTable();

//Service.cpp
SSIZE_T __stdcall GetServiceInfo();
size_t WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
size_t WINAPI ServiceControl(const DWORD dwControlCode);
BOOL WINAPI ExecuteService();
void __stdcall TerminateService();
DWORD WINAPI ServiceProc(LPVOID lpParameter);
BOOL WINAPI UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint, DWORD dwWaitHint);

//Process.cpp
SSIZE_T __stdcall RequestProcess(const PSTR Send, const size_t Length, SOCKET_DATA FunctionData, const size_t Protocol, const size_t Index);
inline SSIZE_T __stdcall CheckHosts(const PSTR Request, PSTR Result, const size_t Length);
SSIZE_T __stdcall TCPReceiveProcess(const SOCKET_DATA FunctionData, const size_t Index);

//Monitor.cpp
inline SSIZE_T __stdcall FirewallTest();
SSIZE_T __stdcall MonitorInitialization();
SSIZE_T __stdcall UDPMonitor(const SOCKET_DATA LocalhostData);
SSIZE_T __stdcall TCPMonitor(const SOCKET_DATA LocalhostData);

//Captrue.cpp
SSIZE_T __stdcall CaptureInitialization();
SSIZE_T __stdcall Capture(const pcap_if *pDrive);
SSIZE_T __stdcall IPLayer(const PSTR Recv, const size_t Length, const USHORT Protocol);
inline bool __stdcall ICMPCheck(const PSTR Buffer, const size_t Length, const size_t Protocol);
inline bool __stdcall TCPCheck(const PSTR Buffer);
inline bool __stdcall DTDNSOCheck(const PSTR Buffer, bool &SignHopLimit);
inline SSIZE_T __stdcall DNSMethod(const PSTR Recv, const size_t Length, const size_t Protocol);

//Request.cpp
SSIZE_T __stdcall DomainTest(const size_t Protocol);
SSIZE_T __stdcall ICMPEcho();
SSIZE_T __stdcall ICMPv6Echo();
SSIZE_T __stdcall TCPRequest(const PSTR Send, PSTR Recv, const size_t SendSize, const size_t RecvSize, const SOCKET_DATA TargetData);
SSIZE_T __stdcall UDPRequest(const PSTR Send, const size_t Length, const SOCKET_DATA TargetData, const SSIZE_T Index);

//Console.cpp
BOOL WINAPI CtrlHandler(const DWORD fdwCtrlType);
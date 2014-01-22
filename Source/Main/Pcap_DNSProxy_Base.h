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

//////////////////////////////////////////////////
// Base Header
//C Standard Library header
#include <ctime>                   //Time
//#include <cstdio>                  //I/O

//C++ Standard Template Library
#include <thread>                  //Thread
#include <vector>                  //Vector
#include <regex>                   //Regular Expression
//#include <string>                  //String
//#include <mutex>                   //Mutex Lock‎

//Windows Network API header
/*
#include <winsock2.h>              //Winsock, must be including before windows.h
#include <winsvc.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
*/

//WinPcap header
#include "WinPcap/pcap.h"

//Windows API header
#include <tchar.h>                 //Unicode/Wide-Character Set Support
//#include <windows.h>

//////////////////////////////////////////////////
// Base Define
#pragma comment(lib, "ws2_32.lib")            //Winsock
//#pragma comment(lib, "iphlpapi.lib")        //Winsock
#ifdef _WIN64
#pragma comment(lib, "WinPcap/wpcap_x64.lib") //WinPcap library(x64)
#else 
#pragma comment(lib, "WinPcap/wpcap_x86.lib") //WinPcap library(x86)
#endif

#pragma pack(1)                    //Memory alignment
#define LITTLE_ENDIAN      1       //OS: Windows
//#pragma comment(linker, "/subsystem:windows /entry:mainCRTStartup") //Hide console

//Remember "WPCAP" and "HAVE_REMOTE" to preprocessor options!


//////////////////////////////////////////////////
// Protocol Header structures
//Ethernet Frame Header
//ARP = ETHERTYPE_ARP/0x0806 & RARP = ETHERTYPE_RARP/0x8035
//PPPoE(Connecting) = ETHERTYPE_PPPOED/0x8863
//802.1X = ETHERTYPE_EAPOL/0x888E
#define ETHERTYPE_IP       0x0800  //IPv4 over Ethernet
#define ETHERTYPE_IPV6     0x86DD  //IPv6 over Ethernet
#define ETHERTYPE_PPPOES   0x8864  //PPPoE(Transmission)
typedef struct _eth_hdr
{
	UCHAR                  Dst[6];
	UCHAR                  Src[6];
	USHORT                 Type;
}eth_hdr;

//Point-to-Point Protocol over Ethernet/PPPoE Header
#define PPPOETYPE_IPV4     0x0021  //IPv4 over PPPoE
#define PPPOETYPE_IPV6     0x0057  //IPv6 over PPPoE
typedef struct _pppoe_hdr
{
	UCHAR                  VersionType;
	UCHAR                  Code;
	USHORT                 SessionID;
	USHORT                 Length;
	USHORT                 Protocol;
}pppoe_hdr;

//802.1X Protocol Header
/*
typedef struct _802_1x_hdr
{
	UCHAR                  Version;
	UCHAR                  Type;
	USHORT                 Length;
};
*/

//Internet Protocol version 4/IPv4 Header
typedef struct _ipv4_hdr
{
#if LITTLE_ENDIAN
	UCHAR                  IHL:4;         //Header Length
	UCHAR                  Version:4;
#else //BIG_ENDIAN
	UCHAR                  Version:4;
	UCHAR                  IHL:4;         //Header Length
#endif
	union {
		UCHAR              TOS;           //Type of service, but RFC 2474 redefine it to DSCP and ECN
		struct {
		#if LITTLE_ENDIAN
			UCHAR          DSCP_First:4;  //DiffServ/Differentiated Services first part
			UCHAR          ECN:2;         //Explicit Congestion Notification
			UCHAR          DSCP_Second:2; //DiffServ/Differentiated Services second part
		#else //BIG_ENDIAN
			UCHAR          DSCP:6;        //DiffServ/Differentiated Services
			UCHAR          ECN:2;         //Explicit Congestion Notification
		#endif
		}TOSBits;
	};
	USHORT                 Length;
	USHORT                 ID;
	union {
		USHORT             Flags;
		struct {
		#if LITTLE_ENDIAN
			UCHAR          FO_First:4;    //Fragment Offset first part
			UCHAR          Zero:1;        //Reserved bit
			UCHAR          DF:1;          //Don't Fragment
			UCHAR          MF:1;          //More Fragments
			UCHAR          FO_Second_A:1; //Fragment Offset Second-A part
			UCHAR          FO_Second_B;   //Fragment Offset Second-B part
		#else //BIG_ENDIAN
			UCHAR          Zero:1;        //Reserved bit
			UCHAR          DF:1;          //Don't Fragment
			UCHAR          MF:1;          //More Fragments
			UCHAR          FO_First_A:1;  //Fragment Offset First-A part
			UCHAR          FO_First_B:4;  //Fragment Offset First-B part
			UCHAR          FO_Second;     //Fragment Offset second part
		#endif
		}FlagsBits;
	};
	UCHAR                  TTL;
	UCHAR                  Protocol;
	USHORT                 Checksum;
	in_addr                Src;
	in_addr                Dst;
}ipv4_hdr;

//Internet Protocol version 6/IPv6 Header
typedef struct _ipv6_hdr
{
	union {
		ULONG              VerTcFlow;
		struct {
#if LITTLE_ENDIAN
			UCHAR          TrafficClass_First:4;     //Traffic Class first part
			UCHAR          Version:4;
			UCHAR          FlowLabel_First:4;        //Traffic Class second part
			UCHAR          TrafficClass_Second:4;    //Traffic Class second part
			USHORT         FlowLabel_Second;         //Flow Label second part
#else //BIG_ENDIAN
			UCHAR          Version:4;
			UCHAR          TrafficClass_First:4;     //Traffic Class first part
			UCHAR          TrafficClass_Second:4;    //Traffic Class second part
			UCHAR          FlowLabel_First:4;        //Flow Label first part
			USHORT         FlowLabel_Second;         //Flow Label second part
#endif
		}VerTcFlowBits;
	};
	USHORT                 PayloadLength;
	UCHAR                  NextHeader;
	UCHAR                  HopLimit;
	in6_addr               Src;
	in6_addr               Dst;
}ipv6_hdr;

//Generic Routing Encapsulation/GRE Protocol Header
/*
#define IPPROTO_GRE 47
typedef struct _gre_hdr
{
	USHORT                Flags_Version;
	USHORT                Type;
}gre_hdr;
*/

//Internet Control Message Protocol/ICMP Header
typedef struct _icmp_hdr
{
	UCHAR                 Type;
	UCHAR                 Code;
	USHORT                Checksum;
	USHORT                ID;
	USHORT                Sequence;
//	ULONG                 TimeStamp;   //Non-essential
}icmp_hdr;

//Internet Control Message Protocol version 6/ICMPv6 Header
#define ICMPV6_REQUEST    128
#define ICMPV6_REPLY      129
typedef struct _icmpv6_hdr
{
	UCHAR                 Type;
	UCHAR                 Code;
	USHORT                Checksum;
	USHORT                ID;
	USHORT                Sequence;
//	USHORT                Nonce;      //Non-essential
}icmpv6_hdr;

//Transmission Control Protocol/TCP Header
typedef struct _tcp_hdr
{
	USHORT                Src_Port;
	USHORT                Dst_Port;
	ULONG                 Sequence;
	ULONG                 Acknowledge;
#if LITTLE_ENDIAN
	UCHAR                 Reserved_First:4;
	UCHAR                 HeaderLength:4;
	union {
		struct {
			UCHAR         Flags:6;
			UCHAR         Reseverd_Second:2;
		}FlagsAll;
		struct {
			UCHAR         PSH:1;
			UCHAR         RST:1;
			UCHAR         SYN:1;
			UCHAR         FIN:1;
			UCHAR         URG:1;
			UCHAR         ACK:1;
			UCHAR         Reseverd_Second:2;
		}FlagsBits;
	};
#else //BIG_ENDIAN
	UCHAR                 Header_Length:4;
	UCHAR                 Reserved_First:4;
	union {
		struct {
			UCHAR         Reseverd_Second:2;
			UCHAR         Flags:6;
		}FlagsAll;
		struct {
			UCHAR         Reseverd_Second:2;
			UCHAR         URG:1;
			UCHAR         ACK:1;
			UCHAR         PSH:1;
			UCHAR         RST:1;
			UCHAR         SYN:1;
			UCHAR         FIN:1;
		}FlagsBits;
	};
#endif
	USHORT                Windows;
	USHORT                Checksum;
	USHORT                Urgent_Pointer;
}tcp_hdr;

//User Datagram Protocol/UDP Header
typedef struct _udp_hdr
{
	USHORT                Src_Port;
	USHORT                Dst_Port;
	USHORT                Length;
	USHORT                Checksum;
}udp_hdr;

//TCP or UDP Pseudo Header(Using to get Checksum) with IPv4
typedef struct _ipv4_psd_hdr
{
	in_addr               Src;
	in_addr               Dst;
	UCHAR                 Zero;
	UCHAR                 Protocol;
	USHORT                Length;
}ipv4_psd_hdr;

//ICMPv6, TCP or UDP Pseudo Header(Using to get Checksum) with IPv6
typedef struct _ipv6_psd_hdr
{
	in6_addr              Src;
	in6_addr              Dst;
	ULONG                 Length;
	UCHAR                 Zero[3];
	UCHAR                 Next_Header;
}ipv6_psd_hdr;

//DNS Header
#define DNS_Port          53       //DNS Port(TCP and UDP)
// DNS Records ID
#define Class_IN          0x0001   //DNS Class IN, its ID is 1
#define A_Records         0x0001   //DNS A records, its ID is 1
#define CNAME_Records     0x0005   //DNS CNAME records, its ID is 5
#define PTR_Records       0x000C   //DNS PTR records, its ID is 12
#define AAAA_Records      0x001C   //DNS AAAA records, its ID is 28
typedef struct _dns_hdr
{
	USHORT                ID;
	union {
		USHORT            Flags;
	#if LITTLE_ENDIAN
		struct {
			UCHAR         OPCode_Second:1;
			UCHAR         AA:1;             //Authoritative Answer
			UCHAR         TC:1;             //Truncated
			UCHAR         RD:1;             //Recursion Desired
			UCHAR         QR:1;             //Response
			UCHAR         OPCode_First:3;
			UCHAR         RCode:4;          //Reply code
			UCHAR         RA:1;             //Recursion available
			UCHAR         Zero:1;           //Reserved
			UCHAR         AD:1;             //Answer authenticated
			UCHAR         CD:1;             //Non-authenticated data
	#else //BIG_ENDIAN
		struct {
			UCHAR         QR:1;             //Response
			UCHAR         OPCode:4;
			UCHAR         AA:1;             //Authoritative
			UCHAR         TC:1;             //Truncated
			UCHAR         RD:1;             //Recursion desired
			UCHAR         RA:1;             //Recursion available
			UCHAR         Zero:1;           //Reserved
			UCHAR         AD:1;             //Answer authenticated
			UCHAR         CD:1;             //Non-authenticated data
			UCHAR         RCode:4;          //Reply code
	#endif
		}FlagsBits;
	};
	USHORT                Questions;
	USHORT                Answer;
	USHORT                Authority;
	USHORT                Additional;
}dns_hdr;

//DNS Query
typedef struct _dns_qry
{
//	PUCHAR                Name;
	USHORT                Type;
	USHORT                Classes;
}dns_qry;

//DNS A record response
typedef struct _dns_a_
{
	USHORT                Name;
	USHORT                Type;
	USHORT                Classes;
	ULONG                 TTL;
	USHORT                Length;
	in_addr               Addr;
}dns_a_record;

//DNS CNAME record response
typedef struct _dns_cname_
{
	USHORT                PTR;
	USHORT                Type;
	USHORT                Classes;
	ULONG                 TTL;
	USHORT                Length;
//	PUCHAR                PrimaryName;
}dns_cname_record;

//DNS PTR record response
typedef struct _dns_ptr_
{
	USHORT                Name;
	USHORT                Type;
	USHORT                Classes;
	ULONG                 TTL;
	USHORT                Length;
//	PUCHAR                DomainName;
}dns_ptr_record;

//DNS AAAA record response
typedef struct _dns_aaaa_
{
	USHORT                Name;
	USHORT                Type;
	USHORT                Classes;
	ULONG                 TTL;
	USHORT                Length;
	in6_addr              Addr;
}dns_aaaa_record;
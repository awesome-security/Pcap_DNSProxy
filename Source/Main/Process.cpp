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


#include "Pcap_DNSProxy.h"

PortTable PortList;

extern Configuration Parameter;
extern std::string LocalhostPTR[2];
extern std::vector<HostsTable> *Using, *Modificating;

//Independent request process
SSIZE_T __stdcall RequestProcess(const PSTR Send, const size_t Length, SOCKET_DATA FunctionData, const size_t Protocol, const size_t Index)
{
//Initialization
	PSTR SendBuffer = nullptr, RecvBuffer = nullptr;
	try {
		SendBuffer = new char[PACKET_MAXSIZE]();
		RecvBuffer = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		delete[] SendBuffer;
		delete[] RecvBuffer;
		TerminateService();
		return RETURN_ERROR;
	}
	memset(SendBuffer, 0, PACKET_MAXSIZE);
	memset(RecvBuffer, 0, PACKET_MAXSIZE);
	dns_hdr *dnshdr = (dns_hdr *)SendBuffer;
	memcpy(SendBuffer, Send, Length);

//Check hosts
	if (dnshdr->Questions == htons(0x0001))
	{
		SSIZE_T HostLength = CheckHosts(SendBuffer, RecvBuffer, Length);
		if (HostLength > sizeof(dns_hdr) && HostLength < PACKET_MAXSIZE)
		{
			if (Protocol == IPPROTO_TCP) //TCP
			{
				USHORT DataLength = htons((USHORT)HostLength);
				send(FunctionData.Socket, (PSTR)&DataLength, sizeof(USHORT), NULL);
				send(FunctionData.Socket, RecvBuffer, (int)HostLength, NULL);
			}
			else { //UDP
				sendto(FunctionData.Socket, RecvBuffer, (int)HostLength, NULL, (PSOCKADDR)&(FunctionData.Sockaddr), FunctionData.AddrLen);
			}

			delete[] SendBuffer;
			delete[] RecvBuffer;
			return 0;
		}
	}

	if (Parameter.TCPMode)
	{
		SSIZE_T SendLen = TCPRequest(SendBuffer, RecvBuffer, Length, PACKET_MAXSIZE, FunctionData);
		if (SendLen > 0 && SendLen < PACKET_MAXSIZE)
		{
			if (Protocol == IPPROTO_TCP) //TCP
			{
				USHORT DataLength = htons((USHORT)SendLen);
				send(FunctionData.Socket, (PSTR)&DataLength, sizeof(USHORT), NULL);
				send(FunctionData.Socket, RecvBuffer, (int)SendLen, NULL);
			}
			else { //UDP
				sendto(FunctionData.Socket, RecvBuffer, (int)SendLen, NULL, (PSOCKADDR)&(FunctionData.Sockaddr), FunctionData.AddrLen);
			}
		}
		else { //The connection is RESET or other errors when connecting.
			PortList.RecvData[Index] = FunctionData;
			UDPRequest(SendBuffer, Length, FunctionData, Index);
		}
	}
	else {
		PortList.RecvData[Index] = FunctionData;
		UDPRequest(SendBuffer, Length, FunctionData, Index);
	}

	delete[] SendBuffer;
	delete[] RecvBuffer;
	return 0;
}

//Check hosts from list
inline SSIZE_T __stdcall CheckHosts(const PSTR Request, PSTR Result, const size_t Length)
{
//Initialization
	PSTR Buffer = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		TerminateService();
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);

	size_t LengthString = DNSQueryToChar(Request + sizeof(dns_hdr), Buffer);
	std::string Domain(Buffer);
	delete[] Buffer;

//Response
	memcpy(Result, Request, Length);
	dns_hdr *hdr = (dns_hdr *)Result;
	hdr->Flags = htons(0x8180); //Standard query response and no error
	dns_qry *qry = (dns_qry *)(Result + Length - sizeof(dns_qry));

//Class IN
	if (qry->Classes != htons(Class_IN))
		return 0;

//PTR Records
	if (qry->Type == htons(PTR_Records))
	{
		//IPv4 Regex
		static const std::regex IPv4PrivateB(".(1[6-9]|2[0-9]|3[01]).172.in-addr.arpa", std::regex_constants::extended);
		//IPv6 Regex
		static const std::regex IPv6ULA(".f.[cd].([0-9]|[a-f]).([0-9]|[a-f]).ip6.arpa", std::regex_constants::extended);
		static const std::regex IPv6LUC(".f.f.([89]|[ab]).([0-9]|[a-f]).ip6.arpa", std::regex_constants::extended);

		//IPv4 check
		if (Domain.find(LocalhostPTR[0]) != std::string::npos || //IPv4 Localhost
			Domain.find(".10.in-addr.arpa") != std::string::npos || //Private class A address(10.0.0.0/8, Section 3 in RFC 1918)
			Domain.find(".127.in-addr.arpa") != std::string::npos || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
			Domain.find(".254.169.in-addr.arpa") != std::string::npos || //Link-local address(169.254.0.0/16, RFC 3927)
			std::regex_search(Domain, IPv4PrivateB) || //Private class B address(172.16.0.0/16, Section 3 in RFC 1918)
			Domain.find(".168.192.in-addr.arpa") != std::string::npos || //Private class C address(192.168.0.0/24, Section 3 in RFC 1918)
		//IPv6 check
			Domain.find("1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa") != std::string::npos || //Loopback address(::1, Section 2.5.3 in RFC 4291)
			Domain.find(LocalhostPTR[1]) != std::string::npos || //IPv6 Localhost
			std::regex_search(Domain, IPv6ULA) || //Unique Local Unicast address/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
			std::regex_search(Domain, IPv6LUC)) //Link-Local Unicast Contrast address(FE80::/10, Section 2.5.6 in RFC 4291)
		{
			hdr->Answer = htons(0x0001);
			dns_ptr_record *rsp = (dns_ptr_record *)(Result + Length);
			rsp->Name = htons(0xC00C); //Pointer of same request
			rsp->Classes = htons(Class_IN); //Class IN
			rsp->TTL = htonl(600); //10 minutes
			rsp->Type = htons(PTR_Records);
			rsp->Length = htons((USHORT)(Parameter.LocalhostServerOptions.LocalhostServerLength) + 1);
			memcpy(Result + Length + sizeof(dns_ptr_record), Parameter.LocalhostServerOptions.LocalhostServer, Parameter.LocalhostServerOptions.LocalhostServerLength + 1);
			return Length + sizeof(dns_ptr_record) + Parameter.LocalhostServerOptions.LocalhostServerLength + 2;
		}
	}

	if (!Using->empty())
	{
	//AAAA Records
		if (qry->Type == htons(AAAA_Records))
		{
			for (std::vector<HostsTable>::iterator iter = Using->begin();iter != Using->end();iter++)
			{
				if (std::regex_search(Domain, iter->Pattern) && iter->Protocol == AF_INET6)
				{
					hdr->Answer = htons((USHORT)iter->ResponseNum);
					memcpy(Result + Length, iter->Response, iter->ResponseLength);
					return Length + iter->ResponseLength;
				}
			}
		}

	//A record
		if (qry->Type == htons(A_Records))
		{
			for (std::vector<HostsTable>::iterator iter = Using->begin();iter != Using->end();iter++)
			{
				if (std::regex_search(Domain, iter->Pattern) && iter->Protocol == AF_INET)
				{
					hdr->Answer = htons((USHORT)iter->ResponseNum);
					memcpy(Result + Length, iter->Response, iter->ResponseLength);
					return Length + iter->ResponseLength;
				}
			}
		}
	}

	return 0;
}

//Receive packets
SSIZE_T __stdcall TCPReceiveProcess(const SOCKET_DATA FunctionData, const size_t Index)
{
//Initialization
	PSTR Buffer = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		TerminateService();
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);

//Receive
	bool PUD = false, Sign = false;
	SSIZE_T RecvLength = 0;
	while (!Sign)
	{
		RecvLength = recv(FunctionData.Socket, Buffer, PACKET_MAXSIZE, NULL);
		if (RecvLength == sizeof(USHORT)) //TCP segment of a reassembled PDU
		{
			PUD = true;
			continue;
		}
		else if (RecvLength > sizeof(dns_hdr))
		{
			Sign = true;
			if (PUD)
			{
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
					RequestProcess(Buffer, RecvLength, FunctionData, IPPROTO_TCP, Index + THREAD_MAXNUM*(THREAD_PARTNUM - 2));
				else //IPv4
					RequestProcess(Buffer, RecvLength, FunctionData, IPPROTO_TCP, Index + THREAD_MAXNUM*(THREAD_PARTNUM - 1));
			}
			else {
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
					RequestProcess(Buffer + sizeof(USHORT), RecvLength - sizeof(USHORT), FunctionData, IPPROTO_TCP, Index + THREAD_MAXNUM*(THREAD_PARTNUM - 2));
				else //IPv4
					RequestProcess(Buffer + sizeof(USHORT), RecvLength - sizeof(USHORT), FunctionData, IPPROTO_TCP, Index + THREAD_MAXNUM*(THREAD_PARTNUM - 1));

			}
		}

		delete[] Buffer;
		return 0;
	}

	delete[] Buffer;
	return 0;
}
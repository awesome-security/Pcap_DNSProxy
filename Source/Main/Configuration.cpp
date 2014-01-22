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

#define UTF_8             65001       //Microsoft Windows Codepage of UTF-8
#define UTF_16_LE         1200 * 100  //Microsoft Windows Codepage of UTF-16 Little Endian/LE(Make it longer than the PACKET_MAXSIZE)
#define UTF_16_BE         1201 * 100  //Microsoft Windows Codepage of UTF-16 Big Endian/BE(Make it longer than the PACKET_MAXSIZE)
#define UTF_32_LE         12000       //Microsoft Windows Codepage of UTF-32 Little Endian/LE
#define UTF_32_BE         12001       //Microsoft Windows Codepage of UTF-32 Big Endian/BE

std::vector<HostsTable> HostsList[2], *Using = &HostsList[0], *Modificating = &HostsList[1];

extern std::wstring Path;
extern Configuration Parameter;

//Read parameter from configuration file
SSIZE_T Configuration::ReadParameter()
{
//Initialization
	FILE *Input = nullptr;
	PSTR Buffer = nullptr, Target = nullptr, LocalhostServer = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
		Target = new char[PACKET_MAXSIZE/32]();
		LocalhostServer = new char[PACKET_MAXSIZE/8]();
	}
	catch (std::bad_alloc)
	{
		::PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		delete[] Buffer;
		delete[] Target;
		delete[] LocalhostServer;
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);
	memset(Target, 0, PACKET_MAXSIZE/32);
	memset(LocalhostServer, 0, PACKET_MAXSIZE/8);

//Open file
	std::wstring ConfigPath(Path);
	ConfigPath.append(_T("Config.ini"));
	_wfopen_s(&Input, ConfigPath.c_str(), _T("r"));

	if (Input == nullptr)
	{
		::PrintError(2, _T("Cannot open configuration file(Config.ini)"), NULL, NULL);

		delete[] Buffer;
		delete[] Target;
		delete[] LocalhostServer;
		return RETURN_ERROR;
	}

	static const char PaddingData[] = ("abcdefghijklmnopqrstuvwabcdefghi"); //Microsoft Windows Ping padding data
	static const char LocalDNSName[] = ("pcap_dnsproxy.localhost.server"); //Localhost DNS server name
	size_t Encoding = 0, Line = 1;
	SSIZE_T Result = 0;
	std::string sBuffer;
	while(!feof(Input))
	{
		memset(Buffer, 0, PACKET_MAXSIZE);
		fgets(Buffer, PACKET_MAXSIZE, Input);

	//Check encoding
		if (!ReadEncoding(Encoding, Buffer, sBuffer, PACKET_MAXSIZE))
			continue;
		memset(Buffer, 0, PACKET_MAXSIZE);
		memcpy(Buffer, sBuffer.c_str(), sBuffer.length());
		Result = 0;
		Line++;

	//Base block
		if (sBuffer.find("Print Error = ") == 0 && sBuffer.length() < 17)
		{
			Result = atoi(Buffer + 14);
			if (Result == 0)
				this->PrintError = false;
		}
		else if (sBuffer.find("Hosts = ") == 0 && sBuffer.length() < 25)
		{
			Result = atoi(Buffer + 8);
			if (Result >= 5)
				this->Hosts = Result * 1000;
			else if (Result > 0 && Result < 5)
				this->Hosts = 5000; //5s is least time between auto-refreshing
			else 
				this->Hosts = 0; //Read Hosts OFF
		}
		else if (sBuffer.find("IPv4 DNS Address = ") == 0 && sBuffer.length() > 25 && sBuffer.length() < 35)
		{
			if (sBuffer.find('.') == std::string::npos) //IPv4 Address
			{
				::PrintError(2, _T("DNS server IPv4 Address format error"), NULL, Line);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}

		//IPv4 Address check
			memcpy(Target, Buffer + 19, sBuffer.length() - 19);
			Result = inet_pton(AF_INET, Target, &(this->DNSTarget.IPv4Target));
			if (Result == FALSE)
			{
				::PrintError(2, _T("DNS server IPv4 Address format error"), NULL, Line);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}
			else if (Result == RETURN_ERROR)
			{
				::PrintError(2, _T("DNS server IPv4 Address convert failed"), WSAGetLastError(), Line);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}

			this->DNSTarget.IPv4 = true;
		}
		else if (sBuffer.find("IPv6 DNS Address = ") == 0 && sBuffer.length() > 21 && sBuffer.length() < 59)
		{
			if (sBuffer.find(':') == std::string::npos) //IPv6 Address
			{
				::PrintError(2, _T("DNS server IPv6 Address format error"), NULL, Line);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}

		//IPv6 Address check
			memcpy(Target, Buffer + 19, sBuffer.length() - 19);
			Result = inet_pton(AF_INET6, Target, &(this->DNSTarget.IPv6Target));
			if (Result == FALSE)
			{
				::PrintError(2, _T("DNS server IPv6 Address format error"), NULL, Line);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}
			else if (Result == RETURN_ERROR)
			{
				::PrintError(2, _T("DNS server IPv6 Address convert failed"), WSAGetLastError(), Line);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}

			this->DNSTarget.IPv6 = true;
		}
		else if (sBuffer.find("Operation Mode = ") == 0 && sBuffer.length() < 24)
		{
			if (sBuffer.find("Server") == 17)
				this->ServerMode = true;
		}
		else if (sBuffer.find("Protocol = ") == 0 && sBuffer.length() < 15)
		{
			if (sBuffer.find("TCP") == 11)
				this->TCPMode = true;
		}
	//Extend Test
		else if (sBuffer.find("IPv4 TTL = ") == 0 && sBuffer.length() > 11 && sBuffer.length() < 15)
		{
			Result = atoi(Buffer + 11);
			if (Result > 0 && Result < 256)
				this->HopLimitOptions.IPv4TTL = Result;
		}
		else if (sBuffer.find("IPv6 Hop Limits = ") == 0 && sBuffer.length() > 18 && sBuffer.length() < 22)
		{
			Result = atoi(Buffer + 18);
			if (Result > 0 && Result < 256)
				this->HopLimitOptions.IPv6HopLimit = Result;
		}
		else if (sBuffer.find("Hop Limits/TTL Fluctuation = ") == 0 && sBuffer.length() > 29 && sBuffer.length() < 34)
		{
			Result = atoi(Buffer + 29);
			if (Result >= 0 && Result < 255)
				this->HopLimitOptions.HopLimitFluctuation = Result;
		}
		else if (sBuffer.find("IPv4 Options Filter = ") == 0 && sBuffer.length() < 24)
		{
			Result = atoi(Buffer + 22);
			if (Result == 1)
				this->IPv4Options = true;
		}
		else if (sBuffer.find("ICMP Test = ") == 0 && sBuffer.length() < 23)
		{
			Result = atoi(Buffer + 12);
			if (Result >= 5)
				this->ICMPOptions.ICMPSpeed = Result* 1000;
			else if (Result > 0 && Result < 5)
				this->ICMPOptions.ICMPSpeed = 5000; //5s is least time between ICMP Tests
			else 
				this->ICMPOptions.ICMPSpeed = 0; //ICMP Test OFF
		}
		else if (sBuffer.find("ICMP ID = ") == 0 && sBuffer.length() < 17)
		{
			Result = strtol(Buffer + 10, NULL, 16);
			if (Result > 0)
				this->ICMPOptions.ICMPID = htons((USHORT)Result);
		}
		else if (sBuffer.find("ICMP Sequence = ") == 0 && sBuffer.length() < 23)
		{
			Result = strtol(Buffer + 16, NULL, 16);
			if (Result > 0)
				this->ICMPOptions.ICMPSequence = htons((USHORT)Result);
		}
		else if (sBuffer.find("TCP Options Filter = ") == 0 && sBuffer.length() < 23)
		{
			Result = atoi(Buffer + 21);
			if (Result == 1)
				this->TCPOptions = true;
		}
		else if (sBuffer.find("DNS Options Filter = ") == 0 && sBuffer.length() < 23)
		{
			Result = atoi(Buffer + 21);
			if (Result == 1)
				this->DNSOptions = true;
		}
		else if (sBuffer.find("Blacklist Filter = ") == 0 && sBuffer.length() < 22)
		{
			Result = atoi(Buffer + 19);
			if (Result == 1)
				this->Blacklist = true;
		}
	//Data block
		else if (sBuffer.find("Domain Test = ") == 0)
		{
			if (sBuffer.length() > 17 && sBuffer.length() < 270) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
			{
				memcpy(this->DomainTestOptions.DomainTest, Buffer + 14, sBuffer.length() - 14);
				this->DomainTestOptions.DomainTestCheck = true;
			}
			else {
				continue;
			}
		}
		else if (sBuffer.find("Domain Test ID = ") == 0 && sBuffer.length() < 24)
		{
			Result = strtol(Buffer + 17, NULL, 16);
			if (Result > 0)
				this->DomainTestOptions.DomainTestID = htons((USHORT)Result);
		}
		else if (sBuffer.find("Domain Test Speed = ") == 0 && sBuffer.length() < 30 /* && Parameter.DomainTest[0] != 0 */ )
		{
			Result = atoi(Buffer + 20);
			if (Result > 0)
				this->DomainTestOptions.DomainTestSpeed = Result * 1000;
		}
		else if (sBuffer.find("ICMP PaddingData = ") == 0)
		{
			if (sBuffer.length() > 36 && sBuffer.length() < 84) //The length of ICMP padding data must between 18 bytes and 64 bytes.
			{
				this->PaddingDataOptions.PaddingDataLength = sBuffer.length() - 18;
				memcpy(this->PaddingDataOptions.PaddingData, Buffer + 19, sBuffer.length() - 19);
			}
			else if (sBuffer.length() >= 84)
			{
				::PrintError(2, _T("The ICMP padding data is too long"), NULL, Line);
				continue;
			}
			else {
				continue;
			}
		}
		else if (sBuffer.find("Localhost Server Name = ") == 0 && sBuffer.length() > 26 && sBuffer.length() < 280) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
			PINT Point = nullptr;
			try {
				Point = new int[PACKET_MAXSIZE/8]();
			}
			catch (std::bad_alloc)
			{
				::PrintError(1, _T("Memory allocation failed"), NULL, NULL);

				delete[] Buffer;
				delete[] Target;
				delete[] LocalhostServer;
				return RETURN_ERROR;
			}
			memset(Point, 0, sizeof(int)*(PACKET_MAXSIZE/8));

			size_t Index = 0;
			this->LocalhostServerOptions.LocalhostServerLength = sBuffer.length() - 24;

		//Convert from char to DNS query
			LocalhostServer[0] = 46;
			memcpy(LocalhostServer + sizeof(char), Buffer + 24, this->LocalhostServerOptions.LocalhostServerLength);
			for (Index = 0;Index < sBuffer.length() - 25;Index++)
			{
				if (LocalhostServer[Index] == 45 || LocalhostServer[Index] == 46 || LocalhostServer[Index] == 95 || 
					LocalhostServer[Index] > 47 && LocalhostServer[Index] < 58 || LocalhostServer[Index] > 96 && LocalhostServer[Index] < 123)
				{
					if (LocalhostServer[Index] == 46)
					{
						Point[Result] = (int)Index;
						Result++;
					}
					continue;
				}
				else {
					::PrintError(2, _T("Localhost server name format error"), NULL, Line);
					this->LocalhostServerOptions.LocalhostServerLength = 0;
					break;
				}
			}

			if (this->LocalhostServerOptions.LocalhostServerLength > 2)
			{
				PSTR LocalhostServerName = nullptr;
				try {
					LocalhostServerName = new char[PACKET_MAXSIZE/8]();
				}
				catch (std::bad_alloc)
				{
					::PrintError(1, _T("Memory allocation failed"), NULL, NULL);

					delete[] Buffer;
					delete[] Target;
					delete[] LocalhostServer;
					delete[] Point;
					return RETURN_ERROR;
				}
				memset(LocalhostServerName, 0, PACKET_MAXSIZE/8);

				for (Index = 0;Index < (size_t)Result;Index++)
				{
					if (Index == Result - 1)
					{
						LocalhostServerName[Point[Index]] = (int)(this->LocalhostServerOptions.LocalhostServerLength - Point[Index]);
						memcpy(LocalhostServerName + Point[Index] + 1, LocalhostServer + Point[Index] + 1, this->LocalhostServerOptions.LocalhostServerLength - Point[Index]);
					}
					else {
						LocalhostServerName[Point[Index]] = Point[Index + 1] - Point[Index] - 1;
						memcpy(LocalhostServerName + Point[Index] + 1, LocalhostServer + Point[Index] + 1, Point[Index + 1] - Point[Index]);
					}
				}

				memcpy(this->LocalhostServerOptions.LocalhostServer, LocalhostServerName, this->LocalhostServerOptions.LocalhostServerLength + 1);
				delete[] Point;
				delete[] LocalhostServerName;
			}
		}
		else {
			continue;
		}
	}
	fclose(Input);
	delete[] Buffer;
	delete[] Target;
	delete[] LocalhostServer;

//Set default
	if (this->HopLimitOptions.HopLimitFluctuation < 0 && this->HopLimitOptions.HopLimitFluctuation >= 255)
		this->HopLimitOptions.HopLimitFluctuation = 2; //Default HopLimitFluctuation is 2
	if (ntohs(this->ICMPOptions.ICMPID) <= 0)
		this->ICMPOptions.ICMPID = htons((USHORT)GetCurrentProcessId()); //Default DNS ID is current process ID
	if (ntohs(this->ICMPOptions.ICMPSequence) <= 0)
		this->ICMPOptions.ICMPSequence = htons(0x0001); //Default DNS Sequence is 0x0001
	if (ntohs(this->DomainTestOptions.DomainTestID) <= 0)
		this->DomainTestOptions.DomainTestID = htons(0x0001); //Default Domain Test DNS ID is 0x0001
	if (this->DomainTestOptions.DomainTestSpeed <= 5000) //5s is least time between Domain Tests
		this->DomainTestOptions.DomainTestSpeed = 900000; //Default Domain Test request every 15 minutes.
	if (this->PaddingDataOptions.PaddingDataLength <= 0)
	{
		this->PaddingDataOptions.PaddingDataLength = sizeof(PaddingData);
		memcpy(this->PaddingDataOptions.PaddingData, PaddingData, sizeof(PaddingData) - 1); //Load default padding data from Microsoft Windows Ping
	}
	if (this->LocalhostServerOptions.LocalhostServerLength <= 0)
		this->LocalhostServerOptions.LocalhostServerLength = CharToDNSQuery((PSTR)LocalDNSName, this->LocalhostServerOptions.LocalhostServer); //Default Localhost DNS server name

//Check parameters
	if (!this->DNSTarget.IPv4 && !this->DNSTarget.IPv6 || !this->TCPMode && this->TCPOptions)
	{
		::PrintError(2, _T("Base rule(s) error"), NULL, NULL);
		return RETURN_ERROR;
	}

	return 0;
}

//Read Hosts file
SSIZE_T Configuration::ReadHosts()
{
//Read Hosts: ON/OFF
	if (Parameter.Hosts == 0)
		return 0;

//Initialization
	FILE *Input = nullptr;
	PSTR Buffer = nullptr, AddrTemp = nullptr;
	try {
		Buffer = new char[PACKET_MAXSIZE]();
		AddrTemp = new char[PACKET_MAXSIZE/8]();
	}
	catch (std::bad_alloc)
	{
		::PrintError(1, _T("Memory allocation failed"), NULL, NULL);

		delete[] Buffer;
		delete[] AddrTemp;
		TerminateService();
		return RETURN_ERROR;
	}
	memset(Buffer, 0, PACKET_MAXSIZE);
	memset(AddrTemp, 0, PACKET_MAXSIZE/8);
	std::wstring HostsFilePath(Path);
	HostsFilePath.append(_T("Hosts.ini"));

	size_t Encoding = 0, Line = 1, Index = 0, Front = 0, Rear = 0, Result = 0;
	std::string sBuffer, Domain;
	while (true)
	{
		_wfopen_s(&Input, HostsFilePath.c_str(), _T("r"));
		if (Input == nullptr)
		{
			::PrintError(3, _T("Cannot open hosts file(Hosts.ini)"), NULL, NULL);

			CleanupHostsTable();
			Sleep((DWORD)this->Hosts);
			continue;
		}
		
		Encoding = 0, Line = 1;
		while(!feof(Input))
		{
			memset(Buffer, 0, PACKET_MAXSIZE);
			fgets(Buffer, PACKET_MAXSIZE, Input);

		//Check encoding
			if (!ReadEncoding(Encoding, Buffer, sBuffer, PACKET_MAXSIZE))
				continue;
			memset(Buffer, 0, PACKET_MAXSIZE);
			memcpy(Buffer, sBuffer.c_str(), sBuffer.length());
			Line++;

		//Check spacing
			Index = 0, Front = 0, Rear = 0;
			if (sBuffer.find(32) != std::string::npos) //Space
			{
				Index = sBuffer.find(32);
				Front = Index;
				if (sBuffer.rfind(32) > Index)
					Rear = sBuffer.rfind(32);
				else 
					Rear = Front;
			}
			if (sBuffer.find(9) != std::string::npos) //HT
			{
				if (Index == 0)
					Index = sBuffer.find(9);
				if (Index > sBuffer.find(9))
					Front = sBuffer.find(9);
				else 
					Front = Index;
				if (sBuffer.rfind(9) > Index)
					Rear = sBuffer.rfind(9);
				else 
					Rear = Front;
			}

		//End

			Domain.clear();
			if (Front > 2) //Minimum length of IPv6 address
			{
				HostsTable Temp;
				size_t Vertical[THREAD_MAXNUM/4] = {0}, VerticalIndex = 0;
				bool MultiAddressesError = false;

			//Multiple Addresses
				for (Index = 0;Index < Front;Index++)
				{
					if (Buffer[Index] == 124)
					{
						if (VerticalIndex > THREAD_MAXNUM/4)
						{
							MultiAddressesError = true;
							::PrintError(3, _T("Too many Hosts IP addresses"), NULL, Line);
							break;
						}
						else if (Index - Vertical[VerticalIndex] < 2) //Minimum length of IPv6 address
						{
							MultiAddressesError = true;
							::PrintError(3, _T("Multiple addresses format error"), NULL, Line);
							break;
						}
						else {
							VerticalIndex++;
							Vertical[VerticalIndex] = Index + 1;
						}
					}
				}
				if (MultiAddressesError)
					continue;
				VerticalIndex++;
				Vertical[VerticalIndex] = Front + 1;
				Temp.ResponseNum = VerticalIndex;

			//End

				Index = 0;
				if (VerticalIndex > 0)
				{
					memset(AddrTemp, 0, PACKET_MAXSIZE/8);

				//Response initialization
					try {
						Temp.Response = new char[PACKET_MAXSIZE]();
					}
					catch (std::bad_alloc)
					{
						::PrintError(1, _T("Memory allocation failed"), NULL, NULL);

						delete[] Buffer;
						delete[] AddrTemp;
						CleanupHostsTable();
						TerminateService();
						return RETURN_ERROR;
					}
					memset(Temp.Response, 0, PACKET_MAXSIZE);

				//AAAA Records(IPv6)
					if (sBuffer.find(58) != std::string::npos)
					{
						Temp.Protocol = AF_INET6;
						while (Index < VerticalIndex)
						{
						//Make a response
							dns_aaaa_record rsp = {0};
							rsp.Name = htons(0xC00C); //Pointer of same request
							rsp.Classes = htons(Class_IN); //Class IN
							rsp.TTL = htonl(600); //10 minutes
							rsp.Type = htons(AAAA_Records);
							rsp.Length = htons(sizeof(in6_addr));

						//Convert addresses
							memcpy(AddrTemp, Buffer + Vertical[Index], Vertical[Index + 1] - Vertical[Index] - 1);
							Result = inet_pton(AF_INET6, AddrTemp, &rsp.Addr);
							if (Result == FALSE)
							{
								::PrintError(3, _T("Hosts IPv6 address format error"), NULL, Line);
								delete[] Temp.Response;

								Index++;
								continue;
							}
							else if (Result == RETURN_ERROR)
							{
								::PrintError(3, _T("Hosts IPv6 address convert failed"), WSAGetLastError(), Line);
								delete[] Temp.Response;

								Index++;
								continue;
							}
							memcpy(Temp.Response + Temp.ResponseLength, &rsp, sizeof(dns_aaaa_record));
						
						//Check length
							Temp.ResponseLength += sizeof(dns_aaaa_record);
							if (Temp.ResponseLength >= PACKET_MAXSIZE)
							{
								::PrintError(3, _T("Too many Hosts IP addresses"), NULL, Line);
								delete[] Temp.Response;

								Index++;
								continue;
							}

							memset(AddrTemp, 0, PACKET_MAXSIZE/8);
							Index++;
						}
					}
				//A Records(IPv4)
					else {
						Temp.Protocol = AF_INET;
						while (Index < VerticalIndex)
						{
						//Make a response
							dns_a_record rsp = {0};
							rsp.Name = htons(0xC00C); //Pointer of same request
							rsp.Classes = htons(Class_IN); //Class IN
							rsp.TTL = htonl(600); //10 minutes
							rsp.Type = htons(A_Records);
							rsp.Length = htons(sizeof(in_addr));

						//Convert addresses
							memcpy(AddrTemp, Buffer + Vertical[Index], Vertical[Index + 1] - Vertical[Index] - 1);
							Result = inet_pton(AF_INET, AddrTemp, &rsp.Addr);
							if (Result == FALSE)
							{
								::PrintError(3, _T("Hosts IPv4 address format error"), NULL, Line);
								
								delete[] Temp.Response;
								Index++;
								continue;
							}
							else if (Result == RETURN_ERROR)
							{
								::PrintError(3, _T("Hosts IPv4 address convert failed"), WSAGetLastError(), Line);

								delete[] Temp.Response;
								Index++;
								continue;
							}
							memcpy(Temp.Response + Temp.ResponseLength, &rsp, sizeof(dns_a_record));

						//Check length
							Temp.ResponseLength += sizeof(dns_a_record);
							if (Temp.ResponseLength >= PACKET_MAXSIZE)
							{
								::PrintError(3, _T("Too many Hosts IP addresses"), NULL, Line);

								delete[] Temp.Response;
								Index++;
								continue;
							}

							memset(AddrTemp, 0, PACKET_MAXSIZE/8);
							Index++;
						}
					}

				//Sign patterns
					Domain.append(sBuffer, Rear + 1, sBuffer.length() - Rear);
					try {
						std::regex TempPattern(Domain, std::regex_constants::extended);
						Temp.Pattern = TempPattern;
					}
					catch(std::regex_error)
					{
						::PrintError(3, _T("Regular expression pattern error"), NULL, Line);

						delete[] Temp.Response;
						continue;
					}

				//Add to global HostsTable
					if (Temp.ResponseLength > sizeof(dns_hdr) + sizeof(dns_qry))
						Modificating->push_back(Temp);
				}
			}
		}
		fclose(Input);

	//Update Hosts list
		if (!Modificating->empty())
		{
			Using->swap(*Modificating);
			for (std::vector<HostsTable>::iterator iter = Modificating->begin();iter != Modificating->end();iter++)
				delete[] iter->Response;
			Modificating->clear();
			Modificating->resize(0);
		}
		else { //Hosts Table is empty
			CleanupHostsTable();
		}
		
	//Auto-refresh
		Sleep((DWORD)this->Hosts);
	}

	delete[] Buffer;
	delete[] AddrTemp;
	return 0;
}

//Read encoding from file
bool __stdcall ReadEncoding(size_t &Encoding, PSTR Buffer, std::string &sBuffer, const size_t Length)
{
	bool BOM = false; //Byte Order Mark/BOM
	sBuffer.clear();

	if (Encoding == 0)
	{
	//8-bit Unicode Transformation Format/UTF-8 with BOM
		if (Buffer[0] == 0xFFFFFFEF && Buffer[1] == 0xFFFFFFBB && Buffer[2] == 0xFFFFFFBF)
		{
			BOM = true;
			Encoding = UTF_8;
		}
	//32-bit Unicode Transformation Format/UTF-32 Little Endian/LE
		else if (Buffer[0] == 0xFFFFFFFF && Buffer[1] == 0xFFFFFFFE && Buffer[2] == 0 && Buffer[3] == 0)
		{
			BOM = true;
			Encoding = UTF_32_LE;
		}
	//32-bit Unicode Transformation Format/UTF-32 Big Endian/BE
		else if (Buffer[0] == 0 && Buffer[1] == 0 && Buffer[2] == 0xFFFFFFFE && Buffer[3] == 0xFFFFFFFF)
		{
			BOM = true;
			Encoding = UTF_32_BE;
		}
	//16-bit Unicode Transformation Format/UTF-16 Little Endian/LE
		else if (Buffer[0] == 0xFFFFFFFF && Buffer[1] == 0xFFFFFFFE)
		{
			BOM = true;
			Encoding = UTF_16_LE;
		}
	//16-bit Unicode Transformation Format/UTF-16 Big Endian/BE
		else if (Buffer[0] == 0xFFFFFFFE && Buffer[1] == 0xFFFFFFFF)
		{
			BOM = true;
			Encoding = UTF_16_BE;
		}
	}

	size_t Sign = 0;
//UTF-8 and Microsoft Windows ANSI codepages
	if (Encoding == 0 || Encoding == UTF_8)
	{
		if (BOM)
		{
			for (Sign = 3;Sign < strlen(Buffer);Sign++) //Length of UTF-8 BOM is 3(EF BB BF)
			{
				if (Buffer[Sign] >> 8 != 0) //Non-ASCII
					return false;
				else if (Buffer[Sign] == 0x0A || Buffer[Sign] == 0x0D) //New line(CR/Carriage Return or LF/Line Feed)
					Buffer[Sign] = 0;
			}
			sBuffer.append(Buffer + 3);
		}
		else {
			for (Sign = 0;Sign < strlen(Buffer);Sign++)
			{
				if (Buffer[Sign] >> 8 != 0) //Non-ASCII
					return false;
				else if (Buffer[Sign] == 0x0A || Buffer[Sign] == 0x0D) //New line(CR/Carriage Return or LF/Line Feed)
					Buffer[Sign] = 0;
			}
			sBuffer.append(Buffer);
		}
	}

//UTF-16
	if (Encoding == UTF_16_LE || Encoding == UTF_16_BE)
	{
		if (BOM)
			Sign = 2; //Length of UTF-16 BOM is 2(FF FE/FE FF)
		else 
			Sign = 1;

		while (Sign < Length + 1)
		{
			if (Buffer[Sign] == 0 && Buffer[Sign + 1] == 0 || 
				Buffer[Sign] == 10 || Buffer[Sign] == 13) //New line(CR/Carriage Return or LF/Line Feed)
					break;

			if (Buffer[Sign] != 0)
			{
				if (Encoding == UTF_16_LE)
				{
					if (Buffer[Sign + 1] == 0 && Buffer[Sign] >> 8 == 0) //ASCII
						sBuffer.append(sizeof(UCHAR), Buffer[Sign]);
					else
						return false;
				}
				else { //UTF_16_BE
					if (Buffer[Sign - 1] == 0 && Buffer[Sign] >> 8 == 0) //ASCII
						sBuffer.append(sizeof(UCHAR), Buffer[Sign]);
					else
						return false;
				}
			}

			Sign++;
		}
	}

//UTF-32
	if (Encoding == UTF_32_LE || Encoding == UTF_32_BE)
	{
		if (BOM)
			Sign = 10; //Length of UTF-32 BOM is 4(FF FE 00 00/00 00 FE FF)
		else 
			Sign = 6;

		while (Sign < Length)
		{
			if (Buffer[Sign - 3] == 0 && Buffer[Sign - 2] == 0 && Buffer[Sign - 1] == 0 && Buffer[Sign] == 0)
				break;

			if (Buffer[Sign - 3] != 0)
			{
				if (Buffer[Sign - 3] >> 8 != 0) //Non-ASCII
					return false;
				else if (Buffer[Sign - 3] == 0x0A || Buffer[Sign - 3] == 0x0D) //New line(CR/Carriage Return or LF/Line Feed)
					Buffer[Sign - 3] = 0;
				else 
					sBuffer.append(sizeof(UCHAR), Buffer[Sign - 3]);
			}

			Sign++;
		}
	}

	if (sBuffer.empty())
		return false;
	else 
		return true;
}

//Clean Hosts Table
inline void __stdcall CleanupHostsTable()
{
	for (std::vector<HostsTable>::iterator iter = Modificating->begin();iter != Modificating->end();iter++)
		delete[] iter->Response;
	for (std::vector<HostsTable>::iterator iter = Using->begin();iter != Using->end();iter++)
		delete[] iter->Response;
	Modificating->clear();
	Modificating->resize(0);
	Using->clear();
	Using->resize(0);
	return;
}
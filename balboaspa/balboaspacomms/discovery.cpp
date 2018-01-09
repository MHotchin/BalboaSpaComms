
#include "stdafx.h"

#include "Discovery.h"


//  Spa listens on DiscoveryPort for a message.  Responds with "BWGSPA" and MAC address.
//  Once you get the response, connect to port 4257 to control the spa.
const u_short usDiscoveryPort = 30303;
const char *szDiscoveryMessage = "Discovery: Who is out there?";
const char *szSignature = "BWGSPA         \r\n00-15-27-";

CSpaAddress::CSpaAddress(
	const CSpaAddress &Source)
{
	m_SpaAddress = Source.m_SpaAddress;
	m_strMACAddress = Source.m_strMACAddress;
}

CSpaAddress::CSpaAddress(
	const sockaddr_in &SpaAddress,
	const string &szMACAddress)
	: m_SpaAddress(SpaAddress), m_strMACAddress(szMACAddress)
{}

BOOL CSpaAddress::operator==(const CSpaAddress &Other)
{
	return (m_strMACAddress == Other.m_strMACAddress) &&
		(m_SpaAddress.sin_addr.S_un.S_addr == Other.m_SpaAddress.sin_addr.S_un.S_addr) &&
		(m_SpaAddress.sin_port == Other.m_SpaAddress.sin_port);
}

BOOL DiscoverSpas(SpaAddressVector &Spas)
{
	int iResult;

	Spas.clear();

	SOCKET ConnectSocket  = socket(AF_INET, SOCK_DGRAM,	IPPROTO_UDP);

	if (ConnectSocket == INVALID_SOCKET)
	{
		_RPTWN(_CRT_WARN, L"Error at socket(): %ld\n", WSAGetLastError());
		return FALSE;
	}

	DWORD fBroadcast = TRUE;
	iResult = setsockopt(ConnectSocket, SOL_SOCKET, SO_BROADCAST, (const char *)&fBroadcast, sizeof(fBroadcast));

	if (iResult == SOCKET_ERROR)
	{
		_RPTWN(_CRT_WARN, L"setsockopt failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}

	sockaddr_in RecvAddr;

	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(30303);
	iResult = InetPton(AF_INET, L"255.255.255.255", &RecvAddr.sin_addr.s_addr);

	if (iResult <= 0)
	{
		_RPTWN(_CRT_WARN, L"InetPton failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}

	iResult = sendto(ConnectSocket, szDiscoveryMessage, (int)strlen(szDiscoveryMessage) + 1, 0, (SOCKADDR *)& RecvAddr, sizeof(RecvAddr));

	if (iResult == SOCKET_ERROR)
	{
		_RPTWN(_CRT_WARN, L"sendto failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		return FALSE;
	}

	//  If there are multiple spas, we can get more than one response.
	fd_set fsIncoming;
	timeval tvTimeout;

	FD_ZERO(&fsIncoming);
	FD_SET(ConnectSocket, &fsIncoming);

	tvTimeout.tv_sec = 1;
	tvTimeout.tv_usec = 0;

	do
	{
		iResult = select(0, &fsIncoming, NULL, NULL, &tvTimeout);
		tvTimeout.tv_sec = 1;

		if (iResult == SOCKET_ERROR)
		{
			closesocket(ConnectSocket);

			return FALSE;
		}

		if (iResult > 0)
		{
			char RecvBuffer[1024];
			sockaddr_in saFrom;
			int saSize = sizeof(saFrom);

			memset(RecvBuffer, 0, sizeof(RecvBuffer));

			iResult = recvfrom(ConnectSocket, RecvBuffer, sizeof(RecvBuffer), 0, (sockaddr *)&saFrom, &saSize);
			if (iResult == SOCKET_ERROR)
			{
				closesocket(ConnectSocket);
				return FALSE;
			}

			if (strncmp(RecvBuffer, szSignature, strlen(szSignature)) != 0)
			{
				_RPTWN(_CRT_WARN, L"Unexpected Response: %S", RecvBuffer);
			}
			else
			{
				CSpaAddress NewSpa(saFrom, string(&RecvBuffer[17], strlen(&RecvBuffer[16]) - 3));

				Spas.push_back(NewSpa);
			}
		}
	} while (iResult > 0);

	closesocket(ConnectSocket);
	return TRUE;
}


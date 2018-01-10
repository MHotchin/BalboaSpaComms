
#include "stdafx.h"


#include "Discovery.h"
#include "MonitorCallback.h"
#include "SpaComms.h"
#include "Debug.h"

typedef uint8_t crc;


#include "crc.h"

const u_short usConnectionPort = 4257;

CSpaComms::CSpaComms(
	const CSpaAddress &SpaAddress,
	IMonitorCallback *pCallback,
	BOOL fCoalesce)
	: m_SpaAddress(SpaAddress), m_hMonitorThread(0), m_fShutDown(FALSE),
	m_SpaSocket(INVALID_SOCKET), m_fCoalesce(fCoalesce),
	m_PreviousStatusMessage(64), m_pCallback(pCallback)
{
	F_CRC_InicializaTabla();
}

CSpaComms::~CSpaComms()
{
	EndMonitor();

	if (m_SpaSocket != INVALID_SOCKET)
	{
		closesocket(m_SpaSocket);
	}

	if (m_pCallback != NULL)
	{
		m_pCallback->Dispose();
		m_pCallback = NULL;
	}

}

BOOL CSpaComms::StartMonitor(void)
{
	if (m_hMonitorThread != 0)
	{
		return FALSE;
	}

	m_SpaSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKET iResult = INVALID_SOCKET;

	if (m_SpaSocket == INVALID_SOCKET)
	{
		return FALSE;
	}

	sockaddr_in SpaAddressPort = m_SpaAddress.m_SpaAddress;
	SpaAddressPort.sin_port = htons(usConnectionPort);

	iResult = connect(m_SpaSocket, (const sockaddr *)&SpaAddressPort, sizeof(SpaAddressPort));

	if (iResult == INVALID_SOCKET)
	{
		return FALSE;
	}
	uintptr_t hThread = _beginthreadex(NULL, 0, CSpaComms::MonitorThreadProc, this, 0, NULL);

	if (hThread != 0)
	{
		m_hMonitorThread = (HANDLE)hThread;
	}

	return (hThread != 0);
}


void CSpaComms::EndMonitor()
{
	m_fShutDown = TRUE;
	if (m_hMonitorThread != 0)
	{
		WaitForSingleObject(m_hMonitorThread, INFINITE);
		CloseHandle(m_hMonitorThread);
		m_hMonitorThread = 0;
	}
}

unsigned int __stdcall
CSpaComms::MonitorThreadProc(
	void *pParam)
{
	return ((CSpaComms *)pParam)->MonitorThreadProc();
}


//Each message requires MessageTerminators, MessageLength, MessageId (3 bytes), CrcByte
//  MT ML MI MI MI ... CB MT
const UINT cMessageOverhead = 7;
const BYTE byMessageTerminator = 0x7e;

#ifdef _DEBUG
//  Force a small size to exercize buffer stitching code
const size_t uiRecvBufferSize = 15;
#else
const size_t uiRecvBufferSize = 64;
#endif

unsigned int
CSpaComms::MonitorThreadProc()
{
	fd_set fsIncoming;
	timeval tvTimeout;

	FD_ZERO(&fsIncoming);
	FD_SET(m_SpaSocket, &fsIncoming);

	tvTimeout.tv_sec = 1;
	tvTimeout.tv_usec = 0;

	CByteArray LeftOvers;
	LeftOvers.reserve(1024);

	while (!m_fShutDown)
	{
		CByteArray RecvBuffer(uiRecvBufferSize);

		int iResult = select(0, &fsIncoming, NULL, NULL, &tvTimeout);

		if (iResult == SOCKET_ERROR)
		{}

		if (iResult > 0)
		{
			RecvBuffer.resize(uiRecvBufferSize);

			iResult = recv(m_SpaSocket, (char *)&*RecvBuffer.begin(), (int)RecvBuffer.size(), 0);

			if (iResult == SOCKET_ERROR)
			{
				//  ??
			}
			else
			{
				RecvBuffer.resize(iResult);

				//  Add our current input to whatever was leftover from last-time.
				LeftOvers.insert(LeftOvers.end(), RecvBuffer.begin(), RecvBuffer.end());

				auto pByte = LeftOvers.cbegin();

				// Locate beginning of message.  We expect it to be at the begining of the buffer,
				// but let's make sure, shall we?
				_ASSERT(*pByte == byMessageTerminator);

				while (pByte != LeftOvers.cend() && *pByte != byMessageTerminator)
				{
					pByte++;
				}

				if (pByte != LeftOvers.cbegin())
				{
					LeftOvers.erase(LeftOvers.cbegin(), pByte);
				}

				//  May have multiple messages now in the buffer.
				while (LeftOvers.size() >= cMessageOverhead)
				{
					pByte = LeftOvers.cbegin();
					pByte++;

					//  Get size of payload
					BYTE bySize = *pByte;

					if (LeftOvers.cend() - pByte > bySize)
					{
						//  Skip over the payload.
						pByte += bySize;

						_ASSERT(*pByte == byMessageTerminator);

						//  Extract complete message, remove from 'LeftOvers', process.
						CByteArray Message(LeftOvers.cbegin(), pByte + 1);

						ProcessMessage(Message);
						LeftOvers.erase(LeftOvers.cbegin(), pByte + 1);
					}
					else
					{
						//  Buffer has one incomplete message.  Wait for more input.
						break;
					}
				}
			}
		}
	}

	return 0;
}


enum SpaResponseMessageIDs
{
	msStatus = 0xffaf13,
	msConfigResponse = 0x0abf94,
	msFilterConfig = 0x0abf23,
	msControlConfig = 0x0abf24,
	msControlConfig2 = 0x0abf2e,
	msSetTempRange = 0xffaf26
};

const UINT uiPayloadStartOffset = 5;

void
CSpaComms::ProcessMessage(
	const CByteArray &Message)
{
	_ASSERT(Message[0] == byMessageTerminator);
	_ASSERT(Message[Message.size() - 1] == byMessageTerminator);

	UINT uiSize = Message[1];

	//  Message should be the payload + 2 terminators
	_ASSERT(uiSize == Message.size() - 2);

	//  CRC is appended, so don't include that byte when re-calculating.
	crc MessageCRC = F_CRC_CalculaCheckSum(&Message[1], uiSize - 1);


	//  Sometimes fails?
	_ASSERT(MessageCRC == Message[Message.size() - 2]);

	UINT uiMessageID = (Message[2] << 16) + (Message[3] << 8) + Message[4];

	switch (uiMessageID)
	{

	case msConfigResponse:
		if (Message.size() == 32)
		{
			ConfigResponseMessage ConfigResponseMessage;

			ConfigResponseMessage.m_RawMessage = Message;

			char szMacAddress[64];

			sprintf_s(szMacAddress, "%02X-%02X-%02X-%02X-%02X-%02X",
					  Message[uiPayloadStartOffset + 3], Message[uiPayloadStartOffset + 4],
					  Message[uiPayloadStartOffset + 5], Message[uiPayloadStartOffset + 6],
					  Message[uiPayloadStartOffset + 7], Message[uiPayloadStartOffset + 8]);

			ConfigResponseMessage.m_strMACAddress = szMacAddress;
			m_pCallback->ProcessConfigResponse(ConfigResponseMessage);
		}
		else
		{
			m_pCallback->ProcessUnknownMessageRaw(Message);
		}

		break;

	case msFilterConfig:
		if (Message.size() == 15)
		{
			FilterConfigResponseMessage FilterConfigResponse;

			FilterConfigResponse.m_RawMessage = Message;

			FilterConfigResponse.m_Filter1StartTime.m_Hour = Message[uiPayloadStartOffset + 0];
			FilterConfigResponse.m_Filter1StartTime.m_Minute = Message[uiPayloadStartOffset + 1];
			FilterConfigResponse.m_uiFilter1Duration =
				Message[uiPayloadStartOffset + 2] * 60 + Message[uiPayloadStartOffset + 3];

			FilterConfigResponse.m_fFilter2Enabled = (Message[uiPayloadStartOffset + 4] & 0x80) != 0;
			FilterConfigResponse.m_Filter2StartTime.m_Hour = Message[uiPayloadStartOffset + 4] & 0x7f;
			FilterConfigResponse.m_Filter2StartTime.m_Minute = Message[uiPayloadStartOffset + 5];
			FilterConfigResponse.m_uiFilter2Duration =
				Message[uiPayloadStartOffset + 6] * 60 + Message[uiPayloadStartOffset + 7];

			m_pCallback->ProcessFilterConfigResponse(FilterConfigResponse);
		}
		else
		{
			m_pCallback->ProcessUnknownMessageRaw(Message);
		}

		break;

	case msControlConfig:
		if (Message.size() == 28)
		{
			VersionInfoResponseMessage VersionInfoResponse;

			VersionInfoResponse.m_RawMessage = Message;

			VersionInfoResponse.m_strModelName = string((const char *)&Message[uiPayloadStartOffset + 4], 8);
			VersionInfoResponse.m_strModelName.erase(VersionInfoResponse.m_strModelName.find_last_not_of(" ") + 1);
			VersionInfoResponse.SoftwareID[0] = Message[uiPayloadStartOffset + 0];
			VersionInfoResponse.SoftwareID[1] = Message[uiPayloadStartOffset + 1];
			VersionInfoResponse.SoftwareID[2] = Message[uiPayloadStartOffset + 2];
			VersionInfoResponse.CurrentSetup = Message[uiPayloadStartOffset + 12];
			VersionInfoResponse.ConfigurationSignature =
				(Message[uiPayloadStartOffset + 13] << 24) +
				(Message[uiPayloadStartOffset + 14] << 16) +
				(Message[uiPayloadStartOffset + 15] << 8) +
				(Message[uiPayloadStartOffset + 16]);

			m_pCallback->ProcessVersionInfoResponse(VersionInfoResponse);
		}
		else
		{
			m_pCallback->ProcessUnknownMessageRaw(Message);
		}

		break;

	case msControlConfig2:
		if (Message.size() == 13)
		{
			ControlConfig2ResponseMessage ControlConfig2ResponseMessage;

			ControlConfig2ResponseMessage.m_RawMessage = Message;

			m_pCallback->ProcessControlConfig2Response(ControlConfig2ResponseMessage);
		}
		else
		{
			m_pCallback->ProcessUnknownMessageRaw(Message);
		}
		break;

	//case msSetTempRange:
	//	//  Unknown contents
	//	break;


	case msStatus:
		if (Message.size() == 31)
		{
			if (!m_fCoalesce || (Message != m_PreviousStatusMessage))
			{
				if (m_fCoalesce)
				{
					m_PreviousStatusMessage = Message;
				}

				StatusMessage StatusMessage;

				StatusMessage.m_RawMessage = Message;

				StatusMessage.m_Time.m_Hour = Message[8];
				StatusMessage.m_Time.m_Minute = Message[9];
				StatusMessage.m_f24Time = ((Message[14] & 0x02) != 0);

				StatusMessage.m_CurrentTemp = Message[7];
				StatusMessage.m_SetPointTemp = Message[25];
				StatusMessage.m_TempScale = (Message[14] & 0x01) ? tsCelsiusX2 : tsFahrenheight;

				StatusMessage.m_HeatRange = (Message[15] & 0x04) ? hrHigh : hrLow;
				StatusMessage.m_HeatingMode = static_cast<HeatingMode>(Message[10] & 0x03);

				StatusMessage.m_Pump1Status = static_cast<PumpStatus>(Message[16] & 0x03);
				StatusMessage.m_Pump2Status = static_cast<PumpStatus>((Message[16] >> 2) & 0x03);

				StatusMessage.m_fPriming = ((Message[6] & 0x01) != 0);
				StatusMessage.m_fHeating = ((Message[15] & 0x30) != 0);
				StatusMessage.m_fCircPumpRunning = ((Message[18] & 0x02) != 0);
				StatusMessage.m_fLights = ((Message[19] & 0x03) != 0);

				m_pCallback->ProcessStatusMessage(StatusMessage);
			}
		}
		else
		{
			m_pCallback->ProcessUnknownMessageRaw(Message);
		}

		break;

	default:
		m_pCallback->ProcessUnknownMessageRaw(Message);
	}
}


BOOL
CSpaComms::SendSpaMessage(
	const CByteArray &Message)
{
	_ASSERT(Message[0] == byMessageTerminator);
	_ASSERT(Message[Message.size() - 1] == byMessageTerminator);
	_ASSERT(Message[1] == Message.size() - 2);

	int iResult = 0;

	iResult = send(m_SpaSocket, (const char *)&Message[0], (int)Message.size(), 0);

	return (iResult == Message.size());
}


enum SpaCommandMessageID
{
	msConfigRequest = 0x0abf04,
	msFilterConfigRequest = 0x0abf22,
	msToggleItemRequest = 0x0abf11,
	msSetTempRequest = 0x0abf20,
	msSetTempScaleRequest = 0x0abf27,
	msSetTimeRequest = 0x0abf21,
	msSetWiFiSettingsRequest = 0x0abf92,
	msControlConfigRequest = 0x0abf22
};



void
FillInMessageOverhead(
	CByteArray &Message,
	SpaCommandMessageID ID,
	UINT uiPayloadLength)
{
	Message.resize(cMessageOverhead + uiPayloadLength);

	Message[0] = byMessageTerminator;
	Message[Message.size() - 1] = byMessageTerminator;

	Message[1] = uiPayloadLength + cMessageOverhead - 2;
	Message[2] = (ID >> 16) & 0xff;
	Message[3] = (ID >> 8) & 0xff;
	Message[4] = (ID) & 0xff;
}


void
FillInMessageCRC(
	CByteArray &Message)
{
	_ASSERT(Message.size() >= cMessageOverhead);
	_ASSERT(Message[1] == Message.size() - 2);

	crc MessageCRC = F_CRC_CalculaCheckSum(&Message[1], (UINT)(Message.size() - 3));
	Message[Message.size() - 2] = MessageCRC;
}

BOOL
CSpaComms::SendConfigRequest(void)
{
	CByteArray ConfigRequestMessage;

	FillInMessageOverhead(ConfigRequestMessage, msConfigRequest, 0);

	FillInMessageCRC(ConfigRequestMessage);

	return SendSpaMessage(ConfigRequestMessage);
}


BOOL
CSpaComms::SendFilterConfigRequest(void)
{
	CByteArray FilterConfigRequestMessage;

	FillInMessageOverhead(FilterConfigRequestMessage, msFilterConfigRequest, 3);
	FilterConfigRequestMessage[uiPayloadStartOffset] = 0x01;
	FillInMessageCRC(FilterConfigRequestMessage);

	return SendSpaMessage(FilterConfigRequestMessage);
}


BOOL
CSpaComms::SendToggleRequest(
	ToggleSpaItem tsi)
{
	CByteArray ToggleSpaItemRequestMessage;

	FillInMessageOverhead(ToggleSpaItemRequestMessage, msToggleItemRequest, 2);
	ToggleSpaItemRequestMessage[uiPayloadStartOffset] = tsi;
	FillInMessageCRC(ToggleSpaItemRequestMessage);

	return SendSpaMessage(ToggleSpaItemRequestMessage);
}

BOOL
CSpaComms::SendVerInfoRequest(void)
{
	CByteArray VerInfoRequestMessage;

	FillInMessageOverhead(VerInfoRequestMessage, msFilterConfigRequest, 3);
	VerInfoRequestMessage[uiPayloadStartOffset] = 0x02;
	FillInMessageCRC(VerInfoRequestMessage);

	return SendSpaMessage(VerInfoRequestMessage);
}

BOOL CSpaComms::SendControlConfig2Request(void)
{
	CByteArray ControlConfig2RequestMessage;

	FillInMessageOverhead(ControlConfig2RequestMessage, msControlConfigRequest, 3);
	ControlConfig2RequestMessage[uiPayloadStartOffset + 2] = 0x01;
	FillInMessageCRC(ControlConfig2RequestMessage);

	return SendSpaMessage(ControlConfig2RequestMessage);
}

BOOL CSpaComms::SendSetTempRequest(
	UINT uiTemp,
	TempScale ts)
{
	CByteArray SetTempRequestMessage;

	FillInMessageOverhead(SetTempRequestMessage, msSetTempRequest, 1);
	SetTempRequestMessage[uiPayloadStartOffset] = uiTemp;
	FillInMessageCRC(SetTempRequestMessage);


	return SendSetTempScaleRequest(ts) && SendSpaMessage(SetTempRequestMessage);
}

BOOL CSpaComms::SendSetTempScaleRequest(
	TempScale ts)
{
	CByteArray SetTempScaleRequestMessage;

	FillInMessageOverhead(SetTempScaleRequestMessage, msSetTempScaleRequest, 2);
	SetTempScaleRequestMessage[uiPayloadStartOffset + 0] = 0x01;
	SetTempScaleRequestMessage[uiPayloadStartOffset + 1] = ts;
	FillInMessageCRC(SetTempScaleRequestMessage);

	return SendSpaMessage(SetTempScaleRequestMessage);


	return 0;
}


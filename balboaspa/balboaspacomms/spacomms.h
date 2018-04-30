#pragma once


class CSpaComms
{
public:
	CSpaComms(const CSpaAddress &, IMonitorCallback *, 
			  BOOL fCoalesce = TRUE);
	~CSpaComms();
	BOOL StartMonitor(void);
	void EndMonitor(void);

	enum ToggleSpaItem
	{
		tsiPump1 = 0x04,
		tsiPump2 = 0x05,
		tsiLights = 0x11,
		tsiHeatMode = 0x51,
		tsiTempRange = 0x50
	};

	BOOL SendConfigRequest(void);
	BOOL SendFilterConfigRequest(void);
	BOOL SendToggleRequest(ToggleSpaItem);
	BOOL SendVerInfoRequest(void);
	BOOL SendControlConfig2Request(void);
	BOOL SendSetTempRequest(UINT, TempScale);
	BOOL SendSetTempScaleRequest(TempScale);
	BOOL SendSetFilterConfigRequest(const FilterConfigResponseMessage &);
	
private:

	struct sPrivateData;

	std::unique_ptr<sPrivateData> m_pData;

	static  unsigned int __stdcall MonitorThreadProc(void *);
	unsigned int MonitorThreadProc(void);

	void ProcessMessage(const CByteArray &);
	BOOL SendSpaMessage(const CByteArray &);

	CSpaAddress m_SpaAddress;
	HANDLE m_hMonitorThread;
	BOOL m_fShutDown;
	//SOCKET m_SpaSocket;
	BOOL m_fCoalesce;
	CByteArray m_PreviousStatusMessage;

	IMonitorCallback *m_pCallback;

	//  Disallowed operations.
	const CSpaComms & operator=(const CSpaComms &) { return *this; };
};

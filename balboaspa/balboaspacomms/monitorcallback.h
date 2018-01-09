#pragma once





struct SpaTime
{
	BYTE m_Hour;
	BYTE m_Minute;
};


enum TempScale
{
	tsFahrenheight,
	tsCelsiusX2
};
const BYTE byUNKNOWN_TEMP = 0xff;

enum HeatingMode
{
	hmReady = 0,
	hmRest = 1,
	hmReadyInRest = 3
};

enum HeatingRange
{
	hrLow = 0,
	hrHigh = 1
};



enum PumpStatus
{
	psOff = 0,
	psLow = 1,
	psHigh = 2
};

struct RawResponseMessage
{
	CByteArray m_RawMessage;
};


struct StatusMessage : public RawResponseMessage
{
	SpaTime m_Time;
	BOOL m_f24Time;

	BYTE m_CurrentTemp;
	BYTE m_SetPointTemp;
	TempScale m_TempScale;

	HeatingRange m_HeatRange;
	HeatingMode m_HeatingMode;

	PumpStatus m_Pump1Status;
	PumpStatus m_Pump2Status;

	BOOL m_fPriming;
	BOOL m_fHeating;
	BOOL m_fCircPumpRunning;
	BOOL m_fLights;
};


struct ConfigResponseMessage : public RawResponseMessage
{
	string m_strMACAddress;
};

struct ControlConfig2ResponseMessage : public RawResponseMessage
{
	// Unknown contents
};


struct FilterConfigResponseMessage : public RawResponseMessage
{
	SpaTime m_Filter1StartTime;
	UINT m_uiFilter1Duration;

	BOOL m_fFilter2Enabled;
	SpaTime m_Filter2StartTime;
	UINT m_uiFilter2Duration;
};

struct VersionInfoResponseMessage : public RawResponseMessage
{
	string m_strModelName;
	BYTE SoftwareID[3];
	BYTE CurrentSetup;
	DWORD ConfigurationSignature;
};

struct SetTempRangeResponseMessage: public RawResponseMessage
{
	// Unknown contents
};

class IMonitorCallback
{
public:
	virtual void ProcessStatusMessage(const StatusMessage &) {};
	virtual void ProcessConfigResponse(const ConfigResponseMessage &) {};
	virtual void ProcessFilterConfigResponse(const FilterConfigResponseMessage &) {};
	virtual void ProcessVersionInfoResponse(const VersionInfoResponseMessage &) {};
	virtual void ProcessControlConfig2Response(const ControlConfig2ResponseMessage &) {};
	virtual void ProcessSetTempRangeResponse(const SetTempRangeResponseMessage &) {};
	virtual void ProcessUnknownMessageRaw(const CByteArray &) {};

	virtual void Dispose(void) = 0;
	virtual void OnFatalError(void) {};

private:
};


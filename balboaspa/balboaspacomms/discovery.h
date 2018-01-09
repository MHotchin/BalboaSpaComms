#pragma once


class CSpaAddress
{
public:
	CSpaAddress(const CSpaAddress&);
	CSpaAddress(const sockaddr_in &, const string &);
	BOOL operator==(const CSpaAddress &);


	sockaddr_in m_SpaAddress;
	string m_strMACAddress;
};


typedef std::vector<CSpaAddress> SpaAddressVector;

BOOL DiscoverSpas(SpaAddressVector &Spas);

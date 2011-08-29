#ifndef BILL_H_
#define BILL_H_

#include <string>
#include <sstream>

using namespace std;

namespace MPOST
{


class CBill
{
	friend class CAcceptor;
	
public:
	CBill()
	{
        _country		= "***";
        _value          = 0.0;
        _type           = '*';
        _series         = '*';
        _compatibility  = '*';
        _version        = '*';
	}
	
	CBill(string country, double value, char type, char series, char compatibility, char version)
	{
        _country		= country;
        _value          = value;
        _type           = type;
        _series         = series;
        _compatibility  = compatibility;
        _version        = version;
	}
	
	string ToString()
	{
		stringstream ss;

		ss << _country << " " << _value << " " << _series << " " << _type << " " << _compatibility << " " << _version;

		return ss.str();
	}
	
	string GetCountry()
	{
		return _country;
	}
	
	double GetValue()
	{
		return _value;
	}
	
	char GetSeries()
	{
		return _series;
	}
	
	char GetType()
	{
		return _type;
	}
	
	char GetCompatibility()
	{
		return _compatibility;
	}
	
	char GetVersion()
	{
		return _version;
	}
	
private:
    string _country;
    double _value;
    char _series;
    char _type;
    char _compatibility;
    char _version;

};


}

#endif /*BILL_H_*/

#ifndef COUPON_H_
#define COUPON_H_

namespace MPOST
{


class CCoupon
{
	friend class CAcceptor;
	
private:
    int _ownerID;
    double _value;
};


}

#endif /*COUPON_H_*/

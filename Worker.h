#ifndef WORKER_H_
#define WORKER_H_

#include "DataLinkLayer.h"

namespace MPOST
{

class CAcceptor;


class CWorker
{
public:

    CWorker(CAcceptor* acceptor)
    {
        _acceptor = acceptor;
    }

    static void* MessageLoopThread(void* param);
    
    
    void WaitForQuiet()
    {
        _dataLinkLayer->WaitForQuiet();
    }




private:

	static const int CmdOmnibus = 0x10;

    CAcceptor* _acceptor;
    CDataLinkLayer* _dataLinkLayer;
};


}


#endif /*WORKER_H_*/

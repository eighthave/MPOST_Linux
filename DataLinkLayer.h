#ifndef DATALINKLAYER_H_
#define DATALINKLAYER_H_

#include "Acceptor.h"
#include <vector>

using namespace std;

namespace MPOST
{

class CDataLinkLayer
{
public:
	CDataLinkLayer(CAcceptor* acceptor);

    void SendPacket(char payload[], int payloadLength);

    char ComputeCheckSum(vector<char>);

	vector<char>* ReceiveReply();

	void WaitForQuiet();

	bool ReplyAcked(vector<char>* reply);

    void LogCommandAndReply(vector<char>& command, vector<char>& reply, bool wasEchoDiscarded);

	void FlushIdenticalTransactionsToLog();


	private:
		CAcceptor* _acceptor;

    static const char STX;
    static const char ETX;

    char _ackToggleBit;
    char _nakCount;

    static const char ACKMask;

    vector<char> _currentCommand;
	vector<char> _echoDetect;
    vector<char> _previousCommand;
    vector<char> _previousReply;
    int _identicalCommandAndReplyCount;
};

}

#endif /*DATALINKLAYER_H_*/

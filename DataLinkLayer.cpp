#include "DataLinkLayer.h"
#include "Acceptor.h"

#include <termios.h>
#include <iostream>
#include <string>
#include <sstream>


namespace MPOST
{
const char CDataLinkLayer::STX = 0x02;
const char CDataLinkLayer::ETX = 0x03;
const char CDataLinkLayer::ACKMask = 0x01;

// -----------------------------------------------------------------------------------------------

CDataLinkLayer::CDataLinkLayer(CAcceptor* acceptor)
{
    _ackToggleBit = 0;
    _nakCount = 0;

	_acceptor = acceptor;
    _identicalCommandAndReplyCount = 0;
}


// -----------------------------------------------------------------------------------------------

void CDataLinkLayer::SendPacket(char* payload, int payloadLength)
{
    int commandLength = payloadLength + 4; // STX + Length char + ETX + Checksum

	vector<char> command;

	command.push_back(STX);
	command.push_back(commandLength);

    for (int i = 2; i < 2 + payloadLength; i++)
		command.push_back(payload[i - 2]);

    // This char holds a combination of the control code and the acknowledge bit, which toggles between 0 and 1.
    command[2] |= _ackToggleBit;

	command.push_back(ETX);
	command.push_back(ComputeCheckSum(command));

    _currentCommand = command;
    _echoDetect     = command;


	if (write(_acceptor->_port, &command[0], commandLength) == 0)
	{
		close(_acceptor->_port);
		_acceptor->_port = NULL;

		_acceptor->OpenPort();
	}
}


// -----------------------------------------------------------------------------------------------

char CDataLinkLayer::ComputeCheckSum(vector<char> command)
{
    char result = 0;
    for (int i = 1; i < (command[1] - 2); i++)
    {
        result ^= command[i];
    }
    return result;
}


// -----------------------------------------------------------------------------------------------
// PURPOSE
// Once the port read has timed out, we want to disregard any data that may still appear on
// the line.
void CDataLinkLayer::WaitForQuiet()
{
    while (true)
    {
		char byte;

		fd_set fileSet;
		timeval timeout;
		
		timeout.tv_usec = 20 * MICRO_TO_MILLI;
		timeout.tv_sec = 0;	
		FD_ZERO(&fileSet);
		FD_SET(_acceptor->_port, &fileSet);
		
		int res = select(FD_SETSIZE, &fileSet, NULL, NULL, &timeout);
		if (res < 0)	
	        return;

		if (read(_acceptor->_port, &byte, 1) < 1)
			return;
	}
}


// -----------------------------------------------------------------------------------------------


vector<char>* CDataLinkLayer::ReceiveReply()
{
	vector<char>* reply = new vector<char>;

	if (_acceptor->_inSoftResetOneSecondIgnore)
    {
        // We ignore all data coming from the BA for 1 second.
        usleep(1000 * MICRO_TO_MILLI);

		tcflush(_acceptor->_port, TCIOFLUSH);

        _acceptor->_inSoftResetOneSecondIgnore = false;
    }
    
	char stxAndLength[2];
	char payloadAndETXBuffer[128];
//	char nextByte;
	int bytesRemaining, bytesRead;
    char length, checkByte;


	fd_set fileSet;
	timeval timeout;
	
    if (_acceptor->_deviceState != DownloadStart && _acceptor->_deviceState != Downloading)
    	timeout.tv_usec = _acceptor->_transactionTimeout * MICRO_TO_MILLI;
    else
        // When flash downloading, the device can take up to 135 ms to reply. Note that this
        // value was determined from empirical evidence. Peter can't prove that it is correct,
        // but it works.
    	timeout.tv_usec = _acceptor->_downloadTimeout * MICRO_TO_MILLI;
    	
	timeout.tv_sec = 0;	
	FD_ZERO(&fileSet);
	FD_SET(_acceptor->_port, &fileSet);
	
	
	// IMPORTANT NOTE
	// This Linux code was copy directly from the OLE code and is very similar, but the ReceiveReply
	// function is significantly different. Because the read function does not allow the option of
	// a timeout, we have to use a combination of select and read. This resulted in significant delays
	// when reading one byte at a time. As a result, the logic has been changed to allow reading
	// multiple bytes. First the STX and length, and then the remainder of the reply. For the latter
	// a loop is required in case the entire message is not available in one read. Theoretically,
	// reading the 2 bytes of STX and length could also fail (return just one byte)--not sure how
	// best to handle that.
	
	int res = select(FD_SETSIZE, &fileSet, NULL, NULL, &timeout);
	if (res < 0)	
        goto UseExceptionInstead;
	
	bytesRead = read(_acceptor->_port, &stxAndLength, 2);
	if (bytesRead < 1 || stxAndLength[0] != STX)
	{
        goto UseExceptionInstead;
	}
	
	reply->push_back(stxAndLength[0]);	
	reply->push_back(stxAndLength[1]);

	length = stxAndLength[1];
    checkByte = length;

	bytesRemaining = length - 2;
	
	while (bytesRemaining > 0)
    {
		timeout.tv_usec = 20 * MICRO_TO_MILLI;
		timeout.tv_sec = 0;	
		if (select(FD_SETSIZE, &fileSet, NULL, NULL, &timeout) < 0)	
	        goto UseExceptionInstead;

		int bytesRead = read(_acceptor->_port, payloadAndETXBuffer, bytesRemaining);
		
		if (bytesRead < 1)
		{
			goto UseExceptionInstead;
		}
		
		bytesRemaining -= bytesRead;
		
		for (int i = 0; i < bytesRead; i++)
		{
			reply->push_back(payloadAndETXBuffer[i]);

			if (reply->size() < (unsigned)length - 1)
				checkByte ^= payloadAndETXBuffer[i];
//			else
//				cout << payloadAndETXBuffer[i] << endl;
		}
    }

#if 0
	timeout.tv_usec = 20 * MICRO_TO_MILLI;
	timeout.tv_sec = 0;	
	if (select(numFileSets, &fileSet, NULL, NULL, &timeout) < 0)	
        goto UseExceptionInstead;	
	if ((read(_acceptor->_port, &nextByte, 1) < 1) || nextByte != ETX)
		goto UseExceptionInstead;

	reply->push_back(nextByte);

	if (select(numFileSets, &fileSet, NULL, NULL, &timeout) < 0)	
        goto UseExceptionInstead;	
	if ((read(_acceptor->_port, &nextByte, 1) < 1) || nextByte != checkByte)
		goto UseExceptionInstead;

	reply->push_back(nextByte);
#endif
	

	// We never want two transactions to be less than 5 ms (subject to tweaking) apart.
	usleep(5 * MICRO_TO_MILLI);

	// There is a phenomenon that occurs in the circuitry when the head is removed
	// wherein the message sent to the BA is echoed back. This code checks to see if the reply
	// is identical to the command. If so, we return an empty reply.
	if (reply->size() == _echoDetect.size())
	{
		bool replyIdenticalToCommand = true;

		for (unsigned i = 0; i < reply->size(); i++)
			if ((*reply)[i] != _echoDetect[i])
			{
				replyIdenticalToCommand = false;
				break;
			}

		if (replyIdenticalToCommand)
		{
			reply->clear();

			LogCommandAndReply(_currentCommand, *reply, true);

			return reply;
		}
	}
	
    LogCommandAndReply(_currentCommand, *reply, false);

    
	return reply;


UseExceptionInstead:
    WaitForQuiet();

	LogCommandAndReply(_currentCommand, *reply, false);

	return reply;
}


// -----------------------------------------------------------------------------------------------
// PURPOSE
// This command is only used when Acceptor.Close is called, so that the log can be
// "completed" with a final count of how many identical transactions occurred.
void CDataLinkLayer::FlushIdenticalTransactionsToLog()
{
	if (_identicalCommandAndReplyCount > 0)
    {
#if _DEBUG
        TRACE("    : %d transactions identical to previous.", _identicalCommandAndReplyCount);
#endif
		stringstream line;
		line << "    : " << _identicalCommandAndReplyCount <<  " transactions identical to previous.";
        _acceptor->Log(line.str());
    }
}


// -----------------------------------------------------------------------------------------------

void CDataLinkLayer::LogCommandAndReply(vector<char>& command, vector<char>& reply, bool wasEchoDiscarded)
{
    if (!_acceptor->_debugLog)
        return;

    bool isCommandIdentical = true;

    // Compare all bytes in the commands except the checksum.
    for (unsigned i = 0; i < command.size() - 1; i++)
    {
        if (i >= _previousCommand.size())
        {
            isCommandIdentical = false;
            break;
        }

        if (i != 2)
        {
            if (command[i] != _previousCommand[i])
            {
                isCommandIdentical = false;
                break;
            }
        }
        else
        {
            // The second char contains the alternating ack bit, so we have to exclude
            // that bit from the comparison.
            if ((command[i] & 0x7E) != (_previousCommand[i] & 0x7E))
            {
                isCommandIdentical = false;
                break;
            }
        }
    }


    bool isReplyIdentical = true;

    if (reply.size() > 0)
    {
        // Compare all bytes in the replies except the checksum.
        for (unsigned i = 0; i < reply.size() - 1; i++)
        {
            if (i >= _previousReply.size())
            {
                isReplyIdentical = false;
                break;
            }

            if (i != 2)
            {
                if (reply[i] != _previousReply[i])
                {
                    isReplyIdentical = false;
                    break;
                }
            }
            else
            {
                // The second char contains the alternating ack bit, so we have to exclude
                // that bit from the comparison.
                if ((reply[i] & 0x7E) != (_previousReply[i] & 0x7E))
                {
                    isReplyIdentical = false;
                    break;
                }
            }
        }
    }
    else
    {
        isReplyIdentical = _previousReply.size() == 0;
    }

    if (isCommandIdentical && isReplyIdentical && _acceptor->_compressLog)
    {
        _identicalCommandAndReplyCount++;
    }
    else
    {
        if (_identicalCommandAndReplyCount > 0)
        {
#ifdef _DEBUG
			TRACE("    : %d transactions identical to previous.\n", _identicalCommandAndReplyCount);
#endif
			stringstream line;
			line << "    : " << _identicalCommandAndReplyCount <<  " transactions identical to previous.";
            _acceptor->Log(line.str());
        }

        _identicalCommandAndReplyCount = 0;

		string commandString = ("SEND: ");

		for (unsigned i = 0; i < command.size(); i++)
        {
			char buffer[8];
			CAcceptor::itoa(command[i], buffer, 16);

			// Make hex letters uppercase to match the C# version so we can more easily compare
			// trace output.
			for (unsigned i = 0; i < strlen(buffer); i++)
				buffer[i] = toupper(buffer[i]);

			if (strlen(buffer) == 1)
				commandString += "0";
			commandString += buffer;
			commandString += ' ';
        }

	    std::cout << commandString.c_str() << "\n";
	    
#ifdef _DEBUG
        TRACE("%s\n", commandString.c_str());
#endif
        _acceptor->Log(commandString);

		string replyString = ("RECV: ");
		
		// This is just for ease of debugging, so I can see the array in the debugger, something Eclipse does not appear to do for STL types
		char debugReplyBuffer[256];
        for (unsigned i = 0; i < reply.size(); i++)
        	debugReplyBuffer[i] = reply[i];

        if (reply.size() > 0)
        {
            for (unsigned i = 0; i < reply.size(); i++)
            {
				char buffer[8];
				CAcceptor::itoa(reply[i], buffer, 16);

				for (unsigned i = 0; i < strlen(buffer); i++)
					buffer[i] = toupper(buffer[i]);
				
				if (strlen(buffer) == 1)
					replyString += "0";
				replyString += buffer;
				replyString += ' ';
            }
        }
        else
        {
            if (!wasEchoDiscarded)
                replyString += "NO REPLY";
            else
                replyString += "ECHO DISCARDED";
        }

	    std::cout << replyString.c_str() << "\n";
#ifdef _DEBUG
        TRACE("%s\n", replyString.c_str());
#endif
        _acceptor->Log(replyString);

		_previousReply = reply;
		_previousCommand = command;
    }
}


// -----------------------------------------------------------------------------------------------

bool CDataLinkLayer::ReplyAcked(vector<char>* reply)
{
    if (reply->size() < 3)
        return false;

    if (((*reply)[2] & ACKMask) == _ackToggleBit)
    {
        _ackToggleBit ^= 0x01;

        _nakCount = 0;

        return true;
    }
    else
    {
        _nakCount++;

        // If 8 consecutive NAKs are received, force a toggle.
        if (_nakCount == 8)
        {
            _ackToggleBit ^= 0x01;
            _nakCount = 0;
        }

        return false;
    }
}


}

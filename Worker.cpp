#include "Worker.h"
#include "Acceptor.h"

#include <termios.h>
#include <iostream>


namespace MPOST
{

extern pthread_mutex_t inStandardPollMutex;
extern pthread_mutex_t mutex1;
extern pthread_cond_t  condition1;


void* CWorker::MessageLoopThread(void* param)
{
	CAcceptor* acceptor = static_cast<CAcceptor*>(param);

    CDataLinkLayer* dataLinkLayer = new CDataLinkLayer(acceptor);

	acceptor->_dataLinkLayer = dataLinkLayer;

    // We want to poll every 100 ms (arbitrary value within recommended range of 100-200)
    // if there is no other command, but rather than sleeping for 100 ms, we only sleep
    // for 10 ms at a time so that we can check the message messageQueue more frequently. At every
    // 10 loop cycles without a command we issue a standard poll and reset the cycle
    // counter.
    int loopCycleCounter = 0;

    // If the BA does not reply to a command, we ignore it and continue. However, after 30 seconds
    // without receiving any replies, we raise a Disconnected event.
    long timeoutStartTickCount = CAcceptor::GetTickCount();

    while (true)
    {
        if (!acceptor->_inSoftResetWaitForReply)
        {
            usleep(10 * MICRO_TO_MILLI);
        }
        else
       	{
            // After a soft reset, the BA may take up to 15 seconds to start communicating, so
            // we cut the poll rate to once per second.
            usleep(1000 * MICRO_TO_MILLI);
        }

        if ((CAcceptor::GetTickCount() - timeoutStartTickCount) > CAcceptor::CommunicationDisconnectTimeout)
        {
            // NOTE
            // This condition was added when the post-download wait was increased to 30 seconds because testing
            // of the SK2 showed that a 15-second delay was insufficient. In order to avoid a Disconnected event
            // from being raised, we have to suppress the worker thread's checking for disconnection when downloading.
            if (acceptor->_deviceState != Downloading && acceptor->_deviceState != DownloadRestart)
            {
                acceptor->_connected = false;

                if (acceptor->_shouldRaiseDisconnectedEvent)
                    acceptor->RaiseDisconnectedEvent();

                acceptor->_wasDisconnected = true;

                timeoutStartTickCount = CAcceptor::GetTickCount();
            }
        }
        
        if (acceptor->_stopWorkerThread)
        {
			// Clear the message queue so that if the worker thread is started, stopped, and then restarted,
			// old messages will be discarded.
			acceptor->_messageQueue.clear();

            acceptor->_stopWorkerThread = false;

			acceptor->_dataLinkLayer = NULL;
			delete dataLinkLayer;

			acceptor->_workerThread = NULL;

            return 0;
        }
        
        if (!acceptor->_messageQueue.empty())
        {
            loopCycleCounter = 0;

			CMessage* message = acceptor->_messageQueue.front();

			tcflush(acceptor->_port, TCIOFLUSH);
			
			acceptor->_messageQueue.pop_front();

            dataLinkLayer->SendPacket(message->_payload, message->_payloadLength);

            vector<char>* reply = dataLinkLayer->ReceiveReply();

            if (reply->size() > 0)
            {
                timeoutStartTickCount = CAcceptor::GetTickCount();

                if (acceptor->_wasDisconnected)
                {
                    acceptor->_wasDisconnected = false;
                    
                    // If OpenThread is still active, that means that the entire connection process was never
                    // completed, so we want to let OpenThread finish and issue the Connected event itself.
                    if (acceptor->_openThread == 0)
                    {
						// NOTE
						// We cannot simply test _deviceState because it is not set until ProcessReply.
	                    if (((*reply)[2] & 0x70) != 0x50)
	                    {
	                        acceptor->_connected = true;
	                        acceptor->RaiseConnectedEvent();
	                    }
                    }
                }

                if (acceptor->_inSoftResetWaitForReply)
                {
                    acceptor->_inSoftResetWaitForReply = false;
                }

                acceptor->_isReplyAcked = dataLinkLayer->ReplyAcked(reply);

                if (message->_isSynchronous)
                {
	                acceptor->_replyQueue.push_back(reply);

//	                pthread_mutex_unlock( &mutex1 );	                
//	                pthread_cond_signal(&condition1);
                }
                else
                    acceptor->ProcessReply(*reply);
            }
            else
            {
                // If we receive no reply and the command was synchronous, we still post an empty
                // reply to the messageQueue so that the caller does not block. The caller is
                // expected to handle an empty reply.
                if (message->_isSynchronous)
                {
                    acceptor->_replyQueue.push_back(reply);
                }
            }

			delete message;
        }
        else
        {
            // NOTE
            // This flag exists solely to handle a scenario in which the Calibrate method sets
            // DeviceState to CalibrateStart and the CWorker thread somehow executes another standard
            // poll before the main thread has had a chance to issue the Calibrate command. When that
            // happens ProcessReply sees that the Calibrate bit is not set but DeviceState == CalibrateStart,
            // and it erroneously thinks calibration has completed. Our solution, perhaps not the best
            // is to suppress that standard poll. This flag is reset when a command is dequeued
            // from the message queue. Needless to say, we should not set this flag unless we are
            // about to issue a command.
            if (acceptor->_suppressStandardPoll)
                continue;

            loopCycleCounter++;

            if (loopCycleCounter == 9)
            {
                loopCycleCounter = 0;
                
                pthread_mutex_lock(&inStandardPollMutex);
                                	  
                
                char payload[4];

                acceptor->ConstructOmnibusCommand(payload, CmdOmnibus, 1);

                dataLinkLayer->SendPacket(payload, sizeof(payload));

                vector<char>* reply = dataLinkLayer->ReceiveReply();

                if (reply->size() > 0)
                {
                    timeoutStartTickCount = CAcceptor::GetTickCount();

                    if (acceptor->_wasDisconnected)
                    {
                        acceptor->_wasDisconnected = false;

						// NOTE
						// We cannot simply test _deviceState because it is not set until ProcessReply.
                        if (((*reply)[2] & 0x70) != 0x50)
                        {
                            acceptor->_connected = true;
                            acceptor->RaiseConnectedEvent();
                        }
                        else
                        {
                            acceptor->RaiseDownloadRestartEvent();
                        }
                    }

                    if (acceptor->_inSoftResetWaitForReply)
                    {
                        acceptor->_inSoftResetWaitForReply = false;
                    }


                    // NOTE
                    // My preference was to reset the event at the beginning of ProcessReply, but
                    // the problem is that there is a tiny possibility that between the time that
                    // execution enters ProcessReply and the event is reset, that Calibrate
                    // will test the event and find it non-signaled.                    
                    acceptor->_isReplyAcked = dataLinkLayer->ReplyAcked(reply);
                    
                    acceptor->ProcessReply(*reply);
                }
				else
				{
					delete reply;
				}

                pthread_mutex_unlock(&inStandardPollMutex);                                	  
            }
        }
    }
}

}

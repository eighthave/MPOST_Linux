#include "Acceptor.h"
#include "Worker.h"

#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

namespace MPOST
{

const char CAcceptor::C_R_N[] = "Copyright (C) 2007 MEI - "
                                "The information contained here-in is the property of MEI and is not to be " 
                                "disclosed or used without prior written permission of MEI. This copyright " 
                                "extends to all media in which this information may be preserved including " 
                                "magnetic storage, computer print-out or visual display.";



pthread_mutex_t inStandardPollMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition1 = PTHREAD_COND_INITIALIZER;

CAcceptor::CAcceptor()
{
	pthread_mutex_init(&inStandardPollMutex, NULL);
	pthread_mutex_init(&mutex1,   NULL);	
	pthread_cond_init(&condition1, NULL);

	_port						= NULL;

    _autoStack              			= false;
    _bnfStatus              			= Unknown;
    _capApplicationID       			= false;
    _capApplicationPN       			= false;
    _capAssetNumber         			= false;
    _capAudit               			= false;
    _capBarCodes            			= false;
    _capBarCodesExt         			= false;
    _capBNFStatus           			= false;
    _capBookmark            			= false;
    _capBootPN              			= false;
    _capCalibrate           			= false;
    _capCashBoxTotal        			= false;
    _capCouponExt           			= false;
    _capDevicePaused        			= false;
    _capDeviceSoftReset     			= false;
    _capDeviceType          			= false;
    _capDeviceResets        			= false;
    _capDeviceSerialNumber  			= false;
    _capEscrowTimeout       			= false;			
    _capFlashDownload					= false;
    _capNoPush              			= false;
    _capOrientationExt      			= false;
    _capPupExt              			= false;
    _capTestDoc             			= false;
    _capVariantID           			= false;
    _capVariantPN           			= false;
    _cashBoxAttached        			= false;
    _cashBoxFull            			= false;
	_connected							= false;
    _debugLog               			= false;
    _deviceFailure          			= false;
    _devicePaused           			= false;
    _deviceStalled          			= false;
    _deviceState            			= Disconnected;
    _docType                			= None;
	_enableAcceptance					= false;
    _enableBarCodes         			= false;
    _enableBookmarks        			= false;
    _enableCouponExt        			= false;
    _enableNoPush           			= false;
    _highSecurity           			= false;
    _orientationCtl         			= FourWay;
    _orientationCtlExt      			= FourWay;
	_version							= "V1.12, 283795112";

    _isDeviceJammed						= false;
    _isCheated	 						= false;
    _isPoweredUp	 					= false;
    _isInvalidCommand	 				= false;
    
    _inSoftResetOneSecondIgnore			= false;
    _inSoftResetWaitForReply			= false;

    _wasDocTypeSetOnEscrow				= false;

    // This flag is set to true when communication is lost (30 sec w/o a reply) AFTER having been
    // established to begin with (via the Open method). When the worker thread receives a reply
    // and this flag is true, it should raise a Connected event.
    _wasDisconnected					= false;

    // This flag is only true when the application starts up and only until it receives a 
    // to its very first poll, an "empty" poll used to retrieve capabilities. The flag is used
    // to suppress the raising of events, except PowerUp, for that first poll cycle.
    _isVeryFirstPoll					= true;

	_suppressStandardPoll				= false;

    _shouldRaiseConnectedEvent			= true;
	_shouldRaiseDisconnectedEvent		= true;
	_shouldRaiseEscrowEvent				= true;
    _shouldRaisePUPEscrowEvent			= true;
    _shouldRaiseStackedEvent			= true;
    _shouldRaiseReturnedEvent			= true;
    _shouldRaiseRejectedEvent			= true;
    _shouldRaiseCheatedEvent			= true;
    _shouldRaiseStackerFullEvent		= true;
    _shouldRaiseCalibrateStartEvent		= true;
    _shouldRaiseCalibrateProgressEvent	= true;
    _shouldRaiseCalibrateFinishEvent	= false;
    _shouldRaiseDownloadStartEvent		= true;
    _shouldRaiseDownloadRestartEvent	= true;
    _shouldRaiseDownloadProgressEvent	= true;
    _shouldRaiseDownloadFinishEvent		= true;
    _shouldRaisePauseDetectedEvent		= true;
    _shouldRaisePauseClearedEvent		= false;
    _shouldRaiseStallDetectedEvent		= true;
    _shouldRaiseStallClearedEvent		= false;
    _shouldRaiseJamDetectedEvent		= true;
    _shouldRaiseJamClearedEvent			= false;
    _shouldRaisePowerUpEvent			= true;
    _shouldRaiseInvalidCommandEvent		= true;

    _shouldRaiseCashBoxAttachedEvent	= false;
    _shouldRaiseCashBoxRemovedEvent		= true;

    _stopWorkerThread					= false;
    _stopOpenThread						= false;
    _stopFlashDownloadThread			= false;

	_transactionTimeout					= 50;
	_downloadTimeout					= 200;
 

    for (int i = Events_Begin; i < Events_End; i++)
    	_eventHandlers[i] = 0;
    

    _workerThread						= NULL;
    _openThread							= NULL;
    _flashDownloadThread				= NULL;
	_dataLinkLayer						= NULL;

	_isReplyAcked = false;

    _compressLog = false; // true;

	_debugLogPath = "/root/";
}


void CAcceptor::Open(string portName, PowerUp powerUp)
{
    if (_connected)
	{
		throw CAcceptorException("Open cannot be called when Connected == true.");
	}


    // If unable to connect, we raise a Disconnected event (even if Connected is never raised).
    _shouldRaiseDisconnectedEvent = true;


    _devicePortName = portName;
    _devicePowerUp = powerUp;


	if (!OpenPort())
	{
		throw CAcceptorException("Could not open the serial port.");
	}


    if (_debugLog)
    {
        OpenLogFile();
    }


	pthread_create(&_workerThread, NULL, CWorker::MessageLoopThread, (void*)this);

	pthread_create(&_openThread, NULL, CAcceptor::OpenThread, (void*)this);
	
	
}


// -----------------------------------------------------------------------------------------------
// PURPOSE
// Because opening a connection to the BA can take a long time (usually no more than 30 secs), but
// under some circumstances as much as 2 minutes, we spawn a thread so that the host application's can
// is not blocked for that long.
void* CAcceptor::OpenThread(void* param)
{
	CAcceptor* acceptor = static_cast<CAcceptor*>(param);

	vector<char>* reply = NULL;

    bool wasStopped = false;

    PollingLoop(acceptor, &reply, wasStopped);

    if (wasStopped)
        return 0;

    acceptor->ProcessReply(*reply);

    acceptor->QueryDeviceCapabilities();

    if (acceptor->_deviceState != DownloadRestart)
    {
        acceptor->SetUpBillTable();

        acceptor->_connected = true;

        if (acceptor->_shouldRaiseConnectedEvent)
            acceptor->RaiseConnectedEvent();
    }
    else
    {
        acceptor->RaiseDownloadRestartEvent();
    }

	acceptor->_openThread = NULL;

	return 0;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::OpenPort()
{
    struct termios oldtio,my_termios;
    
    _port = open(_devicePortName.c_str(), O_RDWR | O_NOCTTY  | O_NDELAY); 
    if (_port < 0) {perror(_devicePortName.c_str()); exit(-1); }
    
    tcgetattr(_port, &oldtio); /* save current port settings */
    
    bzero(&my_termios, sizeof(my_termios));
    my_termios.c_cflag = B9600 | CS7 | CREAD | CLOCAL | HUPCL | PARENB;
    my_termios.c_oflag = 0;
    my_termios.c_iflag = IGNPAR | IGNBRK;
    my_termios.c_lflag = NOFLSH;
 
    cfsetospeed(&my_termios, B9600);
    cfsetispeed(&my_termios, B9600);
    
       
    /* set input mode (non-canonical, no echo,...) */
    my_termios.c_lflag = 0;
     
    my_termios.c_cc[VTIME]    = 0;
    my_termios.c_cc[VMIN]     = 11;// 11;
    
    tcflush(_port, TCIFLUSH);
    tcsetattr(_port, TCSANOW, &my_termios);

	return true;
}


// -----------------------------------------------------------------------------------------------
// NOTE
// This loop is used by both OpenThread and FlashDownloadThread.
//
void CAcceptor::PollingLoop(CAcceptor* acceptor, vector<char>** reply, bool &wasStopped)
{
    long startTickCount = GetTickCount();

    // We poll until we either receive a reply or the host application calls Close.
    do
    {
        // The first poll command we send needs to have everything turned off so that we can determine the model
        // and capabilities.
        char payload[] = { CmdOmnibus, 0x00, 0x10, 0x00 };

        // Because the DataLinkLayer (used by Worker) attempts to reopen the serial port
        // when it cannot communicate, the Worker.MessageLoopThread can block for more than 10 ms,
        // during which time the SendCommand below can be adding multiple unnecessary commands
        // to the message queue. All polling commands (really, there should only be one) that
        // the MessageLoopThread did not process are irrelevant and can be discarded.        
		acceptor->_messageQueue.clear();

        *reply = acceptor->SendSynchronousCommand(payload, sizeof(payload));

        if (GetTickCount() - startTickCount > PollingDisconnectTimeout)
        {
            if (acceptor->_shouldRaiseDisconnectedEvent)
                acceptor->RaiseDisconnectedEvent();

            startTickCount = GetTickCount();
        }


        if (acceptor->_flashDownloadThread != 0)
        {
            if (acceptor->_stopFlashDownloadThread)
            {
                acceptor->_stopFlashDownloadThread = true;
                pthread_join(acceptor->_flashDownloadThread, NULL);
                acceptor->_deviceState = Idling;
                wasStopped = true;
				return;
            }
        }
        else if (acceptor->_openThread != 0)
        {
            if (acceptor->_stopOpenThread)
            {
                acceptor->_stopOpenThread = false;
                acceptor->_stopWorkerThread = true;

                pthread_join(acceptor->_workerThread, NULL);

                close(acceptor->_port);
				acceptor->_port = NULL;
                wasStopped = true;
                return;
            }
        }

		if ((*reply)->size() > 0)
			return;
		else
			delete *reply;
    }
    while (true);
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::Close()
{
    if (_flashDownloadThread != 0)
    {
        _stopFlashDownloadThread = true;

        pthread_join(_flashDownloadThread, NULL); 
    }
    else if (!_connected)
    {
        // Because it might take time to open a connection, and we do the open in a separate thread
        // the host appliction might decide it is taking too long, or respond to the Disconnected
        // event, and call Close. If so, we just stop the Open thread.
        if (_openThread != 0)
        {
            _stopOpenThread = true;
            CloseLogFile();
            return;
        }
    }


    if (_enableAcceptance)
        _enableAcceptance = false;


	if (_dataLinkLayer != NULL)
	    _dataLinkLayer->FlushIdenticalTransactionsToLog();


    CloseLogFile();


    _stopWorkerThread = true;

    
    pthread_join(_workerThread, NULL); 


	close(_port);
	_port = NULL;


    _connected = false;


    if (_shouldRaiseDisconnectedEvent)
        RaiseDisconnectedEvent();
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::EscrowReturn()
{
    VerifyConnected("EscrowReturn");

    char payload[4];

    ConstructOmnibusCommand(payload, CmdOmnibus, 1);

    payload[2] |= 0x40;

    SendAsynchronousCommand(payload, sizeof(payload));
}



// -----------------------------------------------------------------------------------------------

void CAcceptor::EscrowStack()
{
    VerifyConnected("EscrowStack");
    
    char payload[4];

    ConstructOmnibusCommand(payload, CmdOmnibus, 1);

    payload[2] |= 0x20;

    SendAsynchronousCommand(payload, sizeof(payload));
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetApplicationID()
{
    VerifyPropertyIsAllowed(_capApplicationID, "ApplicationID");

    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorApplicationID };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

	if (reply->size() == 14)
	{
		string s(&(*reply)[3], 9);
		
		return s;
	}
	else
	{
		return "";
	}

	delete reply;

	return "";
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetApplicationPN()
{
    VerifyPropertyIsAllowed(_capApplicationPN, "ApplicationPN");

    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorApplicationPartNumber };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

	if (reply->size() == 14)
	{
		string s(&(*reply)[3], 9);
		
		return s;
	}
	else
	{
		return "";
	}

	delete reply;

	return "";
}



// -----------------------------------------------------------------------------------------------

vector<int> CAcceptor::GetAuditLifeTimeTotals()
{
    VerifyPropertyIsAllowed(_capAudit, "AuditLifeTimeTotals");

    
    vector<int> values;

    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorAuditLifeTimeTotals };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    // Verify that the length is of a correct size, which is a multiple of 8 plus 5 overhead
    // bytes, and there must be at least one datum.
    if (reply->size() < 13 || ((reply->size() - 5) % 8 != 0))
	{
		delete reply;
        return values;
	}


    int fieldCount = ((*reply)[1] - 5) / 8;

    for (long i = 0; i < fieldCount; i++)
    {
       int value = (((*reply)[8*i +  3] & 0x0F) << 28) +
                   (((*reply)[8*i +  4] & 0x0F) << 24) + 
                   (((*reply)[8*i +  5] & 0x0F) << 20) +
                   (((*reply)[8*i +  6] & 0x0F) << 16) +
                   (((*reply)[8*i +  7] & 0x0F) << 12) +
                   (((*reply)[8*i +  8] & 0x0F) <<  8) +
                   (((*reply)[8*i +  9] & 0x0F) <<  4) +
                   (((*reply)[8*i + 10] & 0x0F)      );

       values.push_back(value);
    }

	delete reply;

	return values;
}


// -----------------------------------------------------------------------------------------------

vector<int> CAcceptor::GetAuditPerformance()
{
    VerifyPropertyIsAllowed(_capAudit, "AuditPerformance");


    vector<int> values;
    
    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorAuditPerformanceMeasures };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    // Verify that the length is of a correct size, which is a multiple of 8 plus 5 overhead
    // bytes, and there must be at least one data
    if (reply->size() < 9 || ((reply->size() - 5) % 4 != 0))
	{
		delete reply;
        return values;
	}

    int fieldCount = ((*reply)[1] - 5) / 4;

    for (long i = 0; i < fieldCount; i++)
    {
        int value = (((*reply)[4 * i + 3] & 0x0F) << 12) +
                    (((*reply)[4 * i + 4] & 0x0F) << 8) +
                    (((*reply)[4 * i + 5] & 0x0F) << 4) +
                    (((*reply)[4 * i + 6] & 0x0F));

        values.push_back(value);
    }

	delete reply;

	return values;
}


// -----------------------------------------------------------------------------------------------

vector<int> CAcceptor::GetAuditQP()
{
    VerifyPropertyIsAllowed(_capAudit, "AuditQP");


    vector<int> values;
    
    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorAuditQPMeasures };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    // Verify that the length is of a correct size, which is a multiple of 8 plus 5 overhead
    // bytes, and there must be at least one data
    if (reply->size() < 9 || ((reply->size() - 5) % 4 != 0))
	{
		delete reply;
        return values;
	}

    int fieldCount = ((*reply)[1] - 5) / 4;

    for (long i = 0; i < fieldCount; i++)
    {
       int value = (((*reply)[4*i + 3] & 0x0F) << 12) +
                   (((*reply)[4*i + 4] & 0x0F) <<  8) +
                   (((*reply)[4*i + 5] & 0x0F) <<  4) +
                   (((*reply)[4*i + 6] & 0x0F)      );

       values.push_back(value);
    }

	delete reply;

	return values;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetAutoStack()
{
	return _autoStack;
}

void CAcceptor::SetAutoStack(bool newVal)
{
	_autoStack = newVal;
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetBarCode()
{
	return _barCode;
}


// -----------------------------------------------------------------------------------------------

CBill CAcceptor::GetBill()
{
	return _bill;
}


// -----------------------------------------------------------------------------------------------

vector<CBill> CAcceptor::GetBillTypes()
{
	return _billTypes;
}


// -----------------------------------------------------------------------------------------------

vector<bool> CAcceptor::GetBillTypeEnables()
{
	return _billTypeEnables;
}

void CAcceptor::SetBillTypeEnables(vector<bool> newVal)
{
    if (!_connected)
	{
		throw CAcceptorException("Calling BillTypeEnables not allowed when not connected.");
	}

    if (_billTypeEnables.size() != _billTypes.size())
	{
		throw CAcceptorException("CBillTypeEnables size must match BillTypes size.");
	}

    _billTypeEnables = newVal;

    // For terse mode, enabling/disabling denominations occurs in the standard
    // omnibus command.
    if (_expandedNoteReporting)
    {
	    char payload[15] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        ConstructOmnibusCommand(payload, CmdExpanded, 2);
        payload[1] = 0x03; // Sub Type

        for (unsigned i = 0; i < _billTypeEnables.size(); i++)
        {
            int enableIndex = i / 7;
            int bitPosition = i % 7;
            int bit         = 1 << bitPosition;

            if (_billTypeEnables[i])
                payload[5 + enableIndex] |= (char)bit;
        }

        SendAsynchronousCommand(payload, sizeof(payload));
    }
}


// -----------------------------------------------------------------------------------------------

vector<CBill> CAcceptor::GetBillValues()
{
	return _billValues;
}

// -----------------------------------------------------------------------------------------------

vector<bool> CAcceptor::GetBillValueEnables()
{
	return _billValueEnables;
}

void CAcceptor::SetBillValueEnables(vector<bool> newVal)
{
	_billValueEnables = newVal;

    for (unsigned i = 0; i < _billValueEnables.size(); i++)
    {
        for (unsigned j = 0; j < _billTypes.size(); j++)
        {
            if ((_billTypes[i]._value == _billValues[j]._value) && (_billTypes[i]._country == _billValues[j]._country))
            {
                _billTypeEnables[j] = _billValueEnables[i];
			}
        }
    }


    char payload[15] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ConstructOmnibusCommand(payload, CmdExpanded, 2);
    payload[1] = 0x03; // Sub Type

    for (unsigned i = 0; i < _billTypeEnables.size(); i++)
    {
        int enableIndex = i / 7;
        int bitPosition = i % 7;
        int bit         = 1 << bitPosition;

        if (_billTypeEnables[i])
            payload[5 + enableIndex] |= (char)bit;
    }

    SendAsynchronousCommand(payload, sizeof(payload));
}


// -----------------------------------------------------------------------------------------------

BNFStatus CAcceptor::GetBNFStatus()
{
    VerifyPropertyIsAllowed(_capBNFStatus, "BNFStatus");


    BNFStatus status;
    
    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryBNFStatus };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    if (reply->size() == 9)
    {
        if ((*reply)[3] == 0)
        {
            status = NotAttached;
        }
        else
        {
            if ((*reply)[4] == 0)
            	status = OK;
            else
            	status = Error;
        }
    }
    else
    {
    	status = Unknown;
    }

    return status;
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetBootPN()
{
    VerifyPropertyIsAllowed(_capBootPN, "BootPN");

    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorBootPartNumber };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

	if (reply->size() == 14)
	{
		string s(&(*reply)[3], 9);
		
		return s;
	}
	else
	{
		return "";
	}

	delete reply;

	return "";
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapApplicationID()
{
	return _capApplicationID;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapApplicationPN()
{
	return _capApplicationPN;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapAssetNumber()
{
	return _capAssetNumber;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapAudit()
{
	return _capAudit;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapBarCodes()
{
	return _capBarCodes;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapBarCodesExt()
{
	return _capBarCodesExt;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapBNFStatus()
{
	return _capBNFStatus;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapBookmark()
{
	return _capBookmark;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapBootPN()
{
	return _capBootPN;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapCalibrate()
{
	return _capCalibrate;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapCashBoxTotal()
{
	return _capCashBoxTotal;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapCouponExt()
{
	return _capCouponExt;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapDevicePaused()
{
	return _capDevicePaused;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapDeviceSoftReset()
{
	return _capDeviceSoftReset;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapDeviceType()
{
	return _capDeviceType;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapDeviceResets()
{
	return _capDeviceResets;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapDeviceSerialNumber()
{
	return _capDeviceSerialNumber;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapEscrowTimeout()
{
	return _capEscrowTimeout;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapFlashDownload()
{
	return _capFlashDownload;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapNoPush()
{
	return _capNoPush;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapOrientationExt()
{
	return _capOrientationExt;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapPupExt()
{
	return _capPupExt;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapTestDoc()
{
	return _capTestDoc;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapVariantID()
{
	return _capVariantID;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCapVariantPN()
{
	return _capVariantPN;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCashBoxAttached()
{
	return _cashBoxAttached;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetCashBoxFull()
{
	return _cashBoxFull;
}


// -----------------------------------------------------------------------------------------------

long CAcceptor::GetCashBoxTotal()
{
    VerifyPropertyIsAllowed(_capCashBoxTotal, "CashBoxTotal");


    char payload[] = { CmdOmnibus, 0x7F, 0x3C, 0x02 };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    long total;
    
    if (reply->size() < 9)
	{
    	total = 0;
	}
	else
	{
		total = (((*reply)[3] & 0x0F) << 20) +
				(((*reply)[4] & 0x0F) << 16) +
				(((*reply)[5] & 0x0F) << 12) +
				(((*reply)[6] & 0x0F) << 8) +
				(((*reply)[7] & 0x0F) << 4) +
				(((*reply)[8] & 0x0F));
	}


	delete reply;

	return total;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetConnected()
{
	return _connected;
}


// -----------------------------------------------------------------------------------------------

CCoupon CAcceptor::GetCoupon()
{
	return _coupon;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetDebugLog()
{
	return _debugLog;
}

void CAcceptor::SetDebugLog(bool newVal)
{
    if (newVal)
    {
        if (!_debugLog && _connected)
        {
            OpenLogFile();
        }

        _debugLog = newVal;
    }
    else
    {
        if (_debugLog && _connected)
        {
            CloseLogFile();
        }

        _debugLog = newVal;
    }
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetDebugLogPath()
{
	return _debugLogPath;
}

void CAcceptor::SetDebugLogPath(string newVal)
{
	_debugLogPath = newVal;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetDeviceBusy()
{
	
	return (_deviceState != Idling);
}


// -----------------------------------------------------------------------------------------------

long CAcceptor::GetDeviceCRC()
{
    VerifyPropertyIsAllowed(true, "DeviceCRC");


    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQuerySoftwareCRC };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));
    
    long crc;

    if (reply->size() < 7)
	{
    	crc = 0;
	}
	else
	{
		crc = (((*reply)[3] & 0x0F) << 12) +
				(((*reply)[4] & 0x0F) << 8) +
				(((*reply)[5] & 0x0F) << 4) +
				(((*reply)[6] & 0x0F));
	}

	delete reply;

	return crc;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetDeviceFailure()
{
	return (_deviceState == Failed);
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetDeviceJammed()
{
	return _isDeviceJammed;
}


// -----------------------------------------------------------------------------------------------

long CAcceptor::GetDeviceModel()
{
	return _deviceModel;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetDevicePaused()
{
	return _devicePaused;
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetDevicePortName()
{
	return _devicePortName;
}


// -----------------------------------------------------------------------------------------------

PowerUp CAcceptor::GetDevicePowerUp()
{
	return _devicePowerUp;
}


// -----------------------------------------------------------------------------------------------

long CAcceptor::GetDeviceResets()
{
    VerifyPropertyIsAllowed(_capDeviceResets, "DeviceResets");


    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryDeviceResets };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));
    
    long resets;

    if (reply->size() < 9)
	{
    	resets = 0;
	}
	else
	{
		resets = (((*reply)[3] & 0x0F) << 20) +
				 (((*reply)[4] & 0x0F) << 16) +
				 (((*reply)[5] & 0x0F) << 12) +
				 (((*reply)[6] & 0x0F) << 8) +
				 (((*reply)[7] & 0x0F) << 4) +
				 (((*reply)[8] & 0x0F));
	}

	delete reply;

	return resets;
}


// -----------------------------------------------------------------------------------------------

long CAcceptor::GetDeviceRevision()
{
	return _deviceRevision;
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetDeviceSerialNumber()
{
    VerifyPropertyIsAllowed(_capDeviceSerialNumber, "DeviceSerialNumber");


    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorSerialNumber };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    // According to the spec, the string is terminated by a "non-printable" character. I 
    // don't know why it is not more specific, but Peter said to check for 0x00 to 0x1F or
    // 0x7F.
    unsigned validCharIndex = 3;

    while ((validCharIndex < reply->size()) && ((*reply)[validCharIndex] > 0x20) && ((*reply)[validCharIndex] < 0x7F) && validCharIndex <= 22)
        validCharIndex++;
    
    int returnedStringLength = validCharIndex - 3;

	string s(&(*reply)[3], returnedStringLength);

	delete reply;

	return s;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetDeviceStalled()
{
	return _deviceStalled;
}


// -----------------------------------------------------------------------------------------------

State CAcceptor::GetDeviceState()
{
	return _deviceState;
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetDeviceType()
{
    VerifyPropertyIsAllowed(_capDeviceType, "DeviceType");


    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorType };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    // According to the spec, the string is terminated by a "non-printable" character. I 
    // don't know why it is not more specific, but Peter said to check for 0x00 to 0x1F or
    // 0x7F.
    unsigned validCharIndex = 3;

    while ((validCharIndex < reply->size()) && ((*reply)[validCharIndex] > 0x20) && ((*reply)[validCharIndex] < 0x7F) && validCharIndex <= 22)
        validCharIndex++;
    
    int returnedStringLength = validCharIndex - 3;
    
	string s(&(*reply)[3], returnedStringLength);

	delete reply;

	return s;
}


// -----------------------------------------------------------------------------------------------

DocumentType CAcceptor::GetDocType()
{
	return _docType;
}

// -----------------------------------------------------------------------------------------------

int CAcceptor::GetTransactionTimeout()
{
	return _transactionTimeout;
}

// -----------------------------------------------------------------------------------------------

void CAcceptor::SetTransactionTimeout(int newVal)
{
	_transactionTimeout = newVal;
}

// -----------------------------------------------------------------------------------------------

int CAcceptor::GetDownloadTimeout()
{
	return _downloadTimeout;
}

// -----------------------------------------------------------------------------------------------

void CAcceptor::SetDownloadTimeout(int newVal)
{
	_downloadTimeout = newVal;
}

// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetEnableAcceptance()
{
	return _enableAcceptance;
}

void CAcceptor::SetEnableAcceptance(bool newVal)
{
	_enableAcceptance = newVal;
}

// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetEnableBarCodes()
{
	return _enableBarCodes;
}

void CAcceptor::SetEnableBarCodes(bool newVal)
{
	_enableBarCodes = newVal;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetEnableBookmarks()
{
	return _enableBookmarks;
}

void CAcceptor::SetEnableBookmarks(bool newVal)
{
	_enableBookmarks = newVal;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetEnableCouponExt()
{
	return _enableCouponExt;
}

void CAcceptor::SetEnableCouponExt(bool newVal)
{
	_enableCouponExt = newVal;
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetEnableNoPush()
{
	return _enableNoPush;
}

void CAcceptor::SetEnableNoPush(bool newVal)
{
	_enableNoPush = newVal;
}


// -----------------------------------------------------------------------------------------------

Orientation CAcceptor::GetEscrowOrientation()
{
	if (_capOrientationExt)
	{
		return _escrowOrientation;
	}
	else
	{
		return UnknownOrientation;
	}
}


// -----------------------------------------------------------------------------------------------

bool CAcceptor::GetHighSecurity()
{
	return _highSecurity;
}

void CAcceptor::SetHighSecurity(bool newVal)
{
	_highSecurity = newVal;
}


// -----------------------------------------------------------------------------------------------

OrientationControl CAcceptor::GetOrientationControl()
{
	return _orientationCtl;
}

void CAcceptor::SetOrientationControl(OrientationControl newVal)
{
	_orientationCtl = newVal;
}


// -----------------------------------------------------------------------------------------------

OrientationControl CAcceptor::GetOrientationCtlExt()
{
	return _orientationCtlExt;
}

void CAcceptor::SetOrientationCtlExt(OrientationControl newVal)
{
	_orientationCtlExt = newVal;
}


// -----------------------------------------------------------------------------------------------

vector<string> CAcceptor::GetVariantNames()
{
    VerifyPropertyIsAllowed(true, "VariantNames");


    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorVariantName };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    // According to the spec, the string is terminated by a "non-printable" character. I 
    // don't know why it is not more specific, but Peter said to check for 0x00 to 0x1F or
    // 0x7F.
    unsigned validCharIndex = 3;
    int nameCount = 0;

	vector<string> names;

    while ((validCharIndex < reply->size()) && ((*reply)[validCharIndex] > 0x20) && ((*reply)[validCharIndex] < 0x7F) && validCharIndex <= 34)
    {
        if (validCharIndex + 2 < reply->size())
        {
        	string s(&(*reply)[validCharIndex], 3);

			names.push_back(s);

            nameCount++;
        }
        else
            break;

        validCharIndex += 4;
    }



	delete reply;

	return names;
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetVariantID()
{
    VerifyPropertyIsAllowed(_capVariantID, "VariantID");

    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorVariantID };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

	if (reply->size() == 14)
	{
		string s(&(*reply)[3], 9);
		
		return s;
	}
	else
	{
		return "";
	}

	delete reply;

	return "";
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetVariantPN()
{
    VerifyPropertyIsAllowed(_capVariantPN, "VariantPN");

    char payload[] = { CmdAuxiliary, 0, 0, CmdAuxQueryAcceptorVariantPartNumber };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

	if (reply->size() == 14)
	{
		string s(&(*reply)[3], 9);
		
		return s;
	}
	else
	{
		return "";
	}

	delete reply;

	return "";
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::GetVersion()
{
	return _version;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::Calibrate()
{
    VerifyConnected("Calibrate");

    if (_deviceState != Idling)
	{
		throw CAcceptorException("Calibrate allowed only when DeviceState == Idling.");
	}


    char payload[] = { CmdCalibrate, 0x00, 0x00, 0x00 };

    vector<char>* reply;

    // If the worker thread is in the middle of a standard poll, we want to wait until it finishes.
	pthread_mutex_lock(&inStandardPollMutex);		  
	pthread_mutex_unlock(&inStandardPollMutex);
		  

    _suppressStandardPoll = true;


    _deviceState = CalibrateStart;

    RaiseCalibrateStartEvent();

    _shouldRaiseCalibrateProgressEvent = true;

    long startTickCount = GetTickCount();

    do
    {
        reply = SendSynchronousCommand(payload, sizeof(payload));

        if (reply->size() == 11 && (((*reply)[2] & 0x70) == 0x40))
            break;

        if (GetTickCount() - startTickCount > CalibrateTimeout)
        {
            RaiseCalibrateFinishEvent();
			delete reply;
            return;
        }

    }
    while (true);

	delete reply;
}


// -----------------------------------------------------------------------------------------------

struct FlashDownloadThreadParams
{
	CAcceptor*	acceptor;
	ifstream	downloadFile;
	streampos	downloadFileSize;
};

void CAcceptor::FlashDownload(string filePath)
{
    if (!_connected && _deviceState != DownloadRestart)
	{
		throw CAcceptorException("FlashDownload not allowed when not connected.");
	}

	// NOTE
	// THIS FIRST GUARD IS ONLY TEMPORARY UNTIL WE FIGURE OUT WHY THE S2K IS NOT SENDING DOWNLOAD BIT.
	if (!((_deviceModel >= 65 && _deviceModel <= 71) || _deviceModel == 77 || _deviceModel == 80 || _deviceModel == 81 || _deviceModel == 87 || _deviceModel == 88))
	{
		if (_deviceState != Idling && _deviceState != DownloadRestart)
		{
			throw CAcceptorException("FlashDownload allowed only when DeviceState == Idling.");
		}
	}

	time_t currentTime;
	char timeBuffer[32];
	time(&currentTime);    	
	strftime(timeBuffer, sizeof(timeBuffer), "FlashDownload: %H:%M:%S", localtime(&currentTime));
	cout << timeBuffer << endl;


	FlashDownloadThreadParams* params = new FlashDownloadThreadParams;

	params->downloadFile.open(filePath.c_str(), ios_base::in | ios_base::binary);

	streampos begin = params->downloadFile.tellg();
	
	params->downloadFile.seekg(0, ios::end);

	params->downloadFileSize = params->downloadFile.tellg() - begin;

    if ((params->downloadFileSize % 32) != 0)
    {
        params->downloadFile.close();

		throw CAcceptorException("Flash download file size must be divisible by 32.");

		delete params;

		return;
    }


	params->acceptor = this;

	pthread_create(&_flashDownloadThread, NULL, FlashDownloadThread, (void*)params);

	return;
}


void* CAcceptor::FlashDownloadThread(void* param)
{
	FlashDownloadThreadParams* params = static_cast<FlashDownloadThreadParams*>(param);

	CAcceptor* acceptor = params->acceptor;
	
	CSuppressStandardPoll suppressStandardPoll(acceptor);
	
    vector<char>* reply = NULL;

    char payload[] = { CmdOmnibus, 0x00, 0x00, 0x00 };

    int packetNum = 0;
    int finalPacketNum = (int)(params->downloadFileSize / 32);

    if (acceptor->_deviceState != DownloadRestart)
    {
        // If the worker thread is in the middle of a standard poll, we want to wait until it finishes.
    	pthread_mutex_lock(&inStandardPollMutex);    		  
    	pthread_mutex_unlock(&inStandardPollMutex);
    		  
        // NOTE
        // This might technically be wrong, depending on how we choose to define when the DownloadStart
        // state should be set. Setting it here tells the Worker thread to stop normal polling.
        acceptor->_deviceState = DownloadStart;

        do
        {
            if (acceptor->_stopFlashDownloadThread)
            {
                acceptor->_deviceState = Idling;
				acceptor->_flashDownloadThread = NULL;
				delete params;
                return 0;
            }

			if (reply != NULL)
				delete reply;

            reply = acceptor->SendSynchronousCommand(payload, sizeof(payload));

            // If we are disconnected during the download process, we send a DownloadFinish
            // event reporting failure.
            if (!acceptor->_connected)
            {
                acceptor->RaiseDownloadFinishEvent(false);
                acceptor->_deviceState = Idling;
				acceptor->_flashDownloadThread = NULL;
				delete reply;
				delete params;
                return 0;
            }
        }
        while (reply->size() == 0);


        if (((*reply)[2] & 0x70) == 0x20)
        {
            char payload2[] = { CmdFlashDownload, 0x00, 0x00, 0x00 };

            do
            {
				if (reply != NULL)
					delete reply;

                reply = acceptor->SendSynchronousCommand(payload2, sizeof(payload2));

                if (reply->size() == 11)
                {
                    if (((*reply)[6] & 0x07) == 0x02)
                    {
                        acceptor->RaiseDownloadStartEvent(finalPacketNum);

                        break;
                    }
                }
            }
            while (true);
        }
        else
        {
            packetNum = ((((*reply)[3] & 0x0F) << 12) +
                         (((*reply)[4] & 0x0F) << 8) +
                         (((*reply)[5] & 0x0F) << 4) +
                         (((*reply)[6] & 0x0F) + 1)) & 0xFFFF;
        }
    }
    else
    {
        acceptor->_deviceState = DownloadStart;

        acceptor->RaiseDownloadStartEvent(finalPacketNum);
    }


	char payload3[69];

    payload3[0] = CmdFlashDownload;

    long timeoutStartTickCount = GetTickCount();
    
    do
    {
        if (acceptor->_stopFlashDownloadThread)
        {
            acceptor->_deviceState = Idling;
			acceptor->_flashDownloadThread = NULL;
			delete reply;
			delete params;
            return 0;
        }

        payload3[1] = (char)((packetNum & 0xF000) >> 12);
        payload3[2] = (char)((packetNum & 0x0F00) >> 8);
        payload3[3] = (char)((packetNum & 0x00F0) >> 4);
        payload3[4] = (char)(packetNum & 0x000F);

        char dataBuffer[32];

		params->downloadFile.seekg(packetNum * 32);

		params->downloadFile.read(dataBuffer, 32);

		if ((params->downloadFile.rdstate() & ios::failbit) == 0)
		{
			for (int i = 0; i < 32; i++)
			{
				payload3[5 + i * 2] = (char)((dataBuffer[i] & 0xF0) >> 4);
				payload3[6 + i * 2] = (char)(dataBuffer[i] & 0x0F);
			}
		}
		else
		{
			// What do we do???
		}

		if (reply != NULL)
			delete reply;

        reply = acceptor->SendSynchronousCommand(payload3, sizeof(payload3));

        if (reply->size() == 9 && acceptor->_isReplyAcked)
        {
            acceptor->_deviceState = Downloading;

            if (acceptor->_shouldRaiseDownloadProgressEvent)
                acceptor->RaiseDownloadProgressEvent(packetNum);

            packetNum++;
        }
        else
        {
			usleep(200 * MICRO_TO_MILLI);

            if (reply->size() == 9)
            {
                packetNum = ((((*reply)[3] & 0x0F) << 12) +
                             (((*reply)[4] & 0x0F) << 8) +
                             (((*reply)[5] & 0x0F) << 4) +
                             (((*reply)[6] & 0x0F) + 1)) & 0xFFFF;
            }
        }


        if (reply->size() > 0)
        {
            timeoutStartTickCount = GetTickCount();
        }
		else if ((GetTickCount() - timeoutStartTickCount) > CAcceptor::CommunicationDisconnectTimeout)
        {
            acceptor->RaiseDownloadFinishEvent(false);
            acceptor->_deviceState = Idling;
			acceptor->_flashDownloadThread = NULL;
			delete reply;
			delete params;
            return 0;
        }
    }
    while (packetNum < finalPacketNum);


    usleep(30000 * MICRO_TO_MILLI);

    params->downloadFile.close();


    bool wasStopped = false;

    acceptor->PollingLoop(acceptor, &reply, wasStopped);

	time_t currentTime;
	char timeBuffer[32];
	time(&currentTime);    	
	strftime(timeBuffer, sizeof(timeBuffer), "FlashDownload: %H:%M:%S", localtime(&currentTime));
	cout << timeBuffer << endl;
	
	
    if (wasStopped)
	{
		acceptor->_flashDownloadThread = NULL;
		delete reply;
		delete params;
	    return 0;
	}

    acceptor->ProcessReply(*reply);


    // Note that while we would expect the state to be set to Idling in the ProcessReply above,
    // it turns out that the Idling bit is not set in that initial reply, so we have to force it.
    acceptor->_deviceState = Idling;

    acceptor->QueryDeviceCapabilities();

    acceptor->SetUpBillTable();


    acceptor->RaiseDownloadFinishEvent(true);


    acceptor->_connected = true;
    acceptor->RaiseConnectedEvent();

	acceptor->_flashDownloadThread = NULL;

	delete params;

	return 0;
}


// -----------------------------------------------------------------------------------------------

vector<char>* CAcceptor::RawTransaction(char* command, long commandLength)
{
    vector<char>* reply = SendSynchronousCommand(command, commandLength);

    return reply;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ClearCashBoxTotal()
{
    VerifyConnected("ClearCashBoxTotal");


    char payload[] = { CmdAuxiliary, 0x00, 0x00, CmdAuxClearCashBoxTotal };

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    ProcessReply(*reply);
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::SetAssetNumber(string asset)
{
    VerifyConnected("SetAssetNumber");


	char payload[21] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    ConstructOmnibusCommand(payload, CmdExpanded, 2);

    payload[1] = 0x05;

    for (unsigned i = 0; i < asset.size() && i <= 16; i++)
        payload[i + 5] = asset[i];

    SendAsynchronousCommand(payload, sizeof(payload));
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::SetBezel(Bezel bezel)
{
    VerifyConnected("SetBezelNumber");


	char payload[4] = { CmdAuxiliary, bezel, 0x00, CmdAuxSetBezel };

    SendAsynchronousCommand(payload, sizeof(payload));
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::SoftReset()
{
    VerifyConnected("SoftReset");

    
    _docType = NoValue;
    

    char payload[] = { CmdAuxiliary, 0x7F, 0x7F, 0x7F };

    SendAsynchronousCommand(payload, sizeof(payload));

    _inSoftResetOneSecondIgnore = true;
    _inSoftResetWaitForReply = true;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::SpecifyEscrowTimeout(long billTimeout, long barcodeTimeout)
{
    VerifyConnected("SpecifyEscrowTimeout");


	char payload[7] = {0, 0, 0, 0, 0, 0, 0};

    ConstructOmnibusCommand(payload, CmdExpanded, 2);

    payload[1] = 0x04;
    payload[5] = (char)billTimeout;
    payload[6] = (char)barcodeTimeout;

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    ProcessReply(*reply);
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::SpecifyPupExt(char pupMode, PupExt preEscrow, PupExt atEscrow, PupExt postEscrow, PupExt preStack)
{
    VerifyConnected("SpecifyPupExt");


	char payload[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    ConstructOmnibusCommand(payload, CmdExpanded, 2);

    payload[1] = 0x07;
    payload[5] = (char)pupMode;
    payload[6] = GetPupExtValueCode(preEscrow);
    payload[7] = GetPupExtValueCode(atEscrow);
    payload[8] = GetPupExtValueCode(postEscrow);
    payload[9] = GetPupExtValueCode(preStack);

    vector<char>* reply = SendSynchronousCommand(payload, sizeof(payload));

    ProcessReply(*reply);
}


// -----------------------------------------------------------------------------------------------

string CAcceptor::DocumentTypeToString(DocumentType docType)
{
	switch (docType)
	{
	case None:
		return "None";
	case NoValue:
		return "No Value";
	case Bill:
		return "Bill";
	case Barcode:
		return "Barcode";
	case Coupon:
		return "Coupon";
	default:
		return "";
	}
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::Log(string message)
{
	if (!_logWriter.is_open())
	{
		return;
	}

	time_t currentTime;
	time(&currentTime);
	
	char timeBuffer[32];
	strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", localtime(&currentTime));

	stringstream line;

	line << timeBuffer << ": " << message.c_str() << "\n";

	_logWriter.write(line.str().c_str(), (streamsize)line.str().length());

	_logWriter.flush();
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::OpenLogFile()
{
	string shortPortName;
	
	stringstream logFilename;
	
	size_t slashPos = _devicePortName.rfind('/');
	
	if (slashPos != string::npos)
		shortPortName = _devicePortName.substr(slashPos + 1, string::npos);
	else
		shortPortName = _devicePortName;
	
	logFilename << _debugLogPath.c_str() << "MPOST_Log_" << shortPortName.c_str() << ".txt";
	
	_logWriter.open(logFilename.str().c_str(), ios_base::out | ios_base::ate | ios_base::app);
	_logWriter.seekp(0, ios::end); 

	
	time_t currentTime;
	time(&currentTime);	
	char date[32];
	strftime(date, sizeof(date), "%c", localtime(&currentTime));

	stringstream message;

	message << "M/POST version " << _version << " log opened " << date << ".";

	Log("--------------------------------------------------------------------------------");
    Log(message.str());
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::CloseLogFile()
{
    if (_workerThread != 0)
		_dataLinkLayer->FlushIdenticalTransactionsToLog();

    Log("Log closed.");

    _logWriter.close();
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::BuildHardCodedBillTable()
{
    switch (_deviceModel)
    {
        case 1:
        case 12:
        case 23:
        case 30:
        case 31:
        case 'J':
        case 'X':
#ifdef _DEBUG
		case 'T':
#endif
            {
				_billTypes.push_back(CBill("USD", 1, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 2, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 100, '*', '*', '*', '*'));
            }
            break;

        case 'P':
            {
				_billTypes.push_back(CBill("USD", 1, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 2, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 20, '*', '*', '*', '*'));
            }
            break;

        case 'G':
            {
				_billTypes.push_back(CBill());
				_billTypes.push_back(CBill("ARS", 2, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("ARS", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("ARS", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("ARS", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("ARS", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("ARS", 100, '*', '*', '*', '*'));
            }
            break;

        case 'A':
            {
				_billTypes.push_back(CBill("AUD", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("AUD", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("AUD", 100, '*', '*', '*', '*'));
            }
            break;

        case 15:
            {
				_billTypes.push_back(CBill());
				_billTypes.push_back(CBill());
				_billTypes.push_back(CBill("AUD", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("AUD", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("AUD", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("AUD", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("AUD", 100, '*', '*', '*', '*'));
            }
            break;

        case 'W':
            {
				_billTypes.push_back(CBill("BRL", 1, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("BRL", 2, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("BRL", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("BRL", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("BRL", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("BRL", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("BRL", 100, '*', '*', '*', '*'));
            }
            break;

        case 'C':
            {
				_billTypes.push_back(CBill());
				_billTypes.push_back(CBill());
				_billTypes.push_back(CBill("CAD", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("CAD", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("CAD", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("CAD", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("CAD", 100, '*', '*', '*', '*'));
            }
            break;

        case 'D':
            {
				_billTypes.push_back(CBill("EUR", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("EUR", 10, '*', '*', '*', '*'));
            }
            break;

        case 'M':
            {
				_billTypes.push_back(CBill("MXP", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("MXP", 50, '*', '*', '*', '*'));
            }
            break;

        case 'B':
            {
				_billTypes.push_back(CBill("RUR", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("RUR", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("RUR", 100, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("RUR", 500, '*', '*', '*', '*'));
            }
            break;

		default:
			{
				_billTypes.push_back(CBill("USD", 1, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 2, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 5, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 10, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 20, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 50, '*', '*', '*', '*'));
				_billTypes.push_back(CBill("USD", 100, '*', '*', '*', '*'));
            }
            break;


    }

    for (unsigned i = 0; i < _billTypes.size(); i++)
	{
		if (_billTypes[i]._value > 0)
	        _billTypeEnables.push_back(true);
		else
	        _billTypeEnables.push_back(false);
	}
}




// -----------------------------------------------------------------------------------------------

void CAcceptor::BuildBillValues()
{
    unsigned billValuesCount = 0;

    for (unsigned i = 0; i < _billTypes.size(); i++)
    {
        bool valueExists = false;

        for (unsigned j = 0; j < billValuesCount; j++)
        {
            if ((_billTypes[i]._value == _billValues[j]._value) && (_billTypes[i]._country == _billValues[j]._country))
            {
                valueExists = true;
                break;
            }
        }
        
        if (!valueExists)
        {
			_billValues.push_back(CBill(_billTypes[i]._country, _billTypes[i]._value, '*', '*', '*', '*'));
            billValuesCount++;

			_billValueEnables.push_back(_billTypes[i]._value > 0);
        }
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::SetUpBillTable()
{
	ClearBillTable();

    if (_expandedNoteReporting)
    {
    	time_t currentTime;
    	char timeBuffer[32];
    	time(&currentTime);    	
    	strftime(timeBuffer, sizeof(timeBuffer), "RetrieveBillTable: %H:%M:%S", localtime(&currentTime));
    	cout << timeBuffer << endl;

    	RetrieveBillTable();

    	time(&currentTime);    	
    	strftime(timeBuffer, sizeof(timeBuffer), "RetrieveBillTable: %H:%M:%S", localtime(&currentTime));
    	cout << timeBuffer << endl;
    }
    else
    {
        BuildHardCodedBillTable();
    }

    BuildBillValues();
}

// -----------------------------------------------------------------------------------------------

void CAcceptor::ConstructOmnibusCommand(char* payload, char controlCode, int data0Index)
{
    payload[0] = controlCode;

    if (_enableBookmarks && _enableAcceptance && _deviceState != Calibrating)
		payload[0] |= 0x20;


    char data0 = 0;

	if (_enableAcceptance && _deviceState != Calibrating)
    {
        if (_expandedNoteReporting)
        {
            // NOTE
            // This value can be anything OTHER than 0.
            data0 |= 0x7F;
        }
        else
        {
            // Under the current startup logic, the BillTypeEnables array is empty until
            // after the first poll reply is received, at which time it is created based
            // on the number of bills in the bill table.
            if (_billTypeEnables.size() == 0)
            {
                data0 |= 0x7F;
            }
            else
            {
                data0 = 0;

                for (unsigned i = 0; i < _billTypeEnables.size(); i++)
                {
                    int enableBit = 1 << i;

                    if (_billTypeEnables[i])
                        data0 |= (char)enableBit;
                }
            }
        }
    }


    char data1 = 0;

    // Ignore bit 0 since we are not supporting special interrupt mode.

    if (_highSecurity)
        data1 |= 0x02;

    switch (_orientationCtl)
    {
        case OneWay:
            // No bits to enable.
            break;

        case TwoWay:
            data1 |= 0x04;
            break;

        case FourWay:
            data1 |= 0x0C;
            break;
    }

    data1 |= 0x10; // Always enable escrow mode.



    char data2 = 0;

    if (_enableNoPush)
        data2 |= 0x01;

    if (_enableBarCodes && _enableAcceptance && _deviceState != Calibrating)
        data2 |= 0x02;

    switch (_devicePowerUp)
    {
        case A:
            // No bits to enable.
            break;

        case E:
            // Peter says to treat E like A for now.
            break;

        case B:
            data2 |= 0x04;
            break;

        case C:
            data2 |= 0x0C;
            break;
    }

    if (_expandedNoteReporting)
        data2 |= 0x10;

    if (_enableCouponExt && _capCouponExt)
        data2 |= 0x20;


    payload[data0Index] = data0;
    payload[data0Index + 1] = data1;
    payload[data0Index + 2] = data2;
}


// -----------------------------------------------------------------------------------------------

char CAcceptor::GetPupExtValueCode(PupExt pupExt)
{
    switch (pupExt)
    {
        case OutOfService:
            return 0;
        case StackNoCredit:
            return 1;
        case Return:
            return 2;
        case Stack:
            return 3;
        case WaitNoCredit:
            return 4;
        case Wait:
            return 5;

        // Arbitrary value. This should never happen.
        default:
            return 6;
    }
}



// -----------------------------------------------------------------------------------------------

CBill CAcceptor::ParseBillData(vector<char>& reply, unsigned extDataIndex)
{
	CBill bill;
	
    if (reply.size() < (extDataIndex + 14 + 1))
        return bill;

	char country[4];
	strncpy(country, &reply[extDataIndex + 1], 3);
	country[3] = 0;
	bill._country = country;

	char valueString[4];
	strncpy(valueString, &reply[extDataIndex + 4], 3);
	valueString[3] = 0;
	double billValue = atof(valueString);

    char exponentSign = reply[extDataIndex + 7];

	char exponentString[3];
	strncpy(exponentString, &reply[extDataIndex + 8], 2);
	exponentString[2] = 0;
    int exponent = atoi(exponentString);

    if (exponentSign == '+')
    {
        for (int i = 1; i <= exponent; i++)
            billValue *= 10.0;
    }
    else
    {
        for (int i = 1; i <= exponent; i++)
            billValue /= 10.0;
    }

	bill._value = billValue;

    _docType = Bill;

    _wasDocTypeSetOnEscrow = (_deviceState == Escrow);

    switch (reply[extDataIndex + 10])
    {
        case 0x00:
            _escrowOrientation = RightUp;
            break;

        case 0x01:
            _escrowOrientation = RightDown;
            break;

        case 0x02:
            _escrowOrientation = LeftUp;
            break;

        case 0x03:
            _escrowOrientation = LeftDown;
            break;
    }

    bill._type = reply[extDataIndex + 11];
    bill._series = reply[extDataIndex + 12];
    bill._compatibility = reply[extDataIndex + 13];
    bill._version = reply[extDataIndex + 14];
    
    
	return bill;
}

// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessReply(vector<char>& reply)
{
    if (reply.size() < 3)
	{
		delete &reply;
        return;
	}

    char ctl = reply[2];


    if ((ctl & 0x70) == 0x20)
        ProcessStandardOmnibusReply(reply);

    if ((ctl & 0x70) == 0x50)
    {
        _deviceState = DownloadRestart;
    }

    if ((ctl & 0x70) == 0x70)
    {
        char subType = reply[3];

        if (subType == 0x01)
        {
            ProcessExtendedOmnibusBarCodeReply(reply);
        }
        else if (subType == 0x02)
        {
            ProcessExtendedOmnibusExpandedNoteReply(reply);

            if (_deviceState == Escrow || (_deviceState == Stacked && !_wasDocTypeSetOnEscrow))
            {
            	_bill = ParseBillData(reply, 10);

                if (_capOrientationExt)
                {
                    if (_orientationCtlExt == OneWay)
                    {
                        if (_escrowOrientation != RightUp)
                        {
                            EscrowReturn();
                        }
                    }
                    else if (_orientationCtlExt == TwoWay)
                    {
                        if (_escrowOrientation != RightUp && _escrowOrientation != LeftUp)
                        {
                            EscrowReturn();
                        }
                    }
                    else if (_orientationCtlExt == FourWay)
                    {
                        // Accept all orientations.
                    }
                }
            }
        }
        else if (subType == 0x04)
        {
            ProcessExtendedOmnibusExpandedCouponReply(reply);
        }

        RaiseEvents();
    }


    if (_deviceState == Escrow && _autoStack)
    {
        EscrowStack();

        _shouldRaiseEscrowEvent = false;
    }


    // NOTE
    // I'm not 100% sure that this is the best place for this. However, after going through a full round of testing of
	// the C# version of this code, and then a full round of this C++ code testing, I do not think this is a going to be
	// a problem.
    if (_deviceState != Escrow && _deviceState != Stacking)
        _wasDocTypeSetOnEscrow = false;


	delete &reply;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessExtendedOmnibusBarCodeReply(vector<char>& reply)
{
    if (reply.size() < 38)
	{
        return;
	}

    // Set the capabilities before processing the other bytes.
    ProcessData4(reply[8]);

    ProcessData0(reply[4]);
    ProcessData1(reply[5]);
    ProcessData2(reply[6]);
    ProcessData3(reply[7]);
    ProcessData5(reply[9]);

    if (_deviceState == Escrow)
    {
        for (int i = 10; i < 38; i++)
        {
            if (reply[i] != '(')
				_barCode += reply[i];
            else
                break;
        }

        _docType = Barcode;
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessExtendedOmnibusExpandedNoteReply(vector<char>& reply)
{
    // Set the capabilities before processing the other bytes.
    ProcessData4(reply[8]);

    ProcessData0(reply[4]);
    ProcessData1(reply[5]);
    ProcessData2(reply[6]);
    ProcessData3(reply[7]);
    ProcessData5(reply[9]);
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessExtendedOmnibusExpandedCouponReply(vector<char>& reply)
{
    if (reply.size() < 15)
	{
        return;
	}

    // Set the capabilities before processing the other bytes.
    ProcessData4(reply[8]);

    ProcessData0(reply[4]);
    ProcessData1(reply[5]);
    ProcessData2(reply[6]);
    ProcessData3(reply[7]);
    ProcessData5(reply[9]);

    if (_deviceState == Escrow || (_deviceState == Stacked && !_wasDocTypeSetOnEscrow))
    {
        int couponData = ((reply[10] & 0x0F) << 12) +
                         ((reply[11] & 0x0F) << 8) +
                         ((reply[12] & 0x0F) << 4) +
                         ((reply[13] & 0x0F));

        double value = (couponData & 0x07);
        if (value == 3.0)
            value = 5.0;

        int ownerID = (couponData & 0xFFF8) >> 3;

		_coupon._ownerID = ownerID;
		_coupon._value = value;

        _docType = Coupon;

        _wasDocTypeSetOnEscrow = (_deviceState == Escrow);
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessStandardOmnibusReply(vector<char>& reply)
{
    if (reply.size() < 9)
	{
        return;
	}

    // Set the capabilities before processing the other bytes.
    ProcessData4(reply[7]);

    ProcessData0(reply[3]);
    ProcessData1(reply[4]);
    ProcessData2(reply[5]);
    ProcessData3(reply[6]);
    ProcessData5(reply[8]);


    RaiseEvents();
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessData0(char data0)
{
    if ((data0 & 0x0001) != 0)
        // Because the Idling bit and the Calibration bit can both be set at the same time
        // our design requires Calibration to take precedence.
        if (_deviceState != Calibrating && _deviceState != CalibrateStart)
            _deviceState = Idling;


    if ((data0 & 0x0002) != 0)
        // Because the Accepting bit and the Calibration bit can both be set at the same time
        // our design requires Calibration to take precedence.
        if (_deviceState != Calibrating && _deviceState != CalibrateStart)
            _deviceState = Accepting;


    if ((data0 & 0x0004) != 0)
    {
        _deviceState = Escrow;
        
        if (_autoStack)
        	_shouldRaiseEscrowEvent = false;
    }
    else
    {
        _shouldRaiseEscrowEvent = true;
    }

    if ((data0 & 0x0008) != 0)
        _deviceState = Stacking;

    if ((data0 & 0x0010) != 0)
    {
        _deviceState = Stacked;
    }
    else
    {
        _shouldRaiseStackedEvent = true;
    }

    if ((data0 & 0x0020) != 0)
        _deviceState = Returning;

    if ((data0 & 0x0040) != 0)
    {
        _deviceState = Returned;

		_bill = CBill();

        _docType = NoValue;
    }
    else
    {
        _shouldRaiseReturnedEvent = true;
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessData1(char data1)
{
    if ((data1 & 0x0001) != 0)
    {
        _isCheated = true;
    }
    else
    {
        _isCheated = false;
        _shouldRaiseCheatedEvent = true;
    }

    if ((data1 & 0x0002) != 0)
    {
        _deviceState = Rejected;
    }
    else
    {
        _shouldRaiseRejectedEvent = true;
    }


    if ((data1 & 0x0004) != 0)
    {
        _isDeviceJammed = true;
        _shouldRaiseJamClearedEvent = true;
    }
    else
    {
        _isDeviceJammed = false;
        _shouldRaiseJamDetectedEvent = true;
    }


    if ((data1 & 0x0008) != 0)
    {
        _cashBoxFull = true;
    }
    else
    {
        _cashBoxFull = false;
        _shouldRaiseStackerFullEvent = true;
    }


    _cashBoxAttached = ((data1 & 0x0010) != 0);
    
    if (!_cashBoxAttached)
    	_docType = NoValue;


    if ((data1 & 0x0020) != 0)
    {
        _devicePaused = true;
        _shouldRaisePauseClearedEvent = true;
    }
    else
    {
        _devicePaused = false;
        _shouldRaisePauseDetectedEvent = true;
    }


    if ((data1 & 0x0040) != 0)
    {
        _deviceState = Calibrating;

        if (_shouldRaiseCalibrateProgressEvent)
            RaiseCalibrateProgressEvent();
    }
    else
    {
        if (_deviceState == Calibrating)
        {
            _shouldRaiseCalibrateFinishEvent = true;
            _deviceState = Idling;
        }
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessData2(char data2)
{
    if (!_expandedNoteReporting)
    {
        int billTypeIndex = (data2 & 0x38) >> 3;

        if (billTypeIndex > 0)
        {
            // NOTE
            // Even though the bill data can be available anytime, it might be old, so
            // we only set it on Escrow, or on Stacked if it was not already set on Escrow.
            if (_deviceState == Escrow || (_deviceState == Stacked && !_wasDocTypeSetOnEscrow))
            {
            	_bill = _billTypes[billTypeIndex - 1];

                _docType = Bill;

                _wasDocTypeSetOnEscrow = (_deviceState == Escrow);
            }
        }
        else
        {
            if (_deviceState == Stacked || _deviceState == Escrow)
            {
                _bill = CBill();
                _docType = NoValue;
            }

            _wasDocTypeSetOnEscrow = false;
        }
    }
    else
    {
        if (_deviceState == Stacked)
        {
            if (_docType == Bill && _bill._value == 0.0)
            {
                _docType = NoValue;
            }
        }
        else if (_deviceState == Escrow)
        {
			_bill = CBill();
            _docType = NoValue;
        }
    }            
    

    if ((data2 & 0x0001) != 0)
    {
        _isPoweredUp = true;
        _docType = NoValue;
    }
    else
    {
        _shouldRaisePowerUpEvent = true;

        // If this is the very first poll reply, we need this flag to be carried over into
        // the next poll reply so that the PUPEscrow event can be sent, if applicable.
        if (!_isVeryFirstPoll)
            _isPoweredUp = false;
    }


    if ((data2 & 0x0002) != 0)
    {
        _isInvalidCommand = true;
    }
    {
        _isInvalidCommand = false;
        _shouldRaiseInvalidCommandEvent = true;
    }


    if ((data2 & 0x0004) != 0)
        _deviceState = Failed;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessData3(char data3)
{
    if ((data3 & 0x0001) != 0)
    {
        _deviceState = Stalled;
        _shouldRaiseStallClearedEvent = true;
    }
    else
    {
        _shouldRaiseStallDetectedEvent = true;
    }

    if ((data3 & 0x0002) != 0)
    {
        _deviceState = DownloadRestart;
    }

    if ((data3 & 0x0008) != 0)
    {
        _capBarCodesExt = true;
    }

    if ((data3 & 0x0010) != 0)
    {
        _isQueryDeviceCapabilitiesSupported = true;
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessData4(char data4)
{
    // NOTE
    // Theoretically the capabilities should never change, so we should only have to process this once. Consider
    // using a flag to ensure this.

    _deviceModel = data4 & 0x7F;

    // To make the code below easier to read.
    int d = _deviceModel;

    char m = (char)_deviceModel;

    _capApplicationPN        = m == 'T' || m == 'U';
    _capAssetNumber          = m == 'T' || m == 'U';
    _capAudit                = m == 'T' || m == 'U';
    _capBarCodes             = m == 'T' || m == 'U' || d == 15 || d == 23;
    _capBookmark             = true;
    _capBootPN               = m == 'T' || m == 'U';
    _capCalibrate            = true;
    _capCashBoxTotal         = m == 'A' || m == 'B' || m == 'C' || m == 'D' || m == 'G' || m == 'M' || m == 'P' || m == 'W' || m == 'X';
    _capCouponExt            = m == 'P' || m == 'X';
    _capDevicePaused         = m == 'P' || m == 'X' || d == 31;
    _capDeviceSoftReset      = m == 'A' || m == 'B' || m == 'C' || m == 'D' || m == 'G' || m == 'M' || m == 'P' || m == 'T' || m == 'U' || m == 'W' || m == 'X' || d == 31;
    _capDeviceType           = m == 'T' || m == 'U';
    _capDeviceResets         = m == 'A' || m == 'B' || m == 'C' || m == 'D' || m == 'G' || m == 'M' || m == 'P' || m == 'T' || m == 'U' || m == 'W' || m == 'X';
    _capDeviceSerialNumber   = m == 'T' || m == 'U';
    _capFlashDownload        = true;
    _capEscrowTimeout        = m == 'T' || m == 'U';
    _capNoPush               = m == 'P' || m == 'X' || d == 31 || d == 23;
    _capVariantPN            = m == 'T' || m == 'U';

#ifdef _DEBUG
    _expandedNoteReporting = m == 'T' || m == 'U'; // false
#else
    _expandedNoteReporting   = m == 'T' || m == 'U';
#endif
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::ProcessData5(char data5)
{
    if (    _deviceModel < 23 ||                         // S1K
            _deviceModel == 30 || _deviceModel == 31 ||   // S3K
            _deviceModel == 74 ||                        // CFMC
            _deviceModel == 84 || _deviceModel == 85      // CFSC
        )
    {
        _deviceRevision = data5 & 0x7F;
    }
    else // S2K
    {
        _deviceRevision = (data5 & 0x0F) + (data5 & 0x70) * 10;
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::QueryDeviceCapabilities()
{
	if (!_isQueryDeviceCapabilitiesSupported)
        return;

    char payload[] = { CmdAuxiliary, 0x00, 0x00, CmdAuxQueryDeviceCapabilities };

    vector<char> *reply = SendSynchronousCommand(payload, sizeof(payload));

    if (reply->size() < 4)
	{
		// TODO
		// Refactor delete statements;
		delete reply;
        return;
	}

    if (((*reply)[3] & 0x01) != 0)
        _capPupExt = true;

    if (((*reply)[3] & 0x02) != 0)
        _capOrientationExt = true;

    if (((*reply)[3] & 0x04) != 0)
    {
        _capApplicationID = true;
        _capVariantID = true;
    }

    if (((*reply)[3] & 0x08) != 0)
        _capBNFStatus = true;

    if (((*reply)[3] & 0x10) != 0)
        _capTestDoc = true;

	delete reply;
	}

// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseEvents()
{
    if (_isPoweredUp && _shouldRaisePowerUpEvent)
    {
        RaisePowerUpEvent();
        _shouldRaisePowerUpEvent = false;
    }

    // Suppress all events except PowerUp on the first poll, so we can retrieve capabilities
    // and set expanded mode (if available) before sending an Escrow event.
    if (_isVeryFirstPoll)
    {
        _isVeryFirstPoll = false;
        return;
    }

    switch (_deviceState)
    {
        case Escrow:
            if ((_isPoweredUp || !_connected) && _shouldRaisePUPEscrowEvent)
            {
                RaisePUPEscrowEvent();

                _shouldRaisePUPEscrowEvent = false;
            }
            else if (_shouldRaiseEscrowEvent)
            {
                RaiseEscrowEvent();

                _shouldRaiseEscrowEvent = false;
            }
            break;

        case Stacked:
            if (_shouldRaiseStackedEvent)
            {
                RaiseStackedEvent();
                _shouldRaiseStackedEvent = false;
            }
            break;

        case Returned:
            if (_shouldRaiseReturnedEvent)
            {
                RaiseReturnedEvent();
                _shouldRaiseReturnedEvent = false;
            }
            break;

        case Rejected:
            if (_shouldRaiseRejectedEvent)
            {
                RaiseRejectedEvent();
                _shouldRaiseRejectedEvent = false;
            }
            break;

        case Stalled:
            if (_shouldRaiseStallDetectedEvent)
            {
                RaiseStallDetectedEvent();
                _shouldRaiseStallDetectedEvent = false;
            }
            break;

        // NOTE
        // I've put these enum values here for the sole reason of suppressing a warning issued by g++. If you can figure out how to suppress that warning (just that
        // one, not all of them), then you can remove this code.
        case Accepting:
        case CalibrateStart:
        case Calibrating:
        case Connecting:
        case Disconnected:
        case Downloading:
        case DownloadRestart:
        case DownloadStart:
        case Failed:
        case Idling:
        case PupEscrow:
        case Returning:
        case Stacking:
        	break;
    }

    if (_deviceState != Stalled && _shouldRaiseStallClearedEvent)
    {
        RaiseStallClearedEvent();
        _shouldRaiseStallClearedEvent = false;
    }

    if (_cashBoxFull && _shouldRaiseStackerFullEvent)
    {
        RaiseStackerFullEvent();
        _shouldRaiseStackerFullEvent = false;
    }

    if (_isCheated && _shouldRaiseCheatedEvent)
    {
        RaiseCheatedEvent();
        _shouldRaiseCheatedEvent = false;
    }

    if (_cashBoxAttached && _shouldRaiseCashBoxAttachedEvent)
    {
        RaiseCashBoxAttachedEvent();
        _shouldRaiseCashBoxAttachedEvent = false;
        _shouldRaiseCashBoxRemovedEvent = true;
    }

    if (!_cashBoxAttached && _shouldRaiseCashBoxRemovedEvent)
    {
        RaiseCashBoxRemovedEvent();
        _shouldRaiseCashBoxRemovedEvent = false;
        _shouldRaiseCashBoxAttachedEvent = true;
    }

    if (_devicePaused && _shouldRaisePauseDetectedEvent)
    {
        RaisePauseDetectedEvent();
        _shouldRaisePauseDetectedEvent = false;
    }

    if (!_devicePaused && _shouldRaisePauseClearedEvent)
    {
        RaisePauseClearedEvent();
        _shouldRaisePauseClearedEvent = false;
    }

    if (_isDeviceJammed && _shouldRaiseJamDetectedEvent)
    {
        RaiseJamDetectedEvent();
        _shouldRaiseJamDetectedEvent = false;
    }

    if (!_isDeviceJammed && _shouldRaiseJamClearedEvent)
    {
        RaiseJamClearedEvent();
        _shouldRaiseJamClearedEvent = false;
    }

    if (_isInvalidCommand & _shouldRaiseInvalidCommandEvent)
    {
		RaiseInvalidCommandEvent();
        _shouldRaiseInvalidCommandEvent = false;
    }

    if (_shouldRaiseCalibrateFinishEvent)
    {
        RaiseCalibrateFinishEvent();
    }
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseCalibrateStartEvent()
{
    Log("EVNT: CalibrateStart");

    if (_eventHandlers[CalibrateStartEvent])
    	_eventHandlers[CalibrateStartEvent](this, 0);
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseCalibrateProgressEvent()
{
    Log("EVNT: CalibrateProgress");

    if (_eventHandlers[CalibrateProgressEvent])
    	_eventHandlers[CalibrateProgressEvent](this, 0);

    _shouldRaiseCalibrateProgressEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseCalibrateFinishEvent()
{
    Log("EVNT: CalibrateFinish");

    if (_eventHandlers[CalibrateFinishEvent])
    	_eventHandlers[CalibrateFinishEvent](this, 0);

    _shouldRaiseCalibrateFinishEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseConnectedEvent()
{
    Log("EVNT: Connected");
 
    if (_eventHandlers[ConnectedEvent])
    	_eventHandlers[ConnectedEvent](this, 0);

    _shouldRaiseConnectedEvent = false;
    _shouldRaiseDisconnectedEvent = true;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseDisconnectedEvent()
{
    Log("EVNT: Disconnected");

    if (_eventHandlers[DisconnectedEvent])
    	_eventHandlers[DisconnectedEvent](this, 0);

    _shouldRaiseDisconnectedEvent = false;
    _shouldRaiseConnectedEvent = true;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseEscrowEvent()
{
    Log("EVNT: Escrow");

    if (_eventHandlers[EscrowEvent])
    	_eventHandlers[EscrowEvent](this, 0);

    _shouldRaiseEscrowEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaisePUPEscrowEvent()
{
    Log("EVNT: PUP Escrow");

    if (_eventHandlers[PUPEscrowEvent])
    	_eventHandlers[PUPEscrowEvent](this, 0);

    _shouldRaiseEscrowEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseReturnedEvent()
{
    Log("EVNT: Returned");

    if (_eventHandlers[ReturnedEvent])
    	_eventHandlers[ReturnedEvent](this, 0);

    _shouldRaiseReturnedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseStackedEvent()
{
    Log("EVNT: Stacked");

    if (_eventHandlers[StackedEvent])
    	_eventHandlers[StackedEvent](this, 0);

    _shouldRaiseStackedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseRejectedEvent()
{
    Log("EVNT: Rejected");

    if (_eventHandlers[RejectedEvent])
    	_eventHandlers[RejectedEvent](this, 0);

    _shouldRaiseRejectedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseStallDetectedEvent()
{
    Log("EVNT: Stall Detected");

    if (_eventHandlers[StallDetectedEvent])
    	_eventHandlers[StallDetectedEvent](this, 0);

    _shouldRaiseStallDetectedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseStallClearedEvent()
{
    Log("EVNT: Stall Cleared");

    if (_eventHandlers[StallClearedEvent])
    	_eventHandlers[StallClearedEvent](this, 0);

    _shouldRaiseStallClearedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaisePauseDetectedEvent()
{
    Log("EVNT: Pause Detected");

    if (_eventHandlers[PauseDetectedEvent])
    	_eventHandlers[PauseDetectedEvent](this, 0);

    _shouldRaisePauseDetectedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaisePauseClearedEvent()
{
    Log("EVNT: Pause Cleared");

    if (_eventHandlers[PauseClearedEvent])
    	_eventHandlers[PauseClearedEvent](this, 0);

    _shouldRaisePauseClearedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseJamDetectedEvent()
{
    Log("EVNT: Jam Detected");

    if (_eventHandlers[JamDetectedEvent])
    	_eventHandlers[JamDetectedEvent](this, 0);

    _shouldRaiseJamDetectedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseJamClearedEvent()
{
    Log("EVNT: Jam Cleared");

    if (_eventHandlers[JamClearedEvent])
    	_eventHandlers[JamClearedEvent](this, 0);

    _shouldRaiseJamClearedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseStackerFullEvent()
{
    Log("EVNT: Stacker Full");

    if (_eventHandlers[StackerFullEvent])
    	_eventHandlers[StackerFullEvent](this, 0);

    _shouldRaiseStackerFullEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseCheatedEvent()
{
    Log("EVNT: Cheated");

    if (_eventHandlers[CheatedEvent])
    	_eventHandlers[CheatedEvent](this, 0);

    _shouldRaiseCheatedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseCashBoxAttachedEvent()
{
    Log("EVNT: Cash Box Attached");

    if (_eventHandlers[CashBoxAttachedEvent])
    	_eventHandlers[CashBoxAttachedEvent](this, 0);

    _shouldRaiseCashBoxAttachedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseCashBoxRemovedEvent()
{
    Log("EVNT: Cash Box Removed");

    if (_eventHandlers[CashBoxRemovedEvent])
    	_eventHandlers[CashBoxRemovedEvent](this, 0);

    _shouldRaiseCashBoxRemovedEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaisePowerUpEvent()
{
    Log("EVNT: Power Up");

    if (_eventHandlers[PowerUpEvent])
    	_eventHandlers[PowerUpEvent](this, 0);

    _shouldRaisePowerUpEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseInvalidCommandEvent()
{
    Log("EVNT: Invalid Command");

    if (_eventHandlers[InvalidCommandEvent])
    	_eventHandlers[InvalidCommandEvent](this, 0);

    _shouldRaiseInvalidCommandEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseDownloadRestartEvent()
{
    Log("EVNT: Download Restart");

    if (_eventHandlers[DownloadRestartEvent])
    	_eventHandlers[DownloadRestartEvent](this, 0);

    _shouldRaiseDownloadRestartEvent = false;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseDownloadStartEvent(int sectorCount)
{
    Log("EVNT: Download Start");

    if (_eventHandlers[DownloadStartEvent])
    	_eventHandlers[DownloadStartEvent](this, sectorCount);

    _shouldRaiseDownloadStartEvent = false;
    _shouldRaiseDownloadProgressEvent = true;
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseDownloadProgressEvent(int sectorCount)
{
    Log("EVNT: Download Progress");

    if (_eventHandlers[DownloadProgressEvent])
    	_eventHandlers[DownloadProgressEvent](this, sectorCount);
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RaiseDownloadFinishEvent(bool success)
{
    Log("EVNT: Download Finish");

    if (_eventHandlers[DownloadFinishEvent])
    	_eventHandlers[DownloadFinishEvent](this, success);

    _shouldRaiseDownloadFinishEvent = false;
}



// -----------------------------------------------------------------------------------------------

void CAcceptor::ClearBillTable()
{

	_billTypes.clear();

	_billTypeEnables.clear();

	_billValues.clear();

	_billValueEnables.clear();
}


// -----------------------------------------------------------------------------------------------

void CAcceptor::RetrieveBillTable()
{
    int index = 1;

    while (true)
    {
        char payload[6];

        ConstructOmnibusCommand(payload, CmdExpanded, 2);

        payload[1] = 0x02;
        payload[5] = (char)index;

        vector<char>* reply;

        do
        {
            reply = SendSynchronousCommand(payload, sizeof(payload));

            if (reply->size() == 30)
                break;

			delete reply;

            usleep(100000); // Arbitrary value.
        }
        while (true);

        char ctl = (*reply)[2];

        if (((ctl & 0x70) != 0x70) || ((*reply)[3] != 0x02)) // reply[3] == Sub Type
        {
            break;
        }

        if ((*reply)[10] == 0)
		{
			delete reply;
            break;
		}


        CBill billFromTable = ParseBillData(*reply, 10);

		delete reply;

        _billTypes.push_back(billFromTable);

        index++;
    }

    for (unsigned i = 0; i < _billTypes.size(); i++)
        _billTypeEnables.push_back(true);
}

// -----------------------------------------------------------------------------------------------

void CAcceptor::SendAsynchronousCommand(char* payload, int payloadLength)
{
    assert(_replyQueue.size() == 0);

    _messageQueue.push_back(new CMessage(payload, payloadLength, false));
}

// -----------------------------------------------------------------------------------------------

vector<char>* CAcceptor::SendSynchronousCommand(char* payload, int payloadLength)
{
	assert(_replyQueue.size() == 0);

// TODO
// Figure out why the thread synchronization causes FlashDownload to fail. Other than that, the 
// follow statements seem to work.
//	pthread_mutex_lock( &mutex1 );
		  
	_messageQueue.push_back(new CMessage(payload, payloadLength, true));
	
//	pthread_cond_wait(&condition1, &mutex1);
	
//	pthread_mutex_unlock( &mutex1 );

	int thirtySecondTimer = 0;
	
	while (_replyQueue.empty())
	{
		usleep(10000);
		
		if (thirtySecondTimer++ == 3000000)
			break;
	}

	if (!_replyQueue.empty())
	{
		vector<char>* reply = _replyQueue.front();

		_replyQueue.pop_front();

		return reply;
	}
    else
    {
		assert(false);

		return new vector<char>;
    }
}


// -----------------------------------------------------------------------------------------------
// NOTE
// Note that we call this functions only for the properties that require a command to be sent
// to the bill acceptor, to prevent the user from sending an invalid command, which could cause
// a problem. For properties that just return the value of a variable, there is not need to be
// so strict because they are harmless.
void CAcceptor::VerifyPropertyIsAllowed(bool capabilityFlag, string propertyName)
{
    if (!_connected)
	{
		stringstream ss;
		ss << "Calling " << propertyName << " not allowed when not connected.";
		
		throw CAcceptorException(ss.str());
	}

    if (!capabilityFlag)
	{
		stringstream ss;
		ss << "Device does not support " << propertyName << ".";
		
		throw CAcceptorException(ss.str());
	}

    if (_deviceState == DownloadStart || _deviceState == Downloading)
	{
		stringstream ss;
		ss << "Calling " << propertyName << " not allowed during flash download.";
		
		throw CAcceptorException(ss.str());
	}

    if (_deviceState == CalibrateStart || _deviceState == Calibrating)
	{
		stringstream ss;
		ss << "Calling " << propertyName << " not allowed during calibration.";
		
		throw CAcceptorException(ss.str());
	}
}


void CAcceptor::VerifyConnected(string functionName)
{
    if (!_connected)
	{
		stringstream ss;
		ss << "Calling " << functionName << " not allowed when not connected.";
		
		throw CAcceptorException(ss.str());
	}
}


// -----------------------------------------------------------------------------------------------

char* CAcceptor::itoa(int val, char* buffer, int base)
{
	static char tmpBuffer[32];
	
	int i = 30;
	
	for(; val && i ; --i, val /= base)
	{
		tmpBuffer[i] = "0123456789abcdef"[val % base];
	}
	
	if (i == 30)
	{
		tmpBuffer[i] = '0';
		i--;
	}
	if (i == 29)
	{
		tmpBuffer[i] = '0';
		i--;
	}
	
	strcpy(buffer, &tmpBuffer[i + 1]);
	
	return buffer;
}




}

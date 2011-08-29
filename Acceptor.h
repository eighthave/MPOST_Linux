#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include <string>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <deque>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <sys/times.h>

#include "Bill.h"
#include "Coupon.h"

using namespace std;


// These defines are intended to highlight a difference between the Linux timing and Windows timing, microseconds vs milliseconds.
#define MICRO_TO_MILLI	1000 



namespace MPOST
{

class CDataLinkLayer;


enum BNFStatus
{
    Unknown,
    OK,
    NotAttached,
    Error
};

enum DocumentType
{
    None,
    NoValue,
    Bill,
    Barcode,
    Coupon
};

enum Orientation
{
    RightUp,
    RightDown,
    LeftUp,
    LeftDown,
    UnknownOrientation
};

enum OrientationControl
{
    FourWay,
    TwoWay,
    OneWay
};

enum PowerUp
{
    A,
    B,
    C,
    E
};

enum PupExt
{
    Return,
    OutOfService,
    StackNoCredit,
    Stack,
    WaitNoCredit,
    Wait
};

enum State
{
    Disconnected,
    Connecting,
    PupEscrow,
    Idling,
    Accepting,
    Escrow,
    Stacking,
    Stacked,
    Returning,
    Returned,
    Rejected,
    Stalled,
    Failed,
    CalibrateStart,
    Calibrating,
    DownloadStart,
    DownloadRestart,
    Downloading,
};

enum Bezel
{
    Standard,
    Platform,
    Diagnostic
};

enum Event
{
	Events_Begin,
	ConnectedEvent,
	EscrowEvent,
	PUPEscrowEvent,
	StackedEvent,
	ReturnedEvent,
	RejectedEvent,
	CheatedEvent,
	StackerFullEvent,
	CalibrateStartEvent,
	CalibrateProgressEvent,
	CalibrateFinishEvent,
	DownloadStartEvent,
	DownloadRestartEvent,
	DownloadProgressEvent,
	DownloadFinishEvent,
	PauseDetectedEvent,
	PauseClearedEvent,
	StallDetectedEvent,
	StallClearedEvent,
	JamDetectedEvent,
	JamClearedEvent,
	PowerUpEvent,
	InvalidCommandEvent,
	CashBoxAttachedEvent,
	CashBoxRemovedEvent,
	DisconnectedEvent,
	Events_End
};


// PURPOSE
// Stores a length and a synchronous flag along with a message payload. This is a struct
// instead of a class because I am not trying to implement strict encapsulation. I just want
// to be able to pass these three data values together easily. Note that we could
// avoid need for a variable to store the message length of we used a vector, but this
// is just as easy.
struct CMessage
{
	CMessage(char* payload, int payloadLength, bool isSynchronous)
    {
		memcpy(_payload, payload, 128);
		_payloadLength = payloadLength;
        _isSynchronous = isSynchronous;
    }

    char _payload[128];

	int _payloadLength;

    // If true, then the reply to the message should be posted to the reply queue because the main Acceptor
    // thread will be (blocked) waiting for it.
    bool _isSynchronous;
};


class CAcceptorException
{
public:
	CAcceptorException(string description = "")
	{
		_description = description;
	}
	
	string GetDescription()
	{
		return _description;
	}

private:
	string	_description;
};


class CAcceptor
{
	friend class CWorker;
	friend class CDataLinkLayer;
	friend class CSuppressStandardPoll;

public:
	CAcceptor();


	void Open(string portName, PowerUp powerUp);	
	void Close();
	void Calibrate();
	void EscrowReturn();
	void EscrowStack();
	void FlashDownload(string filePath);
	void ClearCashBoxTotal();
	vector<char>* RawTransaction(char* command, long commandLength);
	void SetAssetNumber(string asset);
	void SetBezel(Bezel bezel);
	void SoftReset();
	void SpecifyEscrowTimeout(long billTimeout, long barcodeTimeout);
	void SpecifyPupExt(char pupMode, PupExt preEscrow, PupExt atEscrow, PupExt postEscrow, PupExt preStack);
	
		
	string GetApplicationID();
	string GetApplicationPN();
	vector<int> GetAuditLifeTimeTotals();
	vector<int> GetAuditPerformance();
	vector<int> GetAuditQP();
	bool GetAutoStack();
	void SetAutoStack(bool newVal);
	string GetBarCode();
	CBill GetBill();
	vector<CBill> GetBillTypes();
	vector<bool> GetBillTypeEnables();
	void SetBillTypeEnables(vector<bool>);
	vector<CBill> GetBillValues();
	vector<bool> GetBillValueEnables();
	void SetBillValueEnables(vector<bool>);
	BNFStatus GetBNFStatus();
	string GetBootPN();
	bool GetCapApplicationID();
	bool GetCapApplicationPN();
	bool GetCapAssetNumber();
	bool GetCapAudit();
	bool GetCapBarCodes();
	bool GetCapBarCodesExt();
	bool GetCapBNFStatus();
	bool GetCapBookmark();
	bool GetCapBootPN();
	bool GetCapCalibrate();
	bool GetCapCashBoxTotal();
	bool GetCapCouponExt();
	bool GetCapDevicePaused();
	bool GetCapDeviceSoftReset();
	bool GetCapDeviceType();
	bool GetCapDeviceResets();
	bool GetCapDeviceSerialNumber();
	bool GetCapEscrowTimeout();
	bool GetCapFlashDownload();
	bool GetCapNoPush();
	bool GetCapOrientationExt();
	bool GetCapPupExt();
	bool GetCapTestDoc();
	bool GetCapVariantID();
	bool GetCapVariantPN();
	bool GetCashBoxAttached();
	bool GetCashBoxFull();
	long GetCashBoxTotal();
	bool GetConnected();
	CCoupon GetCoupon();
	bool GetDebugLog();
	void SetDebugLog(bool newVal);
	string GetDebugLogPath();
	void SetDebugLogPath(string newVal);
	bool GetDeviceBusy();
	long GetDeviceCRC();
	bool GetDeviceFailure();
	bool GetDeviceJammed();
	long GetDeviceModel();
	bool GetDevicePaused();
	string GetDevicePortName();
	PowerUp GetDevicePowerUp();
	long GetDeviceResets();
	long GetDeviceRevision();
	string GetDeviceSerialNumber();
	bool GetDeviceStalled();
	State GetDeviceState();
	string GetDeviceType();
	DocumentType GetDocType();
	bool GetEnableAcceptance();
	void SetEnableAcceptance(bool newVal);
	bool GetEnableBarCodes();
	void SetEnableBarCodes(bool newVal);
	bool GetEnableBookmarks();
	void SetEnableBookmarks(bool newVal);
	bool GetEnableCouponExt();
	void SetEnableCouponExt(bool newVal);
	bool GetEnableNoPush();
	void SetEnableNoPush(bool newVal);
	Orientation GetEscrowOrientation();
	bool GetHighSecurity();
	void SetHighSecurity(bool newVal);
	OrientationControl GetOrientationControl();
	void SetOrientationControl(OrientationControl newVal);
	OrientationControl GetOrientationCtlExt();
	void SetOrientationCtlExt(OrientationControl newVal);
	vector<string> GetVariantNames();
	string GetVariantID();
	string GetVariantPN();
	string GetVersion();
	int GetTransactionTimeout();
	void SetTransactionTimeout(int newVal);
	int GetDownloadTimeout();
	void SetDownloadTimeout(int newVal);

	void SetEventHandler(Event event, void(*eventHandler)(CAcceptor* acceptor, int value))
	{
		_eventHandlers[event] = eventHandler;	
	}
	
	static string DocumentTypeToString(DocumentType docType);
	
    // NOTE
	// I put this function here because I did not want to create a utility class to host just one function.
	static char* itoa(int val, char* buffer, int base);
	
private:
	void(*_eventHandlers[28])(CAcceptor*, int);
	
	bool OpenPort();
	
	void ProcessReply(vector<char>& reply);

	void ProcessStandardOmnibusReply(vector<char>& reply);
	void ProcessExtendedOmnibusBarCodeReply(vector<char>& reply);
	void ProcessExtendedOmnibusExpandedNoteReply(vector<char>& reply);
	void ProcessExtendedOmnibusExpandedCouponReply(vector<char>& reply);

	void ProcessData0(char data0);
	void ProcessData1(char data1);
	void ProcessData2(char data2);
	void ProcessData3(char data3);
	void ProcessData4(char data4);
	void ProcessData5(char data5);

	CBill ParseBillData(vector<char>& reply, unsigned extDataIndex);
	
	void RaiseEvents();

	void RaiseCalibrateStartEvent();
	void RaiseCalibrateProgressEvent();
	void RaiseCalibrateFinishEvent();
	void RaiseConnectedEvent();
	void RaiseDisconnectedEvent();
	void RaiseEscrowEvent();
	void RaisePUPEscrowEvent();
	void RaiseReturnedEvent();
	void RaiseStackedEvent();
	void RaiseRejectedEvent();
	void RaiseStallDetectedEvent();
	void RaiseStallClearedEvent();
	void RaisePauseDetectedEvent();
	void RaisePauseClearedEvent();
	void RaiseJamDetectedEvent();
	void RaiseJamClearedEvent();
	void RaiseStackerFullEvent();
	void RaiseCheatedEvent();
	void RaiseCashBoxAttachedEvent();
	void RaiseCashBoxRemovedEvent();
	void RaisePowerUpEvent();
	void RaiseInvalidCommandEvent();
	void RaiseDownloadRestartEvent();
	void RaiseDownloadStartEvent(int sectorCount);
	void RaiseDownloadProgressEvent(int sectorCount);
	void RaiseDownloadFinishEvent(bool success);

	char GetPupExtValueCode(PupExt pupExt);

	void QueryDeviceCapabilities();

	void ConstructOmnibusCommand(char* payload, char controlCode, int data0Index);
	void SendAsynchronousCommand(char* payload, int payloadLength);
	vector<char>* SendSynchronousCommand(char* payload, int payloadLength);

	static void* OpenThread(void* param);
	static void* FlashDownloadThread(void* param);
	static void PollingLoop(CAcceptor* acceptor, vector<char>** reply, bool &wasStopped);

	void VerifyPropertyIsAllowed(bool capabilityFlag, string propertyName);
	void VerifyConnected(string functionName);

	void OpenLogFile();
	void CloseLogFile();

	void RetrieveBillTable();
	void SetUpBillTable();
	void BuildBillValues();
	void BuildHardCodedBillTable();
	void ClearBillTable();

	void SetGetLastErrorDescription();

	
	
	

    static const int CmdOmnibus                                   = 0x10;
    static const int CmdCalibrate                                 = 0x40;
    static const int CmdFlashDownload                             = 0x50;
    static const int CmdAuxiliary                                 = 0x60;
    static const int CmdExpanded                                  = 0x70;

    static const int CmdAuxQuerySoftwareCRC                       = 0x00;
    static const int CmdAuxQueryCashBoxTotal                      = 0x01;
    static const int CmdAuxQueryDeviceResets                      = 0x02;
    static const int CmdAuxClearCashBoxTotal                      = 0x03;
    static const int CmdAuxQueryAcceptorType                      = 0x04;
    static const int CmdAuxQueryAcceptorSerialNumber              = 0x05;
    static const int CmdAuxQueryAcceptorBootPartNumber            = 0x06;
    static const int CmdAuxQueryAcceptorApplicationPartNumber     = 0x07;
    static const int CmdAuxQueryAcceptorVariantName               = 0x08;
    static const int CmdAuxQueryAcceptorVariantPartNumber         = 0x09;
    static const int CmdAuxQueryAcceptorAuditLifeTimeTotals       = 0x0A;
    static const int CmdAuxQueryAcceptorAuditQPMeasures           = 0x0B;
    static const int CmdAuxQueryAcceptorAuditPerformanceMeasures  = 0x0C;
    static const int CmdAuxQueryDeviceCapabilities                = 0x0D;
    static const int CmdAuxQueryAcceptorApplicationID             = 0x0E;
    static const int CmdAuxQueryAcceptorVariantID                 = 0x0F;
    static const int CmdAuxQueryBNFStatus                         = 0x10;
    static const int CmdAuxSetBezel		                          = 0x11;


    int _port;

    // NOTE
    // These variables correspond to properties.
    //
    vector<int>					_auditLifeTimeTotals;
    vector<int>					_auditPerformance;
    vector<int>					_auditQP;
    bool                _autoStack;
    string						_barCode;

	// NOTE
	// We create one Bill instance at startup and repeatedly set its properties when bills are escrowed. However, the
	// Bill object we give back to the host application is a new instance.
    CBill						_bill;

	vector<CBill>       _billTypes;
    vector<bool>		_billTypeEnables;
    vector<CBill>       _billValues;
    vector<bool>		_billValueEnables;
    BNFStatus			_bnfStatus;
    string				_bootPN;
    bool                _capApplicationID;
    bool                _capApplicationPN;
    bool                _capAssetNumber;
    bool                _capAudit;
    bool                _capBarCodes;
    bool                _capBarCodesExt;
    bool                _capBNFStatus;
    bool                _capBookmark;
    bool                _capBootPN;
    bool                _capCalibrate;
    bool                _capCashBoxTotal;
    bool                _capCouponExt;
    bool                _capDevicePaused;
    bool                _capDeviceSoftReset;
    bool                _capDeviceType;
    bool                _capDeviceResets;
    bool                _capDeviceSerialNumber;
    bool                _capEscrowTimeout;
    bool                _capFlashDownload;
    bool                _capNoPush;
    bool                _capOrientationExt;
    bool                _capPupExt;
    bool                _capTestDoc;
    bool                _capVariantID;
    bool                _capVariantPN;
    bool                _cashBoxAttached;
    bool                _cashBoxFull;
    int					_cashBoxTotal;
    bool				_connected;
    CCoupon				_coupon;
    bool				_debugLog;
    string				_debugLogPath;
    bool				_deviceFailure;
    int					_deviceModel;
    bool				_devicePaused;
    string				_devicePortName;
    PowerUp				_devicePowerUp;
    int					_deviceResets;
    int					_deviceRevision;
    string				_deviceSerialNumber;
    bool				_deviceStalled;
    State				_deviceState;
    string				_deviceType;
    DocumentType		_docType;
    bool                _enableAcceptance;
    bool                _enableBarCodes;
    bool                _enableBookmarks;
    bool                _enableCouponExt;
    bool                _enableNoPush;
    Orientation			_escrowOrientation;
    bool                _highSecurity;
    OrientationControl	_orientationCtl;
    OrientationControl	_orientationCtlExt;
	string				_version;
	int					_transactionTimeout;
	int					_downloadTimeout;

    // For a soft reset, the spec details very specific behaviour for processing the possible
    // reply and resume normal operation. These flags triggers that behaviour in Worker and DataLinkLayer.
    bool _inSoftResetOneSecondIgnore;
    bool _inSoftResetWaitForReply;

    // NOTE
    // This is not in the spec, but I was told that if the BA model supports expanded note
    // reporting (T or U), then M/POST will use it. The host will not have the option
    // to use terse mode if expanded is available.
    bool _expandedNoteReporting;

    bool _isQueryDeviceCapabilitiesSupported;

    // NOTE
    // Not all the conditions reported by the BA have associated states. For the ones that don't,
    // we use a separate flag, but only so we can raise an event later. The client will not have
    // access to this flag
    bool _isDeviceJammed;
    bool _isCheated;
    bool _isPoweredUp;
    bool _isInvalidCommand;

    bool _wasDocTypeSetOnEscrow;

    // This flag is set to true when communication is lost (30 sec w/o a reply) AFTER having been
    // established to begin with (via the Open method). When the worker thread receives a reply
    // and this flag is true, it should raise a Connected event.
    bool _wasDisconnected;

    // This flag is only true when the application starts up and only until it receives a 
    // to its very first poll, an "empty" poll used to retrieve capabilities. The flag is used
    // to suppress the raising of events, except PowerUp, for that first poll cycle.
    bool _isVeryFirstPoll;

    
    // We only want to fire event the first time the reply indicates that the particular condition exists. Even
    // though the reply will continue to indicate that condition, we will not fire the event again until the condition
    // actually occurs again, and we determine this by detecting when the condition switches back to non-existent
    // and then back. Sorry for the convoluted explanation. I'm hungry and these mini candies aren't doing the trick.
    bool _shouldRaiseConnectedEvent;
    bool _shouldRaiseEscrowEvent;
    bool _shouldRaisePUPEscrowEvent;
    bool _shouldRaiseStackedEvent;
    bool _shouldRaiseReturnedEvent;
    bool _shouldRaiseRejectedEvent;
    bool _shouldRaiseCheatedEvent;
    bool _shouldRaiseStackerFullEvent;
    bool _shouldRaiseCalibrateStartEvent;
    bool _shouldRaiseCalibrateProgressEvent;
    bool _shouldRaiseCalibrateFinishEvent;
    bool _shouldRaiseDownloadStartEvent;
    bool _shouldRaiseDownloadRestartEvent;
    bool _shouldRaiseDownloadProgressEvent;
    bool _shouldRaiseDownloadFinishEvent;
    bool _shouldRaisePauseDetectedEvent;
    bool _shouldRaisePauseClearedEvent;
    bool _shouldRaiseStallDetectedEvent;
    bool _shouldRaiseStallClearedEvent;
    bool _shouldRaiseJamDetectedEvent;
    bool _shouldRaiseJamClearedEvent;
    bool _shouldRaisePowerUpEvent;
    bool _shouldRaiseInvalidCommandEvent;

    // NOTE
    // I am guessing at this logic. Since we expect the cassette to be attached at startup, we do not want to fire
    // an event right away, but if the cassette is not attached at startup, we do fire an event.
    bool _shouldRaiseCashBoxAttachedEvent;
    bool _shouldRaiseCashBoxRemovedEvent;

    bool _shouldRaiseDisconnectedEvent;


    // This is a hidden option, not in the spec.
    bool _compressLog;

	ofstream _logWriter;

	void Log(string message);

	pthread_t _workerThread;
	pthread_t _openThread;
	pthread_t _flashDownloadThread;

	CDataLinkLayer*	_dataLinkLayer;

    int _replyQueuedEvent;

    // Currently, this event is necessary only because of one function, Calibrate, which changes
    // Acceptor._deviceState. This change can screw up ProcessReply, so Calibrate needs to waits for
    // ProcessReply to complete before proceeding.
    int _notInProcessReplyEvent;

    bool _stopWorkerThread;
    bool _stopOpenThread;
    bool _stopFlashDownloadThread;

    bool _suppressStandardPoll;

	// NOTE
	// While not 100% sure, I am reasonably sure that these queues do not need to be synchronized
	// because they are only accessed by a single producer and a single consumer. See the following
	// article:
	// http://www.ddj.com/cpp/184401814

	// NOTE
	// I am using deque instead of queue for one reason--it supports the clear method.
	deque<CMessage*>		_messageQueue;
	deque<vector<char>*>	_replyQueue;


    bool _isReplyAcked;


    static const char C_R_N[];

    static const int CommunicationDisconnectTimeout = 3000;
    static const int PollingDisconnectTimeout       = 3000;
    static const int CalibrateTimeout               = 3000;


    // Note that this event is used in multiple contexts. Is it acceptable to reuse an event like
    // like this? Care must be taken that all uses are mutually exclusive.
    int _signalMainThreadEvent;

	static long GetTickCount()
	{
	    tms tm;
	    return times(&tm);
	}
};

// PURPOSE
// This class ensures that suppressStandardPoll is set back to false when set by FlashDownload, Calibrate, or any other
// function that uses it. With so many code paths in FlashDownload, it would be easy to introduce a bug by forgetting
// to set it back to false.
class CSuppressStandardPoll
{
public:
    CSuppressStandardPoll(CAcceptor* acceptor)
    {
        _acceptor = acceptor;
        _acceptor->_suppressStandardPoll = true;
    }

    ~CSuppressStandardPoll()
    {
        _acceptor->_suppressStandardPoll = false;
    }

    CAcceptor* _acceptor;
};

};


#endif /*ACCEPTOR_H_*/

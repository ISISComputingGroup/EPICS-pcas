/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 *
 */

/*
 * ANSI C
 */
#include <string.h>

/*
 * EPICS
 */
#include <server.h>
#include <casEventSysIL.h> // casMonitor inline func

#if 0
VERSIONID(caEventQueuec,"%W% %G%")
#endif

#if 0
#include 	<memDebugLib.h>
#endif


//
// casEventSys::show()
//
void casEventSys::show(unsigned level)
{
	printf ("casEventSys at %x\n", (unsigned) this);
	if (level>=1u) {
		printf ("\thas coreClient at %x\n", (unsigned) &this->coreClient);
		printf ("\tnumEventBlocks = %d, maxLogEntries = %d\n",
			this->numEventBlocks, this->maxLogEntries);	
		printf ("\tthere are %d events in the queue\n",
			this->eventLogQue.count());
		printf ("\tevents off =  %d\n", this->eventsOff);
	}
}


//
// casEventSys::~casEventSys()
//
casEventSys::~casEventSys()
{
	casEvent 	*pE;
	
	this->mutex.lock();

	/*
	 * They must cancel all active event blocks first
	 */
	assert (this->numEventBlocks==0);

	while ( (pE = this->eventLogQue.get()) ) {
		delete pE;
	}
}


//
// casEventSys::installMonitor()
//
void casEventSys::installMonitor()
{
	this->mutex.lock();
	this->numEventBlocks++;
	this->maxLogEntries += averageEventEntries;
	this->mutex.unlock();
}

//
// casEventSys::removeMonitor()
//
void casEventSys::removeMonitor() 
{       
	this->mutex.lock();
	assert (this->numEventBlocks>=1u);
	this->numEventBlocks--;
	this->maxLogEntries -= averageEventEntries;
	this->mutex.unlock();
}


//
// casEventSys::process()
//
casProcCond casEventSys::process()
{
	casEvent	*pEvent;
	caStatus	status;
	casProcCond	cond = casProcOk;
	unsigned long	nAccepted = 0u;

	this->mutex.lock();

	while ( (pEvent = this->eventLogQue.get()) ) {

		status = pEvent->cbFunc(*this);
		if (status==S_cas_success) {
			/*
			 * only remove it after it was accepted by the
			 * client
			 */
			nAccepted++;
		}
		else if (status==S_cas_sendBlocked) {
			/*
			 * not accepted so return to the head of the list
			 * (we will try again later)
			 */
			this->insertEventQueue(*pEvent);
			cond = casProcOk;
			break;
		}
		else if (status==S_cas_disconnect) {
			cond = casProcDisconnect;
			break;
		}
		else {
			errMessage(status, 
				"unexpect error processing event");
			cond = casProcDisconnect;
			break;
		}
  	}

	/*
	 * call flush function if they provided one 
	 */
	if (nAccepted > 0u) {
		this->coreClient.eventFlush();
	}

	this->mutex.unlock();

	return cond;
}


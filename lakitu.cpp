// lakitu.cpp
// Capture continuously from all found MATRIX VISION devices
//
// *** LINUX VERSION ***
//
// No display. Uses Linux threads.
//
#ifndef linux
#error This file is only for Linux
#endif

#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <apps/Common/exampleHelper.h>
#include <mvIMPACT_CPP/mvIMPACT_acquire.h>

using namespace std;
using namespace mvIMPACT::acquire;


static unsigned int capture_data(Device *pDev) {
    unsigned int cnt = 0;

    try {
        pDev->open();
    } catch(const ImpactAcquireException& e) {
        // this e.g. might happen if the same device is already opened in another process...
        //cout << "An error occurred while opening the device " << pThreadParameter->device()->serial.read()
        cout << "An error occurred while opening the device " << pDev->serial.read()
             << "(error code: " << e.getErrorCode() << "(" << e.getErrorCodeAsString() << ")). Terminating thread." << endl
             << "Press [ENTER] to end the application..."
             << endl;
        getchar();
        return -1;
    }

    // establish access to the statistic properties
    //Statistics statistics(pThreadParameter->device());
    Statistics statistics(pDev);
    // create an interface to the device found
    FunctionInterface fi(pDev);

    // prefill the capture queue. There can be more then 1 queue for some device, but for this sample
    // we will work with the default capture queue. If a device supports more then one capture or result
    // queue, this will be stated in the manual. If nothing is set about it, the device supports one
    // queue only. Request as many images as possible. If there are no more free requests 'DEV_NO_FREE_REQUEST_AVAILABLE'
    // will be returned by the driver.
    int result = DMR_NO_ERROR;
    SystemSettings ss(pDev);
    const int REQUEST_COUNT = ss.requestCount.read();
    for(int i = 0; i < REQUEST_COUNT; i++) {
        result = fi.imageRequestSingle();
        if(result != DMR_NO_ERROR) {
            cout << "Error while filling the request queue: " << ImpactAcquireException::getErrorCodeAsString( result ) << endl;
        }
    }

    // run thread loop
    const Request* pRequest = 0;
    const unsigned int timeout_ms = 8000;   // USB 1.1 on an embedded system needs a large timeout for the first image
    int requestNr = INVALID_ID;
    // This next comment is valid once we have a display:
    // we always have to keep at least 2 images as the display module might want to repaint the image, thus we
    // can't free it unless we have a assigned the display to a new buffer.
    int lastRequestNr = INVALID_ID;
    //while(!pThreadParameter->terminated()) {
    while(true) {
        // wait for results from the default capture queue
        requestNr = fi.imageRequestWaitFor(timeout_ms);
        if(fi.isRequestNrValid(requestNr)) {
            pRequest = fi.getRequest(requestNr);
            if(pRequest->isOK()) {
                ++cnt;
                // here we can display some statistical information every 100th image
                if(cnt % 100 == 0) {
                    //cout << "Info from " << pThreadParameter->device()->serial.read()
                    cout << "Info from " << pDev->serial.read()
                         << ": " << statistics.framesPerSecond.name() << ": " << statistics.framesPerSecond.readS()
                         << ", " << statistics.errorCount.name() << ": " << statistics.errorCount.readS()
                         << ", " << statistics.captureTime_s.name() << ": " << statistics.captureTime_s.readS() << endl;
                }
            } else {
                cout << "Error: " << pRequest->requestResult.readS() << endl;
            }

            if(fi.isRequestNrValid(lastRequestNr)) {
                // this image has been displayed thus the buffer is no longer needed...
                fi.imageRequestUnlock(lastRequestNr);
            }
            lastRequestNr = requestNr;
            // send a new image request into the capture queue
            fi.imageRequestSingle();
        } else {
            // If the error code is -2119(DEV_WAIT_FOR_REQUEST_FAILED), the documentation will provide
            // additional information under TDMR_ERROR in the interface reference (
            cout << "imageRequestWaitFor failed (" << requestNr << ", " << ImpactAcquireException::getErrorCodeAsString( requestNr )
                 << ", device " << pDev->serial.read() << ") timeout value too small?" << endl;
        }
    }

    // free the last potential locked request
    if(fi.isRequestNrValid(requestNr)) {
        fi.imageRequestUnlock(requestNr);
    }
    // clear the request queue
    fi.imageRequestReset(0,0);
    return 0;
}

int main( int /*argc*/, char* /*argv*/[] ) {
    DeviceManager devMgr;
    const unsigned int devCnt = devMgr.deviceCount();
    if( devCnt == 0 ) {
        cout << "No MATRIX VISION device found! Unable to continue!" << endl;
        return 0;
    }
    Device *pDev = getDeviceFromUserInput(devMgr);

    // start capture
    capture_data(pDev);

    // now all threads will start running...
    cout << "Press any key to end the acquisition" << endl;
    getchar();

    // stop all threads again
    cout << "Terminating capture..." << endl;

    return 0;
}

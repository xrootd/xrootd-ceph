//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientConnAdmin                                                   //
//                                                                      //
// Author: G. Ganis (CERN, 2007)                                        //
//                                                                      //
// High level handler of connections for XrdClientAdmin.                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef XRD_ADMIN_CONN_H
#define XRD_ADMIN_CONN_H


#include "XrdClient/XrdClientConn.hh"

class XrdClientConnAdmin : public XrdClientConn {

private:

    bool             fInit;
    bool             fRedirected; // TRUE if has been redirected

public:
    XrdClientConnAdmin() : XrdClientConn(), fInit(0) { }
    virtual ~XrdClientConnAdmin() { }

    bool                       GetAccessToSrv();
    XReqErrorType              GoToAnotherServer(XrdClientUrlInfo newdest);
    bool                       SendGenCommand(ClientRequest *req, 
                                              const void *reqMoreData,
                                              void **answMoreDataAllocated,
                                              void *answMoreData, bool HasToAlloc,
                                              char *CmdName, int substreamid = 0);
};

#endif

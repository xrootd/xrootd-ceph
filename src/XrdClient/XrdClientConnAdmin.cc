//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientConnAdmin                                                   //
//                                                                      //
// Author: G. Ganis (CERN, 2007)                                        //
//                                                                      //
// High level handler of connections for XrdClientAdmin.                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

const char *XrdClientConnAdminCVSID = "$Id$";

#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientConnAdmin.hh"
#include "XrdClient/XrdClientLogConnection.hh"

//_____________________________________________________________________________
bool XrdClientConnAdmin::SendGenCommand(ClientRequest *req, const void *reqMoreData,
                                        void **answMoreDataAllocated, 
                                        void *answMoreData, bool HasToAlloc,
                                        char *CmdName,
                                        int substreamid)
{
   // SendGenCommand tries to send a single command for a number of times 

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientConnAdmin::SendGenCommand",
                                  " CmdName: " << CmdName << ", fInit: " << fInit);

   // Run the command
   bool fInitSv = fInit;
   fInit = 0;
   fRedirected = 0;
   bool rc = XrdClientConn::SendGenCommand(req, reqMoreData, answMoreDataAllocated, 
                                           answMoreData, HasToAlloc,
                                           CmdName, substreamid);
   fInit = fInitSv;
   if (fInit && fRedirected) {
      if (GoToAnotherServer(*GetLBSUrl()) != kOK)
         return 0;
      fGlobalRedirCnt = 0;
   }

   //  Done
   return rc;
}

//_____________________________________________________________________________
XReqErrorType XrdClientConnAdmin::GoToAnotherServer(XrdClientUrlInfo newdest)
{
   // Re-directs to another server

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientConnAdmin::GoToAnotherServer",
        " going to "<<newdest.Host<<":"<<newdest.Port);

   // Disconnect existing logical connection
   Disconnect(0);

   // Connect to the new destination
   if (Connect(newdest, fUnsolMsgHandler) == -1) {
      // Note: if Connect is unable to work then we are in trouble.
      // It seems that we have been redirected to a non working server
      Error("GoToAnotherServer",
            "Error connecting to ["<<newdest.Host<<":"<<newdest.Port);
      // If no conn is possible then we return to the load balancer
      return kREDIRCONNECT;
   }
   //
   // Set fUrl to the new data/lb server if the connection has been succesfull
   fUrl = newdest;

   // Check if we need to handshake: this will be needed the first time or if
   // the underlying physical connection has gone
   XrdClientPhyConnection *phyconn =
      ConnectionManager->GetConnection(GetLogConnID())->GetPhyConnection();

   // If already logged-in we have nothing more to do
   if (phyconn->IsLogged()) {
      // Flag redirection
      fRedirected = 1;
      // Already handshaked successfully
      return kOK;
   }

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientConnAdmin::GoToAnotherServer",
        " getting access to "<<newdest.Host<<":"<<newdest.Port);

   // Run the handshake
   if (IsConnected() && !GetAccessToSrv()) {
      Error("GoToAnotherServer",
            "Error handshaking to ["<<newdest.Host<<":"<<newdest.Port << "]");
      return kREDIRCONNECT;
   }

   // Ok
   SetStreamID(ConnectionManager->GetConnection(GetLogConnID())->Streamid());
   // Flag redirection
   fRedirected = 1;

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientConnAdmin::GoToAnotherServer",
        " done! ("<<newdest.Host<<":"<<newdest.Port<<")");

   // Done
   return kOK;
}

//_____________________________________________________________________________
bool XrdClientConnAdmin::GetAccessToSrv()
{
   // Gets access to the connected server. The login and authorization steps
   // are performed here (calling method DoLogin() that performs logging-in
   // and calls DoAuthentication() ).
   // If the server redirects us, this is gently handled by the general
   // functions devoted to the handling of the server's responses.
   // Nothing is visible here, and nothing is visible from the other high
   // level functions.

   bool rc = XrdClientConn::GetAccessToSrv();

   // Trim the LSBUrl
   if (fLBSUrl)
      fLBSUrl->File = "";

   // Flag we went through here at least once
   fInit = 1;

   // Done
   return rc;
}

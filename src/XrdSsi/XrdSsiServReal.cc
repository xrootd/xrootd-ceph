/******************************************************************************/
/*                                                                            */
/*                     X r d S s i S e r v R e a l . c c                      */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <errno.h>
#include <stdio.h>
#include <string.h>
  
#include "XrdSsi/XrdSsiServReal.hh"
#include "XrdSsi/XrdSsiSessReal.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace
{
       XrdSysMutex           ChMutex;
       const char            Channel[4] = {'0', '1', '2', '3'};
       Atomic(unsigned int)  Chnum      = 0;
static const  unsigned int   ChMask     = 3;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSsiServReal::~XrdSsiServReal()
{
   XrdSsiSessReal *sP;

// Free pointer to the manager node
//
   if (manNode) {free(manNode); manNode = 0;}

// Delete all free session objects
//
   while((sP = freeSes))
        {freeSes = sP->nextSess;
         delete sP;
        }
}

/******************************************************************************/
/* Private:                        A l l o c                                  */
/******************************************************************************/

XrdSsiSessReal *XrdSsiServReal::Alloc(const char *sName)
{
   XrdSsiSessReal *sP;

// Check if we can grab this from out queue
//
   myMutex.Lock();
   actvSes++;
   if ((sP = freeSes))
      {freeCnt--;
       freeSes = sP->nextSess;
       myMutex.UnLock();
       sP->InitSession(this, sName);
      } else {
       myMutex.UnLock();
       if (!(sP = new XrdSsiSessReal(this, sName)))
          {myMutex.Lock(); actvSes--; myMutex.UnLock();}
      }

// Return the pointer
//
   return sP;
}

/******************************************************************************/
/* Private:                       G e n U R L                                 */
/******************************************************************************/
  
bool XrdSsiServReal::GenURL(XrdSsiService::Resource *rP,
                            char *buff, int blen, bool uCon)
{
   static const char affTab[] = "\0\0n\0w\0s\0S";
   const char *iSep, *iVal, *tVar, *tVal, *uVar, *uVal;
   const char *aVar, *aVal;
   unsigned int theCh;
   int n;
   char ChID;
   bool xCGI = false;

// Get the channel number to use for this request
//
   Atomic_BEG(ChMutex);
   theCh = Atomic_INC(Chnum);
   Atomic_END(ChMutex);
   ChID = Channel[theCh & ChMask];

// Preprocess avoid list, if any
//
   if (!(rP->rDesc.hAvoid) || !*(rP->rDesc.hAvoid)) tVar = tVal = "";
      else {tVar = "?tried=";
            tVal = rP->rDesc.hAvoid;
            xCGI = true;
           }

// Preprocess affinity
//
   if (!(rP->rDesc.affinity)) aVar = aVal = "";
      else {aVar = (xCGI ? "&cms.aff=" : "?cms.aff=");
            aVal = &affTab[rP->rDesc.affinity*2];
            xCGI = true;
           }

// Check if we need to specify a user name
//
   if (!rP->rDesc.rUser || !(*rP->rDesc.rUser)) uVar = uVal = "";
      else {uVar = (xCGI ? "&ssi.user=" : "?ssi.user=");
            uVal = rP->rDesc.rUser;
            xCGI = true;
           }

// Preprocess the cgi information
//
   if (!(rP->rDesc.rInfo) || !*(rP->rDesc.rInfo)) iSep = iVal = "";
      else {iVal = rP->rDesc.rInfo;
            if (xCGI) iSep = (*iVal == '&' ? "" : "&");
               else   iSep = (*iVal == '?' ? "" : "?");
           }

// Generate appropriate url
//                                               t   a   u   i
   n = snprintf(buff, blen, "xroot://ssi%c@%s/%s%s%s%s%s%s%s%s%s",
                             ChID, manNode,
                             rP->rDesc.rName, tVar, tVal, aVar, aVal,
                                              uVar, uVal, iSep, iVal);

// Return overflow or not
//
   return n < blen;
}

/******************************************************************************/
/*                             P r o v i s i o n                              */
/******************************************************************************/
  
void XrdSsiServReal::Provision(XrdSsiService::Resource *resP,
                               unsigned short           timeOut,
                               bool                     userConn
                              )
{
   XrdSsiSessReal *sObj;
   char            epURL[4096];

// Validate the resource name
//
   if (!resP->rDesc.rName || !(*resP->rDesc.rName))
      {resP->eInfo.Set("Resource name missing.", EINVAL);
       resP->ProvisionDone(0);
       return;
      }

// Construct url
//
   if (!GenURL(resP, epURL, sizeof(epURL), userConn))
      {resP->eInfo.Set("Resource url is too long.", ENAMETOOLONG);
       resP->ProvisionDone(0);
       return;
      }

// Obtain a new session object
//
   if (!(sObj = Alloc(resP->rDesc.rName)))
      {resP->eInfo.Set("Insufficient memory.", ENOMEM);
       resP->ProvisionDone(0);
       return;
      }

// Now just effect an open to this resource
//
   if (!(sObj->Open(resP, epURL, timeOut,
                    (resP->rDesc.rOpts & XrdSsiResource::autoUnP) != 0)))
      {Recycle(sObj);
       resP->ProvisionDone(0);
      }
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdSsiServReal::Recycle(XrdSsiSessReal *sObj)
{

// Add to queue unless we have too many of these
//
   myMutex.Lock();
   actvSes--;
   if (freeCnt >= freeMax) {myMutex.UnLock(); delete sObj;}
      else {sObj->ClrEvent();
            sObj->nextSess = freeSes;
            freeSes = sObj;
            freeCnt++;
            myMutex.UnLock();
           }
}

/******************************************************************************/
/* Private:                       R e t E r r                                 */
/******************************************************************************/
  
XrdSsiSession *XrdSsiServReal::RetErr(XrdSsiErrInfo &eInfo,
                                     const char    *eTxt, int eNum, bool async)
{
// Set the error information
//
   eInfo.Set(eTxt, eNum);

// Now return dependent on the processing mode
//
   return (async ? (XrdSsiSession *)-1 : 0);
}

/******************************************************************************/
/*                                  S t o p                                   */
/******************************************************************************/
  
bool XrdSsiServReal::Stop()
{
// Make sure we are clean
//
   myMutex.Lock();
   if (actvSes) {myMutex.UnLock(); return false;}
   myMutex.UnLock();
   delete this;
   return true;
}

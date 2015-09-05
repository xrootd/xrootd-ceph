#ifndef __SSI_FILE_H__
#define __SSI_FILE_H__
/******************************************************************************/
/*                                                                            */
/*                         X r d S s i F i l e . h h                          */
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

#include <string.h>
#include <sys/types.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSsi/XrdSsiBVec.hh"
#include "XrdSsi/XrdSsiFileReq.hh"
#include "XrdSsi/XrdSsiRRTable.hh"
#include "XrdSys/XrdSysPthread.hh"
  
class XrdSfsXioHandle;
class XrdSsiSession;

class XrdSsiFile : public XrdSfsFile
{
public:

// SfsFile methods
//
        int              open(const char                *fileName,
                                    XrdSfsFileOpenMode   openMode,
                                    mode_t               createMode,
                              const XrdSecEntity        *client = 0,
                              const char                *opaque = 0);
                        
        int              close();
                        
        int              fctl(const int            cmd,
                              const char          *args,
                                    XrdOucErrInfo &out_error);

        int              fctl(const int            cmd,
                                    int            alen,
                              const char          *args,
                              const XrdSecEntity  *client);
                        
        const char      *FName();
                        
        int              getCXinfo(char cxtype[4], int &cxrsz);
                        
        int              getMmap(void **Addr, off_t &Size);
                        
        int              read(XrdSfsFileOffset   fileOffset,
                              XrdSfsXferSize     preread_sz);
                        
        XrdSfsXferSize   read(XrdSfsFileOffset   fileOffset,
                              char              *buffer,
                              XrdSfsXferSize     buffer_size);
                        
        int              read(XrdSfsAio *aioparm);

        XrdSfsXferSize   readv(XrdOucIOVec      *readV,
                               int           readCount);

        int              SendData(XrdSfsDio         *sfDio,
                                  XrdSfsFileOffset   offset,
                                  XrdSfsXferSize     size);

static  void             SetAuth(int axq) {authXQ = axq;}

static  void             SetMaxSz(int mSz) {maxRSZ = mSz;}
                        
        void             setXio(XrdSfsXio *xP) {xioP = xP;}
                        
        int              stat(struct stat *buf);
                        
        int              sync();
                        
        int              sync(XrdSfsAio *aiop);
                        
        int              truncate(XrdSfsFileOffset fileOffset);
                        
        XrdSfsXferSize   write(XrdSfsFileOffset   fileOffset,
                               const char        *buffer,
                               XrdSfsXferSize     buffer_size);
                        
        int              write(XrdSfsAio *aioparm);
                        
// Constructor and destructor
//                      
                         XrdSsiFile(const char *user, int MonID);
                        
virtual                 ~XrdSsiFile();
                        
                        
private:                
void                     CopyECB();
int                      CopyErr(const char *op, int rc);
char                    *GetUser(const char *incgi);
bool                     NewRequest(int reqid, XrdOucBuffer *oP,
                                    XrdSfsXioHandle *bR, int rSz);
XrdSfsXferSize           writeAdd(const char *buff, XrdSfsXferSize blen, int rid);
static int               maxRSZ;
static int               authXQ;

const char              *tident;
char                    *gigID;
char                    *fsUser;
XrdSfsFile              *fsFile;
XrdSysMutex              myMutex;
XrdSfsXio               *xioP;
XrdOucBuffer            *oucBuff;
XrdSsiSession           *sessP;
int                      reqSize;
int                      reqLeft;
bool                     isOpen;
bool                     inProg;
bool                     viaDel;

XrdSsiBVec               eofVec;
XrdSsiRRTable<XrdSsiFileReq> rTab;
};
#endif
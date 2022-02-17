//------------------------------------------------------------------------------
// Copyright (c) 2014-2015 by European Organization for Nuclear Research (CERN)
// Author: Sebastien Ponce <sebastien.ponce@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#include <sys/types.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fcntl.h>
#include <iomanip>
#include <ctime>

#include "XrdCeph/XrdCephPosix.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdCeph/XrdCephOssFile.hh"

#include "XrdCeph/XrdCephOssBufferedFile.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephBufferAlgSimple.hh"
#include "XrdCeph/XrdCephBuffers/XrdCephBufferDataSimple.hh"
#include "XrdCeph/XrdCephBuffers/CephIOAdapterRaw.hh"
#include "XrdCeph/XrdCephBuffers/CephIOAdapterAIORaw.hh"

using namespace XrdCephBuffer;

extern XrdSysError XrdCephEroute;
extern XrdOucTrace XrdCephTrace;


XrdCephOssBufferedFile::XrdCephOssBufferedFile(XrdCephOss *cephoss,XrdCephOssFile *cephossDF, 
                                                size_t buffersize):
                  XrdCephOssFile(cephoss), m_cephoss(cephoss), m_xrdOssDF(cephossDF), m_bufsize(buffersize)
{

}

XrdCephOssBufferedFile::~XrdCephOssBufferedFile() {
    // XrdCephEroute.Say("XrdCephOssBufferedFile::Destructor");

  // remember to delete the inner XrdCephOssFile object
  if (m_xrdOssDF) {
    delete m_xrdOssDF;
    m_xrdOssDF = nullptr;
  }

}


int XrdCephOssBufferedFile::Open(const char *path, int flags, mode_t mode, XrdOucEnv &env) {

  int rc = m_xrdOssDF->Open(path, flags, mode, env);
  if (rc < 0) {
    return rc;
  }
  m_fd = m_xrdOssDF->getFileDescriptor();
  BUFLOG("XrdCephOssBufferedFile::Open got fd: " << m_fd << " " << path);
  m_flags = flags; // e.g. for write/read knowledge
  m_path  = path; // good to keep the path for final stats presentation

  // opened a file, so create the buffer here; note - this might be better delegated to the first read/write ...
  // need the file descriptor, so do it after we know the file is opened (and not just a stat for example)
  std::unique_ptr<IXrdCephBufferData> cephbuffer = std::unique_ptr<IXrdCephBufferData>(new XrdCephBufferDataSimple(m_bufsize));
  // std::unique_ptr<ICephIOAdapter>     cephio     = std::unique_ptr<ICephIOAdapter>(new CephIOAdapterRaw(cephbuffer.get(),m_fd));
  std::unique_ptr<ICephIOAdapter>     cephio     = std::unique_ptr<ICephIOAdapter>(new CephIOAdapterAIORaw(cephbuffer.get(),m_fd));

  LOGCEPH( "XrdCephOssBufferedFile::Open: fd: " << m_fd <<  " Buffer created: " << cephbuffer->capacity() );
  m_bufferAlg = std::unique_ptr<IXrdCephBufferAlg>(new XrdCephBufferAlgSimple(std::move(cephbuffer),std::move(cephio),m_fd) );

  // start the timer
  //m_timestart = std::chrono::steady_clock::now();
  m_timestart = std::chrono::system_clock::now();
  // return the file descriptor 
  return rc;
}

int XrdCephOssBufferedFile::Close(long long *retsz) {
  // if data is still in the buffer and we are writing, make sure to write it to 
  if ((m_flags & (O_WRONLY|O_RDWR)) != 0) {
    ssize_t rc = m_bufferAlg->flushWriteCache();
    if (rc < 0) {
        LOGCEPH( "XrdCephOssBufferedFile::Close: flush Error fd: " << m_fd << " rc:" << rc );
        // still try to close the file
        ssize_t rc2 = m_xrdOssDF->Close(retsz);
        if (rc2 < 0) {
          LOGCEPH( "XrdCephOssBufferedFile::Close: Close error after flush Error fd: " << m_fd << " rc:" << rc2 );
        }
        // still attempt to close the file; ignore the return error here
        m_xrdOssDF->Close(retsz);
        return rc; // return the original flush error
    }
  } // check for write
  const std::chrono::time_point<std::chrono::system_clock> now =
         std::chrono::system_clock::now();
  const std::time_t t_s = std::chrono::system_clock::to_time_t(m_timestart);
  const std::time_t t_c = std::chrono::system_clock::to_time_t(now);

  auto t_dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timestart).count();

  LOGCEPH("XrdCephOssBufferedFile::Summary: {\"fd\":" << m_fd << ", \"Elapsed_time_ms\":" << t_dur 
          << ", \"path\":\"" << m_path  
          << "\", read_B:"   << m_bytesRead.load() 
          << ", readV_B:"     << m_bytesReadV.load() 
          << ", readAIO_B:"   << m_bytesReadAIO.load() 
          << ", writeB:"     << m_bytesWrite.load()
          << ", writeAIO_B:" << m_bytesWriteAIO.load()
          << ", startTime:\"" << std::put_time(std::localtime(&t_s), "%F %T") << "\", endTime:\"" 
          << std::put_time(std::localtime(&t_c), "%F %T") << "\""
          << "}");

  return m_xrdOssDF->Close(retsz);
}


ssize_t XrdCephOssBufferedFile::ReadV(XrdOucIOVec *readV, int rnum) {
  // don't touch readV in the buffering method
  ssize_t rc = m_xrdOssDF->ReadV(readV,rnum);
  if (rc > 0) m_bytesReadV.fetch_add(rc);
  return rc;
}

ssize_t XrdCephOssBufferedFile::Read(off_t offset, size_t blen) {
  return m_xrdOssDF->Read(offset, blen);
}

ssize_t XrdCephOssBufferedFile::Read(void *buff, off_t offset, size_t blen) {
  ssize_t rc = m_bufferAlg->read(buff, offset, blen);
  if (rc >=0) {
    m_bytesRead.fetch_add(rc);
  }
  return rc;
}

int XrdCephOssBufferedFile::Read(XrdSfsAio *aiop) {

  // LOGCEPH("XrdCephOssBufferedFile::AIOREAD: fd: " << m_xrdOssDF->getFileDescriptor() << "  "  << time(nullptr) << " : " 
  //         << aiop->sfsAio.aio_offset << " " 
  //         << aiop->sfsAio.aio_nbytes << " " << aiop->sfsAio.aio_reqprio << " "
  //         << aiop->sfsAio.aio_fildes );
  ssize_t rc = m_bufferAlg->read_aio(aiop);
  if (rc > 0) m_bytesReadAIO.fetch_add(rc);
  return rc;
}

ssize_t XrdCephOssBufferedFile::ReadRaw(void *buff, off_t offset, size_t blen) {
  // #TODO; ReadRaw should bypass the buffer ?
  return m_xrdOssDF->ReadRaw(buff, offset, blen);
}

int XrdCephOssBufferedFile::Fstat(struct stat *buff) {
  return m_xrdOssDF->Fstat(buff);
}

ssize_t XrdCephOssBufferedFile::Write(const void *buff, off_t offset, size_t blen) {
  ssize_t rc = m_bufferAlg->write(buff, offset, blen);
  if (rc >=0) {
    m_bytesWrite.fetch_add(rc);
  }
  return rc;
}

int XrdCephOssBufferedFile::Write(XrdSfsAio *aiop) {
  // LOGCEPH("XrdCephOssBufferedFile::AIOWRITE: fd: " << m_xrdOssDF->getFileDescriptor() << "  "   << time(nullptr) << " : " 
  //         << aiop->sfsAio.aio_offset << " " 
  //         << aiop->sfsAio.aio_nbytes << " " << aiop->sfsAio.aio_reqprio << " "
  //         << aiop->sfsAio.aio_fildes << " " );
  ssize_t rc = m_bufferAlg->write_aio(aiop);
  if (rc > 0) m_bytesWriteAIO.fetch_add(rc);
  return rc;

}

int XrdCephOssBufferedFile::Fsync() {
  return m_xrdOssDF->Fsync();
}

int XrdCephOssBufferedFile::Ftruncate(unsigned long long len) {
  return m_xrdOssDF->Ftruncate(len);
}

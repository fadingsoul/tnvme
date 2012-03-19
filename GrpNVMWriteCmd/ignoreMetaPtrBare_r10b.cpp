/*
 * Copyright (c) 2011, Intel Corporation.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "boost/format.hpp"
#include "ignoreMetaPtrBare_r10b.h"
#include "globals.h"
#include "grpDefs.h"
#include "../Queues/iocq.h"
#include "../Queues/iosq.h"
#include "../Utils/io.h"
#include "../Cmds/write.h"

namespace GrpNVMWriteCmd {


IgnoreMetaPtrBare_r10b::IgnoreMetaPtrBare_r10b(int fd, string mGrpName, string mTestName,
    ErrorRegs errRegs) :
    Test(fd, mGrpName, mTestName, SPECREV_10b, errRegs)
{
    // 63 chars allowed:     xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    mTestDesc.SetCompliance("revision 1.0b, section 4,6");
    mTestDesc.SetShort(     "Verify metadata ptr is not used for bare namspc");
    // No string size limit for the long description
    mTestDesc.SetLong(
        "MPTR is only used if metadata is not interleaved with the data. For "
        "all bare namspcs from Identify.NN issue a single write cmd sending 1 "
        "data block at LBA 0; set the meta ptr to max value, expect success.");
}


IgnoreMetaPtrBare_r10b::~IgnoreMetaPtrBare_r10b()
{
    ///////////////////////////////////////////////////////////////////////////
    // Allocations taken from the heap and not under the control of the
    // RsrcMngr need to be freed/deleted here.
    ///////////////////////////////////////////////////////////////////////////
}


IgnoreMetaPtrBare_r10b::
IgnoreMetaPtrBare_r10b(const IgnoreMetaPtrBare_r10b &other) : Test(other)
{
    ///////////////////////////////////////////////////////////////////////////
    // All pointers in this object must be NULL, never allow shallow or deep
    // copies, see Test::Clone() header comment.
    ///////////////////////////////////////////////////////////////////////////
}


IgnoreMetaPtrBare_r10b &
IgnoreMetaPtrBare_r10b::operator=(const IgnoreMetaPtrBare_r10b &other)
{
    ///////////////////////////////////////////////////////////////////////////
    // All pointers in this object must be NULL, never allow shallow or deep
    // copies, see Test::Clone() header comment.
    ///////////////////////////////////////////////////////////////////////////
    Test::operator=(other);
    return *this;
}


void
IgnoreMetaPtrBare_r10b::RunCoreTest()
{
    /** \verbatim
     * Assumptions:
     * 1) Test CreateResources_r10b has run prior.
     * \endverbatim
     */
    string work;
    uint32_t numCE;
    uint32_t isrCountB4;
    ConstSharedIdentifyPtr namSpcPtr;

    // Lookup objs which were created in a prior test within group
    SharedIOSQPtr iosq = CAST_TO_IOSQ(gRsrcMngr->GetObj(IOSQ_GROUP_ID));
    SharedIOCQPtr iocq = CAST_TO_IOCQ(gRsrcMngr->GetObj(IOCQ_GROUP_ID));

    if ((numCE = iocq->ReapInquiry(isrCountB4, true)) != 0) {
        iocq->Dump(
            FileSystem::PrepLogFile(mGrpName, mTestName, "iocq",
            "notEmpty"), "Test assumption have not been met");
        throw FrmwkEx("Require 0 CE's within CQ %d, not upheld, found %d",
            iocq->GetQId(), numCE);
    }

    LOG_NRM("For all bare namspc's issue cmd with non-zero meta ptr");
    vector<uint32_t> bare = gInformative->GetBareNamespaces();
    for (size_t i = 0; i < bare.size(); i++) {

        namSpcPtr = gInformative->GetIdentifyCmdNamspc(bare[i]);
        if (namSpcPtr == Identify::NullIdentifyPtr)
            throw FrmwkEx("Identify namspc struct #%d doesn't exist", bare[i]);

        LOG_NRM("Setup read cmd's values that won't change per namspc");
        SharedMemBufferPtr writeMem = SharedMemBufferPtr(new MemBuffer());
        uint64_t lbaDataSize = namSpcPtr->GetLBADataSize();
        writeMem->Init(lbaDataSize);

        SharedWritePtr writeCmd = SharedWritePtr(new Write());
        send_64b_bitmask prpBitmask = (send_64b_bitmask)
            (MASK_PRP1_PAGE | MASK_PRP2_PAGE | MASK_PRP2_LIST);
        writeCmd->SetPrpBuffer(prpBitmask, writeMem);
        writeCmd->SetNLB(0);    // convert to 0-based value
        writeCmd->SetSLBA(0);

        LOG_NRM("Set MPTR in cmd to max value");
        writeCmd->SetDword(0xffffffff, 4);
        writeCmd->SetDword(0xffffffff, 5);

        writeCmd->SetNSID(bare[i]);
        work = str(boost::format("namspc%d") % bare[i]);
        IO::SendCmdToHdw(mGrpName, mTestName, DEFAULT_CMD_WAIT_ms, iosq, iocq,
            writeCmd, work, true);
    }
}


}   // namespace

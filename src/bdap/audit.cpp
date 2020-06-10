// Copyright (c) 2019-2020 Duality Blockchain Solutions Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bdap/audit.h"

#include "bdap/bdap.h"
#include "bdap/utils.h"
#include "key.h"
#include "hash.h"
#include "pubkey.h"
#include "serialize.h"
#include "streams.h"
#include "validation.h"

#include <univalue.h>

void CAuditData::Serialize(std::vector<unsigned char>& vchData) 
{
    CDataStream dsAuditData(SER_NETWORK, PROTOCOL_VERSION);
    dsAuditData << *this;
    vchData = std::vector<unsigned char>(dsAuditData.begin(), dsAuditData.end());
}

bool CAuditData::UnserializeFromData(const std::vector<unsigned char>& vchData, const std::vector<unsigned char>& vchHash) 
{
    try {
        CDataStream dsAuditData(vchData, SER_NETWORK, PROTOCOL_VERSION);
        dsAuditData >> *this;

        std::vector<unsigned char> vchAuditData;
        Serialize(vchAuditData);
        const uint256& calculatedHash = Hash(vchAuditData.begin(), vchAuditData.end());
        const std::vector<unsigned char>& vchRandAuditData = vchFromValue(calculatedHash.GetHex());
        if(vchRandAuditData != vchHash) {
            SetNull();
            return false;
        }
    } catch (std::exception& e) {
        SetNull();
        return false;
    }
    return true;
}

bool CAuditData::UnserializeFromData(const std::vector<unsigned char>& vchData) 
{
    try {
        CDataStream dsAuditData(vchData, SER_NETWORK, PROTOCOL_VERSION);
        dsAuditData >> *this;

        std::vector<unsigned char> vchAuditData;
        Serialize(vchAuditData);
    } catch (std::exception& e) {
        SetNull();
        return false;
    }
    return true;
}

bool CAuditData::ValidateValues(std::string& strErrorMessage)
{
    for(const CharString& vchHash : vAuditData) {
        if (vchHash.size() > MAX_BDAP_AUDIT_HASH_SIZE) {
            strErrorMessage = "Invalid audit length. Can not have more than " + std::to_string(MAX_BDAP_AUDIT_HASH_SIZE) + " characters.";
            return false;
        }
    }
    return true;
}

CAudit::CAudit(CAuditData& auditData) {
    auditData.Serialize(vchAuditData);
}

uint256 CAudit::GetHash() const
{
    CDataStream dsAudit(SER_NETWORK, PROTOCOL_VERSION);
    dsAudit << *this;
    return Hash(dsAudit.begin(), dsAudit.end());
}

bool CAudit::Sign(const CKey& key)
{
    if (!key.Sign(Hash(vchAuditData.begin(), vchAuditData.end()), vchSignature)) {
        LogPrintf("CAudit::%s -- Failed to sign audit data.\n", __func__);
        return false;
    }

    return true;
}

bool CAudit::CheckSignature(const std::vector<unsigned char>& vchPubKey) const
{
    CPubKey key(vchPubKey);
    if (!key.Verify(Hash(vchAuditData.begin(), vchAuditData.end()), vchSignature))
        return error("CAudit::%s(): verify signature failed", __func__);

    return true;
}

int CAudit::Version() const
{
    if (vchAuditData.size() == 0)
        return -1;

    return CAuditData(vchAuditData).nVersion;
}

bool CAudit::ValidateValues(std::string& strErrorMessage) const
{
    CAuditData auditData(vchAuditData);
    if (!auditData.ValidateValues(strErrorMessage))
        return false;

    if (vchAuditData.size() == 0)
        return false;

    if (vchOwnerFullObjectPath.size() > MAX_OBJECT_FULL_PATH_LENGTH) {
        strErrorMessage = "Invalid BDAP audit owner FQDN length. Can not have more than " + std::to_string(MAX_OBJECT_FULL_PATH_LENGTH) + " characters.";
        return false;
    }

    if (vchSignature.size() > MAX_SIGNATURE_LENGTH) {
        strErrorMessage = "Invalid BDAP audit signature length. Can not have more than " + std::to_string(MAX_SIGNATURE_LENGTH) + " characters.";
        return false;
    }

    return CAuditData(vchAuditData).ValidateValues(strErrorMessage);
}

void CAudit::Serialize(std::vector<unsigned char>& vchData) 
{
    CDataStream dsAudit(SER_NETWORK, PROTOCOL_VERSION);
    dsAudit << *this;
    vchData = std::vector<unsigned char>(dsAudit.begin(), dsAudit.end());
}

bool CAudit::UnserializeFromData(const std::vector<unsigned char>& vchData, const std::vector<unsigned char>& vchHash) 
{
    try {
        CDataStream dsAudit(vchData, SER_NETWORK, PROTOCOL_VERSION);
        dsAudit >> *this;

        std::vector<unsigned char> vchAudit;
        Serialize(vchAudit);
        const uint256& calculatedHash = Hash(vchAudit.begin(), vchAudit.end());
        const std::vector<unsigned char>& vchRandAudit = vchFromValue(calculatedHash.GetHex());
        if(vchRandAudit != vchHash) {
            SetNull();
            return false;
        }
    } catch (std::exception& e) {
        SetNull();
        return false;
    }
    return true;
}

bool CAudit::UnserializeFromTx(const CTransactionRef& tx) 
{
    std::vector<unsigned char> vchData;
    std::vector<unsigned char> vchHash;
    int nOut;
    if(!GetBDAPData(tx, vchData, vchHash, nOut)) {
        SetNull();
        return false;
    }
    if(!UnserializeFromData(vchData, vchHash)) {
        return false;
    }
    return true;
}

std::string CAudit::ToString() const
{
    CAuditData auditData = GetAuditData();
    std::string strAuditData;
    for(const std::vector<unsigned char>& vchAudit : auditData.vAuditData)
        strAuditData += "\n                           " + stringFromVch(vchAudit);

    return strprintf(
        "CAudit(\n"
        "    nVersion             = %d\n"
        "    Audit Count          = %d\n"
        "    Audit Data           = %s\n"
        "    nTimeStamp           = %d\n"
        "    Owner                = %s\n"
        "    Signed               = %s\n"
        ")\n",
        auditData.nVersion,
        auditData.vAuditData.size(),
        strAuditData,
        auditData.nTimeStamp,
        stringFromVch(vchOwnerFullObjectPath),
        IsSigned() ? "True" : "False"
        );
}

bool BuildAuditJson(const CAudit& audit, UniValue& oAudit)
{
    bool expired = false;
    int64_t expired_time = 0;
    int64_t nTime = 0;
    CAuditData auditData = audit.GetAuditData();
    oAudit.push_back(Pair("version", std::to_string(audit.Version())));
    oAudit.push_back(Pair("audit count", auditData.vAuditData.size()));
    oAudit.push_back(Pair("timestamp", std::to_string(auditData.nTimeStamp)));
    oAudit.push_back(Pair("owner", stringFromVch(audit.vchOwnerFullObjectPath)));
    oAudit.push_back(Pair("signed", audit.IsSigned() ? "True" : "False"));
    oAudit.push_back(Pair("txid", audit.txHash.GetHex()));
    if ((unsigned int)chainActive.Height() >= audit.nHeight-1) {
        CBlockIndex *pindex = chainActive[audit.nHeight-1];
        if (pindex) {
            nTime = pindex->GetMedianTimePast();
        }
    }
    oAudit.push_back(Pair("time", nTime));
    oAudit.push_back(Pair("height", std::to_string(audit.nHeight)));
    expired_time = audit.nExpireTime;
    if(expired_time <= (unsigned int)chainActive.Tip()->GetMedianTimePast())
    {
        expired = true;
    }
    oAudit.push_back(Pair("expires_on", std::to_string(expired_time)));
    oAudit.push_back(Pair("expired", expired));
    return true;
}
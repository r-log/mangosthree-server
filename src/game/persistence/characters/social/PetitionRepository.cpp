/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "PetitionRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"
#include "Database/SqlOperations.h"

#include <sstream>
#include <utility>

namespace
{
    /// Queues a staged holder, deleting it if the database refuses it.
    void QueueHolder(SqlQueryHolder* holder, PetitionHolderCallback callback)
    {
        if (!CharacterDatabase.DelayQueryHolder(std::move(callback), holder))
        {
            delete holder;                                  // delete all unprocessed queries
        }
    }
}

namespace PetitionRepository
{
    void QueryOwnedPetitions(uint32 ownerGuid, std::function<void(std::vector<uint32>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }));
                                      },
                                      "SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '%u'", ownerGuid);
    }

    void QueryExists(uint32 petitionGuid, std::function<void(bool)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }).has_value());
                                      },
                                      "SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid);
    }

    void QueryOwner(uint32 petitionGuid, std::function<void(std::optional<uint32>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }));
                                      },
                                      "SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid);
    }

    static PetitionDetailsRow ReadDetails(Field* fields)
    {
        PetitionDetailsRow row;
        row.ownerGuid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        row.signs = fields[2].GetUInt8();
        return row;
    }

    void QueryDetails(uint32 petitionGuid, std::function<void(std::optional<PetitionDetailsRow>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<PetitionDetailsRow>(result, ReadDetails));
                                      },
                                      "SELECT `ownerguid`, `name`, "
                                      "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
                                      "FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid, petitionGuid);
    }

    void QueueShowSignatures(uint32 petitionGuid, PetitionHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(PETITION_SHOW_SLOT_COUNT);
        holder->SetPQuery(PETITION_SHOW_PETITION,
                          "SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid);
        holder->SetPQuery(PETITION_SHOW_SIGNATURES,
                          "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid);
        QueueHolder(holder, std::move(callback));
    }

    void QueueSignRead(uint32 petitionGuid, uint32 accountId, PetitionHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(PETITION_SIGN_READ_COUNT);
        holder->SetPQuery(PETITION_SIGN_READ_PETITION,
                          "SELECT `ownerguid`, "
                          "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
                          "FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid, petitionGuid);
        // the client does not let one character sign twice, but nothing stops a second character of
        // the same account; the account's signatures are read so the request can refuse it
        holder->SetPQuery(PETITION_SIGN_READ_ACCOUNT,
                          "SELECT `playerguid`, `petitionguid` FROM `petition_sign` WHERE `player_account` = '%u'", accountId);
        QueueHolder(holder, std::move(callback));
    }

    void QueueSign(uint32 petitionGuid, uint32 playerGuid, uint32 accountId, uint32 maxSigns, PetitionHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(PETITION_SIGNED_COUNT);
        holder->SetPQuery(PETITION_SIGNED_BEFORE,
                          "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u'",
                          petitionGuid, playerGuid);
        holder->SetPQuery(PETITION_SIGNED_INSERT,
                          "INSERT INTO `petition_sign` (`ownerguid`, `petitionguid`, `playerguid`, `player_account`) "
                          "SELECT `ownerguid`, `petitionguid`, '%u', '%u' FROM `petition` "
                          "WHERE `petitionguid` = '%u' "
                          "AND (SELECT COUNT(*) FROM `petition_sign` WHERE `petitionguid` = '%u') < %u "
                          "AND NOT EXISTS (SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u')",
                          playerGuid, accountId, petitionGuid, petitionGuid, maxSigns,
                          petitionGuid, playerGuid);
        holder->SetPQuery(PETITION_SIGNED_AFTER,
                          "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u'",
                          petitionGuid, playerGuid);
        QueueHolder(holder, std::move(callback));
    }

    void QueueTurnInRead(uint32 petitionGuid, PetitionHolderCallback callback)
    {
        SqlQueryHolder* holder = new SqlQueryHolder;
        holder->SetSize(PETITION_TURN_IN_COUNT);
        holder->SetPQuery(PETITION_TURN_IN_PETITION,
                          "SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid);
        holder->SetPQuery(PETITION_TURN_IN_SIGNATURES,
                          "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid);
        QueueHolder(holder, std::move(callback));
    }

    std::string GuidList(std::vector<uint32> const& oldPetitions, uint32 petitionGuid)
    {
        std::ostringstream ss;
        for (std::vector<uint32>::const_iterator itr = oldPetitions.begin(); itr != oldPetitions.end(); ++itr)
        {
            ss << "'" << *itr << "' , ";
        }
        ss << "'" << petitionGuid << "'";
        return ss.str();
    }

    void Create(uint32 ownerGuid, uint32 petitionGuid, std::string const& name, std::string const& guidList)
    {
        static SqlStatementID insertPetition;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insertPetition,
                            "INSERT INTO `petition` (`ownerguid`, `petitionguid`, `name`) VALUES (?, ?, ?)");

        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` IN ( %s )",  guidList.c_str());
        CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` IN ( %s )", guidList.c_str());
        stmt.PExecute(ownerGuid, petitionGuid, name.c_str());
        CharacterDatabase.CommitTransaction();
    }

    void Rename(uint32 petitionGuid, std::string const& name)
    {
        static SqlStatementID renamePetition;
        SqlStatement stmt = CharacterDatabase.CreateStatement(renamePetition,
                            "UPDATE `petition` SET `name` = ? WHERE `petitionguid` = ?");
        stmt.PExecute(name.c_str(), petitionGuid);
    }

    void Delete(uint32 petitionGuid)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid);
        CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid);
        CharacterDatabase.CommitTransaction();
    }

    static PetitionSignatureRow ReadSignature(Field* fields)
    {
        PetitionSignatureRow row;
        row.ownerGuid = fields[0].GetUInt32();
        row.petitionGuid = fields[1].GetUInt32();
        return row;
    }

    void QuerySignaturesBy(uint32 playerGuid, std::function<void(std::vector<PetitionSignatureRow>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRows<PetitionSignatureRow>(result, ReadSignature));
                                      },
                                      "SELECT `ownerguid`,`petitionguid` FROM `petition_sign` WHERE `playerguid` = '%u'",
                                      playerGuid);
    }

    void DeleteSignaturesBy(uint32 playerGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `playerguid` = '%u'", playerGuid);
    }

    void DeleteOwnedBy(uint32 ownerGuid)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `ownerguid` = '%u'", ownerGuid);
        CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `ownerguid` = '%u'", ownerGuid);
        CharacterDatabase.CommitTransaction();
    }

    bool ShowPetitionExists(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_SHOW_PETITION);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }).has_value();
    }

    std::vector<uint32> ShowSigners(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_SHOW_SIGNATURES);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static PetitionSignTargetRow ReadSignTarget(Field* fields)
    {
        PetitionSignTargetRow row;
        row.ownerGuid = fields[0].GetUInt32();
        row.signs = fields[1].GetUInt8();
        return row;
    }

    std::optional<PetitionSignTargetRow> SignReadPetition(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_SIGN_READ_PETITION);
        return ReadRow<PetitionSignTargetRow>(result, ReadSignTarget);
    }

    static PetitionAccountSignatureRow ReadAccountSignature(Field* fields)
    {
        PetitionAccountSignatureRow row;
        row.playerGuid = fields[0].GetUInt32();
        row.petitionGuid = fields[1].GetUInt32();
        return row;
    }

    std::optional<PetitionAccountSignatureRow> SignReadAccountSignature(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_SIGN_READ_ACCOUNT);
        return ReadRow<PetitionAccountSignatureRow>(result, ReadAccountSignature);
    }

    bool SignedBefore(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_SIGNED_BEFORE);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }).has_value();
    }

    bool SignedAfter(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_SIGNED_AFTER);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); }).has_value();
    }

    static PetitionTurnInRow ReadTurnIn(Field* fields)
    {
        PetitionTurnInRow row;
        row.ownerGuid = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        return row;
    }

    std::optional<PetitionTurnInRow> TurnInPetition(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_TURN_IN_PETITION);
        return ReadRow<PetitionTurnInRow>(result, ReadTurnIn);
    }

    std::vector<uint32> TurnInSigners(SqlQueryHolder* holder)
    {
        QueryResult* result = holder->GetResult(PETITION_TURN_IN_SIGNATURES);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::vector<PetitionSignatureRow> ReadHolderSignatures(SqlQueryHolder* holder, size_t slot)
    {
        QueryResult* result = holder->GetResult(slot);
        return ReadRows<PetitionSignatureRow>(result, ReadSignature);
    }
}

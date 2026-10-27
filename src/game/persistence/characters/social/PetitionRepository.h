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

#ifndef MANGOS_H_PETITIONREPOSITORY
#define MANGOS_H_PETITIONREPOSITORY

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"

class QueryResult;
class SqlQueryHolder;

/// The slots of the show-signatures and offer holder: whether the petition exists, and who has
/// signed it.
enum PetitionShowSlot
{
    PETITION_SHOW_PETITION      = 0,
    PETITION_SHOW_SIGNATURES    = 1,
    PETITION_SHOW_SLOT_COUNT    = 2
};

/// The slots of the sign request's read: the petition row (owner and the current count) and the
/// account's existing signatures.
enum PetitionSignReadSlot
{
    PETITION_SIGN_READ_PETITION = 0,
    PETITION_SIGN_READ_ACCOUNT  = 1,
    PETITION_SIGN_READ_COUNT    = 2
};

/// The slots of the atomic sign: whether the player had signed before, the conditional insert, and
/// whether the player has signed after.
enum PetitionSignedSlot
{
    PETITION_SIGNED_BEFORE      = 0,
    PETITION_SIGNED_INSERT      = 1,
    PETITION_SIGNED_AFTER       = 2,
    PETITION_SIGNED_COUNT       = 3
};

/// The slots of the turn-in holder: the petition row and its signatures.
enum PetitionTurnInSlot
{
    PETITION_TURN_IN_PETITION   = 0,
    PETITION_TURN_IN_SIGNATURES = 1,
    PETITION_TURN_IN_COUNT      = 2
};

/// The callbacks of the asynchronous reads: they run on the world thread with what the reads
/// returned, which they own.
typedef std::function<void(QueryResult*, SqlQueryHolder*)> PetitionHolderCallback;

/// A petition as its query reads it: its owner, name and number of signatures.
struct PetitionDetailsRow
{
    uint32 ownerGuid = 0;
    std::string name;
    uint8 signs = 0;
};

/// A petition as the sign request reads it: its owner and number of signatures.
struct PetitionSignTargetRow
{
    uint32 ownerGuid = 0;
    uint8 signs = 0;
};

/// One signature of an account, as the sign request reads it: the signer and the petition.
struct PetitionAccountSignatureRow
{
    uint32 playerGuid = 0;
    uint32 petitionGuid = 0;
};

/// One signature of a player, as the removal of their signatures reads it: the petition's owner
/// and the petition.
struct PetitionSignatureRow
{
    uint32 ownerGuid = 0;
    uint32 petitionGuid = 0;
};

/// A petition as the turn-in reads it: its owner and name.
struct PetitionTurnInRow
{
    uint32 ownerGuid = 0;
    std::string name;
};

/// The guild charters' saved rows (`petition`, `petition_sign`). Every read is asynchronous and
/// every write queued on the character database. A holder the database refuses (it is shutting
/// down) is deleted and its callback never runs.
namespace PetitionRepository
{
    /// Asks for the petitions a character owns: their guids.
    void QueryOwnedPetitions(uint32 ownerGuid, std::function<void(std::vector<uint32>)> callback);

    /// Asks whether a petition exists.
    void QueryExists(uint32 petitionGuid, std::function<void(bool)> callback);

    /// Asks for a petition's owner.
    void QueryOwner(uint32 petitionGuid, std::function<void(std::optional<uint32>)> callback);

    /// Asks for what a petition query answers: owner, name and the number of signatures.
    void QueryDetails(uint32 petitionGuid, std::function<void(std::optional<PetitionDetailsRow>)> callback);

    /// Asks, in one holder, whether a petition exists and who has signed it.
    void QueueShowSignatures(uint32 petitionGuid, PetitionHolderCallback callback);

    /// Asks, in one holder, for a petition's owner and count, and for an account's signatures.
    void QueueSignRead(uint32 petitionGuid, uint32 accountId, PetitionHolderCallback callback);

    /// Signs a petition in one holder: the player's signature before, an insert that adds it only
    /// when the petition exists, holds fewer than maxSigns and lacks this player, and the
    /// signature after.
    void QueueSign(uint32 petitionGuid, uint32 playerGuid, uint32 accountId, uint32 maxSigns, PetitionHolderCallback callback);

    /// Asks, in one holder, for a petition's owner and name and for its signatures.
    void QueueTurnInRead(uint32 petitionGuid, PetitionHolderCallback callback);

    /// The list a new charter's write deletes: the owner's old petitions, then the new one.
    std::string GuidList(std::vector<uint32> const& oldPetitions, uint32 petitionGuid);

    /// Writes a new charter in one transaction: the petitions and signatures in guidList deleted,
    /// then the charter's row, the name bound.
    void Create(uint32 ownerGuid, uint32 petitionGuid, std::string const& name, std::string const& guidList);

    /// Renames a petition, the name bound.
    void Rename(uint32 petitionGuid, std::string const& name);

    /// Deletes a petition and its signatures in one transaction.
    void Delete(uint32 petitionGuid);

    /// Asks for the signatures a character has made: each petition's owner and guid.
    void QuerySignaturesBy(uint32 playerGuid, std::function<void(std::vector<PetitionSignatureRow>)> callback);

    /// Deletes the signatures a character has made.
    void DeleteSignaturesBy(uint32 playerGuid);

    /// Deletes the petitions a character owns and their signatures, in one transaction.
    void DeleteOwnedBy(uint32 ownerGuid);

    /// Reads the show holder: whether the petition exists.
    bool ShowPetitionExists(SqlQueryHolder* holder);

    /// Reads the show holder: the guids of the petition's signers.
    std::vector<uint32> ShowSigners(SqlQueryHolder* holder);

    /// Reads the sign request's holder: the petition's owner and number of signatures.
    std::optional<PetitionSignTargetRow> SignReadPetition(SqlQueryHolder* holder);

    /// Reads the sign request's holder: the account's first signature.
    std::optional<PetitionAccountSignatureRow> SignReadAccountSignature(SqlQueryHolder* holder);

    /// Each reads the sign holder: whether the player was on the petition before the insert, and
    /// after it.
    bool SignedBefore(SqlQueryHolder* holder);
    bool SignedAfter(SqlQueryHolder* holder);

    /// Reads the turn-in holder: the petition's owner and name.
    std::optional<PetitionTurnInRow> TurnInPetition(SqlQueryHolder* holder);

    /// Reads the turn-in holder: the guids of the petition's signers.
    std::vector<uint32> TurnInSigners(SqlQueryHolder* holder);

    /// Reads a holder slot's signatures of a player, as QuerySignaturesBy reads them.
    std::vector<PetitionSignatureRow> ReadHolderSignatures(SqlQueryHolder* holder, size_t slot);
}

#endif

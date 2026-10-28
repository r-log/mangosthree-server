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

#include "session/handlers/social/ChatHandlers.h"
#include "session/handlers/social/ChatLinks.h"

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <cstring>
#include <string>
#include <ctime>
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "data/GameConfig.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "session/packets/chat/MessageChatPacket.h"
#include "ChannelMgr.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "spells/auras/SpellAuras.h"
#include "Language.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

/**
 * @brief Applies post-parse security checks to a chat message before broadcast.
 *
 * @param msg The chat message to validate and normalize.
 * @param lang The message language.
 * @return true if the message may continue processing; otherwise false.
 */
bool ChatHandlers::processChatmessageFurtherAfterSecurityChecks(WorldSession& session, std::string& msg, uint32 lang)
{
    if (lang != LANG_ADDON)
    {
        // strip invisible characters for non-addon messages
        if (sGameConfig.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
        {
            stripLineInvisibleChars(msg);
        }

        if (sGameConfig.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) && session.GetSecurity() < SEC_MODERATOR
            && !ChatLinks::IsValid(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chatmessage with an invalid link: %s", session.GetPlayer()->GetName(),
                          session.GetPlayer()->GetGUIDLow(), msg.c_str());
            if (sGameConfig.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
            {
                session.KickPlayer();
            }
            return false;
        }
    }

    return true;
}

/**
 * @brief Handles incoming chat packets for all chat message types.
 *
 * @param recv_data The incoming chat packet.
 */
void ChatHandlers::HandleMessagechat(WorldSession& session, WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    switch (recv_data.GetOpcode())
    {
        case CMSG_MESSAGECHAT_SAY:          type = CHAT_MSG_SAY;            break;
        case CMSG_MESSAGECHAT_YELL:         type = CHAT_MSG_YELL;           break;
        case CMSG_MESSAGECHAT_CHANNEL:      type = CHAT_MSG_CHANNEL;        break;
        case CMSG_MESSAGECHAT_WHISPER:      type = CHAT_MSG_WHISPER;        break;
        case CMSG_MESSAGECHAT_GUILD:        type = CHAT_MSG_GUILD;          break;
        case CMSG_MESSAGECHAT_OFFICER:      type = CHAT_MSG_OFFICER;        break;
        case CMSG_MESSAGECHAT_AFK:          type = CHAT_MSG_AFK;            break;
        case CMSG_MESSAGECHAT_DND:          type = CHAT_MSG_DND;            break;
        case CMSG_MESSAGECHAT_EMOTE:        type = CHAT_MSG_EMOTE;          break;
        case CMSG_MESSAGECHAT_PARTY:        type = CHAT_MSG_PARTY;          break;
        case CMSG_MESSAGECHAT_RAID:         type = CHAT_MSG_RAID;           break;
        case CMSG_MESSAGECHAT_BATTLEGROUND: type = CHAT_MSG_BATTLEGROUND;   break;
        case CMSG_MESSAGECHAT_RAID_WARNING: type = CHAT_MSG_RAID_WARNING;   break;
        default:
            sLog.outError("ChatHandlers::HandleMessagechat: Unknown chat opcode (0x%X)", recv_data.GetOpcode());
            recv_data.rfinish();
            return;
    }

    // no language sent with emote packet.
    if (type != CHAT_MSG_EMOTE && type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
    {
        recv_data >> lang;

        // prevent talking at unknown language (cheating)
        LanguageDesc const* langDesc = GetLanguageDescByID(lang);
        if (!langDesc)
        {
            session.SendNotification(LANG_UNKNOWN_LANGUAGE);
            return;
        }
        if (langDesc->skill_id != 0 && !session.GetPlayer()->HasSkill(langDesc->skill_id))
        {
            // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
            Unit::AuraList const& langAuras = session.GetPlayer()->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
            bool foundAura = false;
            for (Unit::AuraList::const_iterator i = langAuras.begin(); i != langAuras.end(); ++i)
            {
                if ((*i)->GetModifier()->m_miscvalue == int32(lang))
                {
                    foundAura = true;
                    break;
                }
            }
            if (!foundAura)
            {
                session.SendNotification(LANG_NOT_LEARNED_LANGUAGE);
                return;
            }
        }

        if (lang == LANG_ADDON)
        {
            // Disabled addon channel?
            if (!sGameConfig.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
            {
                return;
            }
        }
        // LANG_ADDON should not be changed nor be affected by flood control
        else
        {
            // send in universal language if player in .gmon mode (ignore spell effects)
            if (session.GetPlayer()->isGameMaster())
            {
                lang = LANG_UNIVERSAL;
            }
            else
            {
                // send in universal language in two side iteration allowed mode
                if (sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                {
                    lang = LANG_UNIVERSAL;
                }
                else
                {
                    switch (type)
                    {
                        case CHAT_MSG_PARTY:
                        case CHAT_MSG_PARTY_LEADER:
                        case CHAT_MSG_RAID:
                        case CHAT_MSG_RAID_LEADER:
                        case CHAT_MSG_RAID_WARNING:
                            // allow two side chat at group channel if two side group allowed
                            if (sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            {
                                lang = LANG_UNIVERSAL;
                            }
                            break;
                        case CHAT_MSG_GUILD:
                        case CHAT_MSG_OFFICER:
                            // allow two side chat at guild channel if two side guild allowed
                            if (sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            {
                                lang = LANG_UNIVERSAL;
                            }
                            break;
                    }
                }

                // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
                Unit::AuraList const& ModLangAuras = session.GetPlayer()->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
                if (!ModLangAuras.empty())
                {
                    lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
                }
            }

            if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
            {
                if (!session.GetPlayer()->CanSpeak())
                {
                    std::string timeStr = secsToTimeString(session.m_muteTime - time(NULL));
                    session.SendNotification(session.GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
                    return;
                }

                session.GetPlayer()->UpdateSpeakTime();
            }
        }
    }
    else
    {
        lang = LANG_UNIVERSAL;
    }

    DEBUG_LOG("CHAT: packet received. type %u lang %u", type, lang);

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (type == CHAT_MSG_SAY)
            {
                 session.GetPlayer()->Say(msg, lang);
            }
            else if (type == CHAT_MSG_EMOTE)
            {
                 session.GetPlayer()->TextEmote(msg);
            }
            else if (type == CHAT_MSG_YELL)
            {
                 session.GetPlayer()->Yell(msg, lang);
            }
         } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            uint32 toLength = recv_data.ReadBits(10);
            uint32 msgLength = recv_data.ReadBits(9);
            to = recv_data.ReadString(toLength);
            msg = recv_data.ReadString(msgLength);

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (!normalizePlayerName(to))
            {
                session.SendPlayerNotFoundNotice(to);
                {
                    break;
                }
            }

            Player* player = sObjectMgr.GetPlayer(to.c_str());
            uint32 tSecurity = session.GetSecurity();
            uint32 pSecurity = player ? player->GetSession()->GetSecurity() : SEC_PLAYER;
            if (!player || (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !player->isAcceptWhispers()))
            {
                session.SendPlayerNotFoundNotice(to);
                return;
            }

            if (!sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
            {
                if (session.GetPlayer()->GetTeam() != player->GetTeam())
                {
                    session.SendWrongFactionNotice();
                    return;
                }
            }

            session.GetPlayer()->Whisper(msg, lang, player->GetObjectGuid());
        } break;

        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // if player is in battleground, he can not say to battleground members by /p
            Group* group = session.GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = session.GetPlayer()->GetGroup();
                if (!group || group->isBGGroup())
                {
                    return;
                }
            }

            if ((type == CHAT_MSG_PARTY_LEADER) && !group->IsLeader(session.GetPlayer()->GetObjectGuid()))
            {
                return;
            }

            WorldPacket data;
            BuildMessageChatPacket(data, ChatMsg(type), msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false, group->GetMemberGroup(session.GetPlayer()->GetObjectGuid()));

            break;
        }
        case CHAT_MSG_GUILD:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (session.GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
                {
                    guild->BroadcastToGuild(&session, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (session.GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
                {
                    guild->BroadcastToOfficers(&session, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

            break;
        }
        case CHAT_MSG_RAID:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // if player is in battleground, he can not say to battleground members by /ra
            Group* group = session.GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = session.GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup())
                {
                    return;
                }
            }

            WorldPacket data;
            BuildMessageChatPacket(data, CHAT_MSG_RAID, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(&session).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // if player is in battleground, he can not say to battleground members by /ra
            Group* group = session.GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = session.GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(session.GetPlayer()->GetObjectGuid()))
                {
                    return;
                }
            }

            WorldPacket data;
            BuildMessageChatPacket(data, CHAT_MSG_RAID_LEADER, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isRaidGroup() ||
                !(group->IsLeader(session.GetPlayer()->GetObjectGuid()) || group->IsAssistant(session.GetPlayer()->GetObjectGuid())))
                {
                    return;
                }

            WorldPacket data;
            // in battleground, raid warning is sent only to players in battleground - code is ok
            BuildMessageChatPacket(data, CHAT_MSG_RAID_WARNING, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup())
            {
                return;
            }

            WorldPacket data;
            BuildMessageChatPacket(data, CHAT_MSG_BATTLEGROUND, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup() || !group->IsLeader(session.GetPlayer()->GetObjectGuid()))
            {
                return;
            }

            WorldPacket data;
            BuildMessageChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, msg.c_str(), Language(lang), session.GetPlayer()->GetChatTag(), session.GetPlayer()->GetObjectGuid(), session.GetPlayer()->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel, msg;
            uint32 channelLength = recv_data.ReadBits(10);
            uint32 msgLength = recv_data.ReadBits(9);
            msg = recv_data.ReadString(msgLength);
            channel = recv_data.ReadString(channelLength);

            if (!processChatmessageFurtherAfterSecurityChecks(session, msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (ChannelMgr* cMgr = channelMgr(session.GetPlayer()->GetTeam()))
            {
                if (Channel* chn = cMgr->GetChannel(channel, session.GetPlayer()))
                {
                    chn->Say(session.GetPlayer(), msg.c_str(), lang);
                }
            }
        } break;

        case CHAT_MSG_AFK:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (!session.GetPlayer()->IsInCombat())
            {
                if (session.GetPlayer()->isAFK())                       // Already AFK
                {
                    if (msg.empty())
                    {
                        session.GetPlayer()->ToggleAFK();                // Remove AFK
                    }
                    else
                    {
                        session.GetPlayer()->autoReplyMsg = msg;         // Update message
                    }
                }
                else                                        // New AFK mode
                {
                    session.GetPlayer()->autoReplyMsg = msg.empty() ? session.GetMangosString(LANG_PLAYER_AFK_DEFAULT) : msg;

                    if (session.GetPlayer()->isDND())
                    {
                        session.GetPlayer()->ToggleDND();
                    }

                    session.GetPlayer()->ToggleAFK();
                }
            }
            break;
        }
        case CHAT_MSG_DND:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(9));

            if (session.GetPlayer()->isDND())                           // Already DND
            {
                if (msg.empty())
                {
                    session.GetPlayer()->ToggleDND();                    // Remove DND
                }
                else
                {
                    session.GetPlayer()->autoReplyMsg = msg;             // Update message
                }
            }
            else                                            // New DND mode
            {
                session.GetPlayer()->autoReplyMsg = msg.empty() ? session.GetMangosString(LANG_PLAYER_DND_DEFAULT) : msg;

                if (session.GetPlayer()->isAFK())
                {
                    session.GetPlayer()->ToggleAFK();
                }

                session.GetPlayer()->ToggleDND();
            }

            break;
        }

        default:
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

void ChatHandlers::HandleAddonMessagechat(WorldSession& session, WorldPacket& recv_data)
{
    ChatMsg type;

    switch (recv_data.GetOpcode())
    {
        case CMSG_MESSAGECHAT_ADDON_BATTLEGROUND:   type = CHAT_MSG_BATTLEGROUND;   break;
        case CMSG_MESSAGECHAT_ADDON_GUILD:          type = CHAT_MSG_GUILD;          break;
        case CMSG_MESSAGECHAT_ADDON_OFFICER:        type = CHAT_MSG_OFFICER;        break;
        case CMSG_MESSAGECHAT_ADDON_PARTY:          type = CHAT_MSG_PARTY;          break;
        case CMSG_MESSAGECHAT_ADDON_RAID:           type = CHAT_MSG_RAID;           break;
        case CMSG_MESSAGECHAT_ADDON_WHISPER:        type = CHAT_MSG_WHISPER;        break;
        default:
            sLog.outError("ChatHandlers::HandleAddonMessagechat: Unknown addon chat opcode (0x%X)", recv_data.GetOpcode());
            recv_data.rfinish();
            return;
    }

    // Disabled addon channel?
    if (!sGameConfig.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
    {
        return;
    }

    switch (type)
    {
        case CHAT_MSG_BATTLEGROUND:
        {
            uint32 msgLen = recv_data.ReadBits(9);
            uint32 prefixLen = recv_data.ReadBits(5);
            std::string msg = recv_data.ReadString(msgLen);
            std::string prefix = recv_data.ReadString(prefixLen);

            Group* group = session.GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup())
            {
                return;
            }

            WorldPacket data;
            BuildMessageChatPacket(data, type, msg.c_str(), LANG_ADDON);
            group->BroadcastPacket(&data, false);
            break;
        }
        case CHAT_MSG_GUILD:
        {
            uint32 msgLen = recv_data.ReadBits(9);
            uint32 prefixLen = recv_data.ReadBits(5);
            std::string msg = recv_data.ReadString(msgLen);
            std::string prefix = recv_data.ReadString(prefixLen);

            if (session.GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
                {
                    guild->BroadcastAddonToGuild(&session, msg, prefix);
                }

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            uint32 prefixLen = recv_data.ReadBits(5);
            uint32 msgLen = recv_data.ReadBits(9);
            std::string prefix = recv_data.ReadString(prefixLen);
            std::string msg = recv_data.ReadString(msgLen);

            if (session.GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(session.GetPlayer()->GetGuildId()))
                {
                    guild->BroadcastAddonToOfficers(&session, msg, prefix);
                }
            break;
        }
        case CHAT_MSG_WHISPER:
        {
            uint32 msgLen = recv_data.ReadBits(9);
            uint32 prefixLen = recv_data.ReadBits(5);
            uint32 targetLen = recv_data.ReadBits(10);
            std::string msg = recv_data.ReadString(msgLen);
            std::string prefix = recv_data.ReadString(prefixLen);
            std::string targetName = recv_data.ReadString(targetLen);

            if (!normalizePlayerName(targetName))
            {
                break;
            }

            Player* receiver = sObjectMgr.GetPlayer(targetName.c_str());
            if (!receiver)
            {
                break;
            }

            WorldPacket data;
            BuildMessageChatPacket(data, type, msg.c_str(), LANG_UNIVERSAL, CHAT_TAG_NONE, ObjectGuid(), NULL, receiver->GetObjectGuid(), targetName.c_str(), NULL, 0, prefix.c_str());
            session.GetPlayer()->GetSession()->SendPacket(&data);
            break;
        }
        // Messages sent to "RAID" while in a party will get delivered to "PARTY"
        case CHAT_MSG_PARTY:
        case CHAT_MSG_RAID:
        {
            uint32 prefixLen = recv_data.ReadBits(5);
            uint32 msgLen = recv_data.ReadBits(9);
            std::string prefix = recv_data.ReadString(prefixLen);
            std::string msg = recv_data.ReadString(msgLen);

            Group* group = session.GetPlayer()->GetGroup();
            if (!group || group->isBGGroup())
            {
                break;
            }

            WorldPacket data;
            BuildMessageChatPacket(data, type, msg.c_str(), LANG_ADDON, CHAT_TAG_NONE, ObjectGuid(), NULL, ObjectGuid(), NULL, NULL, 0, prefix.c_str());
            group->BroadcastPacket(&data, false, group->GetMemberGroup(session.GetPlayer()->GetObjectGuid()));
            break;
        }
        default:
        {
            sLog.outError("ChatHandlers::HandleAddonMessagechat: unknown addon message type %u", type);
            break;
        }
    }
}

/**
 * @brief Handles a basic emote opcode from the client.
 *
 * @param recv_data The incoming emote packet.
 */
void ChatHandlers::HandleEmote(WorldSession& session, WorldPacket& recv_data)
{
    if (!session.GetPlayer()->IsAlive() || session.GetPlayer()->IsFeigningDeath())
    {
        return;
    }

    uint32 emote;
    recv_data >> emote;

    session.GetPlayer()->HandleEmoteCommand(emote);
}


namespace MaNGOS
{
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* nam = i_target ? i_target->GetNameForLocaleIdx(loc_idx) : NULL;
                uint32 namlen = (nam ? strlen(nam) : 0) + 1;

                data.Initialize(SMSG_TEXT_EMOTE, (20 + namlen));
                data << ObjectGuid(i_player.GetObjectGuid());
                data << uint32(i_text_emote);
                data << uint32(i_emote_num);
                data << uint32(namlen);
                if (namlen > 1)
                {
                    data.append(nam, namlen);
                }
                else
                {
                    data << uint8(0x00);
                }

                DEBUG_LOG("SMSG_TEXT_EMOTE i_text_emote %u i_emote_num %u",
                    i_text_emote, i_emote_num);
            }

        private:
            Player const& i_player;
            uint32        i_text_emote;
            uint32        i_emote_num;
            Unit const*   i_target;
    };
}                                                           // namespace MaNGOS

/**
 * @brief Handles text emotes, local broadcast, and creature emote notifications.
 *
 * @param recv_data The incoming text emote packet.
 */
void ChatHandlers::HandleTextEmote(WorldSession& session, WorldPacket& recv_data)
{
    if (!session.GetPlayer()->IsAlive())
    {
        return;
    }

    if (!session.GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(session.m_muteTime - time(NULL));
        session.SendNotification(session.GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    uint32 text_emote, emoteNum;
    ObjectGuid guid;

    recv_data >> text_emote;
    recv_data >> emoteNum;
    recv_data >> guid;

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
    {
        return;
    }

    uint32 emote_id = em->EmoteID;

    switch (emote_id)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        case EMOTE_STATE_DANCE:
        case EMOTE_STATE_READ:
            session.GetPlayer()->SetUInt32Value(UNIT_NPC_EMOTESTATE, emote_id);
        default:
        {
            // in feign death state allowed only text emotes.
            if (session.GetPlayer()->IsFeigningDeath())
            {
                break;
            }

            session.GetPlayer()->HandleEmoteCommand(emote_id);
            break;
        }
    }

    Unit* unit = session.GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*session.GetPlayer(), text_emote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(session.GetPlayer(), sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(session.GetPlayer(), emote_worker,  sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE, text_emote, 0, unit);

    // Send scripted event call
    if (unit && unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->AI())
    {
        ((Creature*)unit)->AI()->ReceiveEmote(session.GetPlayer(), text_emote);
    }
}

/**
 * @brief Notifies a player that their whisper target is ignoring them.
 *
 * @param recv_data The incoming ignored notification packet.
 */
void ChatHandlers::HandleChatIgnored(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid iguid;
    uint8 unk;
    // DEBUG_LOG("WORLD: Received opcode CMSG_CHAT_IGNORED");

    recv_data >> unk;                                       // probably related to spam reporting
    recv_data.ReadGuidMask<2, 5, 6, 4, 7, 0, 1, 3>(iguid);
    recv_data.ReadGuidBytes<0, 6, 5, 1, 4, 3, 7, 2>(iguid);

    Player* player = sObjectMgr.GetPlayer(iguid);
    if (!player || !player->GetSession())
    {
        return;
    }

    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_IGNORED, session.GetPlayer()->GetName(), LANG_UNIVERSAL, CHAT_TAG_NONE, session.GetPlayer()->GetObjectGuid());
    player->GetSession()->SendPacket(&data);
}

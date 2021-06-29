/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2015-2016 Symless Ltd.
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "server/ClientProxy1_7.h"
#include "server/Server.h"
#include "synergy/AppUtil.h"
#include "synergy/ProtocolUtil.h"
#include "base/TMethodEventJob.h"
#include "base/Log.h"

#include <algorithm>

//
// ClientProxy1_7
//

ClientProxy1_7::ClientProxy1_7(const String& name, synergy::IStream* stream, Server* server, IEventQueue* events) :
    ClientProxy1_6(name, stream, server, events),
    m_events(events)
{
    m_events->adoptHandler( m_events->forClientListener().connected(), this,
                            new TMethodEventJob<ClientProxy1_7>(this, &ClientProxy1_7::handleClientConnected));
}

ClientProxy1_7::~ClientProxy1_7()
{
    m_events->removeHandler(m_events->forClientListener().connected(), this);
}

void
ClientProxy1_7::handleClientConnected(const Event&, void*)
{
    String allKeyboardLayoutsStr;
    for (const auto& layout : AppUtil::instance().getKeyboardLayoutList()) {
        allKeyboardLayoutsStr += layout;
    }
    ProtocolUtil::writef(getStream(), kMsgDLanguageList, &allKeyboardLayoutsStr);
}

void
ClientProxy1_7::enter(SInt32 xAbs, SInt32 yAbs,
                UInt32 seqNum, KeyModifierMask mask, bool forScreensaver)
{
    auto currentLanguage = AppUtil::instance().getKeyboardLanguage();
    LOG((CLOG_DEBUG "send to \"%s\" start language \"%s\"", getName().c_str(), currentLanguage.c_str()));
    ProtocolUtil::writef(getStream(), kMsgDLanguageSet, &currentLanguage);

    ClientProxy1_6::enter(xAbs, yAbs, seqNum, mask, forScreensaver);
}

bool
ClientProxy1_7::parseMessage(const UInt8* code)
{
    if (memcmp(code, kMsgDLanguageList, 4) == 0) {
        langInfoReceived();
    }
    else if (memcmp(code, kMsgDLanguageSet, 4) == 0) {
        langSetReceived();
    }
    else {
        return ClientProxy1_6::parseMessage(code);
    }

    return true;
}

void
ClientProxy1_7::langInfoReceived()
{
    String clientLayoutList = "";
    ProtocolUtil::readf(getStream(), kMsgDLanguageList + 4, &clientLayoutList);

    String missedLanguages;
    String supportedLanguages;
    auto localLayouts = AppUtil::instance().getKeyboardLayoutList();
    for(int i = 0; i <= (int)clientLayoutList.size() - 2; i +=2) {
        auto layout = clientLayoutList.substr(i, 2);
        if (std::find(localLayouts.begin(), localLayouts.end(), layout) == localLayouts.end()) {
            missedLanguages += layout;
            missedLanguages += ' ';
        }
        else {
            supportedLanguages += layout;
            supportedLanguages += ' ';
        }
    }

    if(!supportedLanguages.empty()) {
        LOG((CLOG_DEBUG "Supported client languages: %s", supportedLanguages.c_str()));
    }

    if(!missedLanguages.empty()) {
        AppUtil::instance().showMessageBox("Language synchronization error",
                                           String("This languages are required for server proper work: ") + missedLanguages);
    }
}

void
ClientProxy1_7::langSetReceived()
{
    String languageCode = "";
    ProtocolUtil::readf(getStream(), kMsgDLanguageSet + 4, &languageCode);
    LOG((CLOG_DEBUG "Recieved new language from client: %s", languageCode.c_str()));
    AppUtil::instance().setKeyboardLanguage(languageCode);
}



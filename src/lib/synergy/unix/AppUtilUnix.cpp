/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2002 Chris Schoeneman
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

#include "base/Log.h"
#include "synergy/unix/AppUtilUnix.h"
#include "synergy/ArgsBase.h"
#include <thread>

#if WINAPI_XWINDOWS
#include <X11/XKBlib.h>
#include "synergy/unix/X11LayoutsParser.h"
#elif WINAPI_CARBON
#include <Carbon/Carbon.h>
#else
#error Platform not supported.
#endif

AppUtilUnix::AppUtilUnix(IEventQueue* events)
{
}

AppUtilUnix::~AppUtilUnix()
{
}

int
standardStartupStatic(int argc, char** argv)
{
    return AppUtil::instance().app().standardStartup(argc, argv);
}

int
AppUtilUnix::run(int argc, char** argv)
{
    return app().runInner(argc, argv, NULL, &standardStartupStatic);
}

void
AppUtilUnix::startNode()
{
    app().startNode();
}

std::vector<String>
AppUtilUnix::getKeyboardLayoutList()
{
    std::vector<String> layoutLangCodes;

#if WINAPI_XWINDOWS
    layoutLangCodes = X11LayoutsParser::getX11LanguageList();
#elif WINAPI_CARBON
    CFStringRef keys[] = { kTISPropertyInputSourceCategory };
    CFStringRef values[] = { kTISCategoryKeyboardInputSource };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1, NULL, NULL);
    CFArrayRef kbds = TISCreateInputSourceList(dict, false);

    for (CFIndex i = 0; i < CFArrayGetCount(kbds); ++i) {
        TISInputSourceRef keyboardLayout = (TISInputSourceRef)CFArrayGetValueAtIndex(kbds, i);
        auto layoutLanguages = (CFArrayRef)TISGetInputSourceProperty(keyboardLayout, kTISPropertyInputSourceLanguages);
        char temporaryCString[128] = {0};
        for(CFIndex index = 0; index < CFArrayGetCount(layoutLanguages) && layoutLanguages; index++) {
            auto languageCode = (CFStringRef)CFArrayGetValueAtIndex(layoutLanguages, index);
            if(!languageCode ||
               !CFStringGetCString(languageCode, temporaryCString, 128, kCFStringEncodingUTF8)) {
                continue;
            }

            std::string langCode(temporaryCString);
			if(langCode.size() != 2) {
				continue;
			}
			
            if(std::find(layoutLangCodes.begin(), layoutLangCodes.end(), langCode) == layoutLangCodes.end()){
                layoutLangCodes.push_back(langCode);
            }

            //Save only first language code
            break;
        }
    }
#endif

    return layoutLangCodes;
}

void
AppUtilUnix::showMessageBox(const String& title, const String& text)
{
    auto thr = std::thread([=]
    {
#if WINAPI_XWINDOWS
        system(String("DISPLAY=:0.0 /usr/bin/notify-send \"" + title + "\" \"" + text + "\"").c_str());
#elif WINAPI_CARBON
        CFStringRef titleStrRef = CFStringCreateWithCString(kCFAllocatorDefault, title.c_str(), kCFStringEncodingMacRoman);
        CFStringRef textStrRef = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(), kCFStringEncodingMacRoman);

        CFUserNotificationDisplayNotice(0, kCFUserNotificationNoteAlertLevel, NULL, NULL, NULL, titleStrRef, textStrRef, CFSTR("OK"));

        CFRelease(titleStrRef);
        CFRelease(textStrRef);
#endif
    });
    thr.detach();
}

void
AppUtilUnix::setKeyboardLanguage(String lang)
{
#if WINAPI_XWINDOWS
    auto allInstalled = getKeyboardLayoutList();
    int i = 0;
    for (; i < allInstalled.size(); ++i){
        if(allInstalled[i] == lang)
        {
            XkbStateRec state;
            auto display = XOpenDisplay(nullptr);
            XkbLockGroup(display, XkbUseCoreKbd, i);
            XCloseDisplay(display);
            LOG((CLOG_NOTE "Lang: \"%s\" is selected", lang.c_str()));
            break;
        }
    }

    if(i == allInstalled.size()) {
        LOG((CLOG_WARN "Failed to change keyboard layout. Requested language is not installed. Lang: \"%s\"", lang.c_str()));
    }
#elif WINAPI_CARBON
    CFStringRef keys[] = { kTISPropertyInputSourceCategory };
    CFStringRef values[] = { kTISCategoryKeyboardInputSource };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, (const void **)keys, (const void **)values, 1, NULL, NULL);
    CFArrayRef kbds = TISCreateInputSourceList(dict, false);

    for (CFIndex i = 0; i < CFArrayGetCount(kbds); ++i) {
        auto keyboardLayout = (TISInputSourceRef)CFArrayGetValueAtIndex(kbds, i);
        auto layoutLanguages = (CFArrayRef)TISGetInputSourceProperty(keyboardLayout, kTISPropertyInputSourceLanguages);
        char temporaryCString[128] = {0};
        for(CFIndex index = 0; index < CFArrayGetCount(layoutLanguages) && layoutLanguages; index++) {
            auto languageCode = (CFStringRef)CFArrayGetValueAtIndex(layoutLanguages, index);
            if(!languageCode ||
               !CFStringGetCString(languageCode, temporaryCString, 128, kCFStringEncodingUTF8)) {
                continue;
            }

            if(String(temporaryCString) != lang) {
                continue;
            }

            if (TISSelectInputSource(keyboardLayout) != noErr) {
                LOG((CLOG_WARN "Failed to change keyboard layout. Lang: \"%s\"", lang.c_str()));
            }

            LOG((CLOG_DEBUG "Language \"%s\" is selected!", lang.c_str()));
            return;
        }
    }

    LOG((CLOG_WARN "Failed to change keyboard layout. Requested language is not installed. Lang: \"%s\"", lang.c_str()));
#endif
}

String
AppUtilUnix::getKeyboardLanguage()
{
#if WINAPI_XWINDOWS
    XkbStateRec state;
    auto display = XOpenDisplay(nullptr);
    XkbGetState(display, XkbUseCoreKbd, &state);
    XkbDescPtr desc = XkbGetKeyboard(display, XkbAllComponentsMask, XkbUseCoreKbd);
    String tempcode(XGetAtomName(display, desc->names->groups[state.group]));
    XCloseDisplay(display);
    return X11LayoutsParser::getIsoCodeByX11LanguageName(tempcode);
#elif WINAPI_CARBON
    auto layoutLanguages = (CFArrayRef)TISGetInputSourceProperty(TISCopyCurrentKeyboardInputSource(), kTISPropertyInputSourceLanguages);
    char temporaryCString[128] = {0};
    for(CFIndex index = 0; index < CFArrayGetCount(layoutLanguages) && layoutLanguages; index++) {
        auto languageCode = (CFStringRef)CFArrayGetValueAtIndex(layoutLanguages, index);
        if(!languageCode ||
           !CFStringGetCString(languageCode, temporaryCString, 128, kCFStringEncodingUTF8)) {
            continue;
        }

        std::string langCode(temporaryCString);
        if(langCode.size() != 2) {
            continue;
        }

        return langCode;
    }
#endif
}

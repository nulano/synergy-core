/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2015 Synergy Si Ltd.
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

#import "OSXHelpers.h"

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import <Cocoa/Cocoa.h>
#import <UserNotifications/UNNotification.h>
#import <UserNotifications/UNUserNotificationCenter.h>
#import <UserNotifications/UNNotificationContent.h>
#import <UserNotifications/UNNotificationTrigger.h>

#import <QtGlobal>

void requestOSXNotificationPermission()
{
#if OSX_DEPLOYMENT_TARGET >= 1014
	if (isOSXDevelopmentBuild())
	{
		qWarning("Not requesting notification permission in dev build");
		return;
	}

	UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
	[center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert + UNAuthorizationOptionSound)
		completionHandler:^(BOOL granted, NSError * _Nullable error) {
		if(error != nil)
		{
			qWarning("Notification permission request error: %s", [[NSString stringWithFormat:@"%@", error] UTF8String]);
		}
	}];
#endif
}

bool
isOSXDevelopmentBuild()
{
	std::string bundleURL = [[[NSBundle mainBundle] bundleURL].absoluteString UTF8String];
	return (bundleURL.find("Applications/Synergy.app") == std::string::npos);
}

bool
showOSXNotification(const QString& title, const QString& body)
{
    NSUserNotification *userNotification = [NSUserNotification new];
    userNotification.title = title.toNSString();
    userNotification.informativeText = body.toNSString();

    NSUserNotificationCenter *center = [NSUserNotificationCenter defaultUserNotificationCenter];
    [center scheduleNotification:userNotification];
    return true;
}

bool
isOSXInterfaceStyleDark()
{
   // Implementation from http://stackoverflow.com/a/26472651
   NSDictionary* dict = [[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain];
   id style = [dict objectForKey:@"AppleInterfaceStyle"];
   return (style && [style isKindOfClass:[NSString class]] && NSOrderedSame == [style caseInsensitiveCompare:@"dark"]);
}

IconsTheme
getOSXIconsTheme()
{
   if (@available(macOS 11, *))
      return IconsTheme::ICONS_TEMPLATE;
   else if(isOSXInterfaceStyleDark())
      return IconsTheme::ICONS_DARK;
   return IconsTheme::ICONS_LIGHT;
}

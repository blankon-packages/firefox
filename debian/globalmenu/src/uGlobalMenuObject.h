/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 *	 Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is globalmenu-extension.
 *
 * The Initial Developer of the Original Code is
 * Canonical Ltd.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Coulson <chris.coulson@canonical.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */

#ifndef _U_GLOBALMENUBASE_H
#define _U_GLOBALMENUBASE_H

#include <nsIContent.h>
#include <nsCOMPtr.h>
#include <nsAutoPtr.h>
#include <imgIDecoderObserver.h>
#include <imgIRequest.h>
#include <imgIContainerObserver.h>
#include <nsThreadUtils.h>

#include <libdbusmenu-glib/server.h>

#include "uGlobalMenuDocListener.h"

// The menuitem attributes need updating
#define UNITY_MENUITEM_IS_DIRTY           (1 << 0)

// The content node says that this menuitem should be visible
#define UNITY_MENUITEM_CONTENT_IS_VISIBLE (1 << 1)

// This menuitem is ignoring DOM events
#define UNITY_MENUITEM_EVENTS_ARE_BLOCKED (1 << 2)

// The icon for this menuitem should always be shown (eg, bookmarks)
#define UNITY_MENUITEM_ALWAYS_SHOW_ICON   (1 << 3)

// This menuitem should only be visible when its menu is opened by the keyboard
#define UNITY_MENUITEM_SHOW_ONLY_FOR_KB   (1 << 4)

// The menu is in the process of opening
#define UNITY_MENU_IS_OPEN_OR_OPENING     (1 << 5)

// The menu needs rebuilding
#define UNITY_MENU_NEEDS_REBUILDING       (1 << 6)

// The shell sent the first "AboutToOpen" event
#define UNITY_MENU_READY                  (1 << 7)

// This menuitem is a checkbox or radioitem which is active
#define UNITY_MENUITEM_TOGGLE_IS_ACTIVE   (1 << 8)

// This menuitem is a checkbox
#define UNITY_MENUITEM_IS_CHECKBOX        (1 << 9)

// This menuitem is a radio item
#define UNITY_MENUITEM_IS_RADIO           (1 << 10)

// The menubar has been registered with the shell
#define UNITY_MENUBAR_IS_REGISTERED       (1 << 11)

enum uMenuObjectType {
  eMenuBar,
  eMenu,
  eMenuItem,
  eMenuSeparator,
  eMenuDummy
};

enum uMenuObjectProperties {
  eLabel = (1 << 0),
  eEnabled = (1 << 1),
  eVisible = (1 << 2),
  eIconData = (1 << 3),
  eType = (1 << 4),
  eShortcut = (1 << 5),
  eToggleType = (1 << 6),
  eToggleState = (1 << 7),
  eChildDisplay = (1 << 8)
};

class uGlobalMenuObject;
class uGlobalMenuBar;

#define UNITY_MENU_BLOCK_EVENTS_FOR_CURRENT_SCOPE()                                \
  uGlobalMenuScopedFlagsRestorer _event_blocker(this,                               \
                                                UNITY_MENUITEM_EVENTS_ARE_BLOCKED); \
  mFlags = mFlags | UNITY_MENUITEM_EVENTS_ARE_BLOCKED;

#define UNITY_MENU_ENSURE_EVENTS_UNBLOCKED()                       \
  if (mFlags & UNITY_MENUITEM_EVENTS_ARE_BLOCKED) {                \
    DEBUG_WITH_THIS_MENUOBJECT("Events are blocked on this node"); \
    return;                                                        \
  }

class uGlobalMenuIconLoader: public imgIDecoderObserver,
                             public nsRunnable
{
  friend class uGlobalMenuObject;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_NSIRUNNABLE

  void LoadIcon();

  uGlobalMenuIconLoader(uGlobalMenuObject *aMenuItem):
                        mMenuItem(aMenuItem) { };
  ~uGlobalMenuIconLoader() { };

protected:
  void Destroy();

private:
  void ClearIcon();
  bool ShouldShowIcon();

  bool mIconLoaded;
  uGlobalMenuObject *mMenuItem;
  nsCOMPtr<nsIContent> mContent;
  nsCOMPtr<imgIRequest> mIconRequest;
  nsIntRect mImageRect;
  static PRPackedBool sImagesInMenus;
};

class uGlobalMenuObject
{
public:
  uGlobalMenuObject (uMenuObjectType aType): mDbusMenuItem(nsnull),
                                             mListener(nsnull),
                                             mParent(nsnull),
                                             mType(aType),
                                             mFlags(0)
                                             { };
  DbusmenuMenuitem* GetDbusMenuItem();
  void SetDbusMenuItem(DbusmenuMenuitem *aDbusMenuItem);
  uGlobalMenuObject* GetParent() { return mParent; }
  uMenuObjectType GetType() { return mType; }
  nsIContent* GetContent() { return mContent; }
  virtual void AboutToShowNotify() { };
  virtual ~uGlobalMenuObject() { };

protected:
  virtual void InitializeDbusMenuItem()=0;
  void SyncLabelFromContent(nsIContent *aContent);
  void SyncLabelFromContent();
  void SyncVisibilityFromContent();
  void SyncSensitivityFromContent(nsIContent *aContent);
  void SyncSensitivityFromContent();
  void SyncIconFromContent();
  void UpdateInfoFromContentClass();
  void UpdateVisibility();
  void DestroyIconLoader();
  bool IsHidden();
  void Invalidate() { mFlags = mFlags | UNITY_MENUITEM_IS_DIRTY; }
  void ClearInvalid() { mFlags = mFlags & ~UNITY_MENUITEM_IS_DIRTY; }
  bool IsDirty() { return !!(mFlags & UNITY_MENUITEM_IS_DIRTY); }
  void OnlyKeepProperties(uMenuObjectProperties aKeep);

  bool ShouldShowOnlyForKb()
  {
    return !!(mFlags & UNITY_MENUITEM_SHOW_ONLY_FOR_KB);
  }

  void SetFlags(PRUint16 aFlags) { mFlags = mFlags | aFlags; }
  void ClearFlags(PRUint16 aFlags) { mFlags = mFlags & ~aFlags; }

  void SetOrClearFlags(bool aSet, PRUint16 aFlags)
  {
    if (aSet) {
      SetFlags(aFlags);
    } else {
      ClearFlags(aFlags);
    }
  }

  nsCOMPtr<nsIContent> mContent;
  DbusmenuMenuitem *mDbusMenuItem;
  nsRefPtr<uGlobalMenuDocListener> mListener;
  uGlobalMenuObject *mParent;
  uMenuObjectType mType;
  uGlobalMenuBar *mMenuBar;

protected:
  friend class uGlobalMenuScopedFlagsRestorer;

  PRUint16 mFlags;

protected:
  friend class uGlobalMenuIconLoader;

  bool ShouldAlwaysShowIcon()
  {
    return !!(mFlags & UNITY_MENUITEM_ALWAYS_SHOW_ICON);
  }

private:
  nsRefPtr<uGlobalMenuIconLoader> mIconLoader;
};

class uGlobalMenuScopedFlagsRestorer
{
public:
  uGlobalMenuScopedFlagsRestorer(uGlobalMenuObject *aMenuObject, PRUint16 aMask = 0):
                                 mMenuObject(aMenuObject), mMask(aMask)
  {
    mFlags = mMenuObject->mFlags;
    mMask = aMask;
  }

  void Cancel() { mMenuObject = nsnull; }

  void ClearFlagWhenDone(PRUint16 aFlags)
  {
    mMask = mMask | aFlags;
    mFlags = mFlags & ~aFlags;
  }

  ~uGlobalMenuScopedFlagsRestorer()
  {
    if (mMenuObject) {
      mMenuObject->mFlags = (mMenuObject->mFlags & ~mMask) | (mFlags & mMask);
    }
  }

private:
  uGlobalMenuObject *mMenuObject;
  PRUint16 mFlags;
  PRUint16 mMask;
};

#endif

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

#ifndef _U_GLOBALMENU_H
#define _U_GLOBALMENU_H

#include <prtypes.h>
#include <nsTArray.h>
#include <nsAutoPtr.h>
#include <nsCOMPtr.h>
#include <nsIContent.h>
#include <nsThreadUtils.h>

#include <libdbusmenu-glib/server.h>

#include "uGlobalMenuObject.h"

// The menu is in the process of opening
#define UNITY_MENU_IS_OPEN_OR_OPENING     (1 << 7)

// The menu needs rebuilding
#define UNITY_MENU_NEEDS_REBUILDING       (1 << 8)

// The shell sent the first "AboutToOpen" event
#define UNITY_MENU_READY                  (1 << 9)

class uGlobalMenuItem;
class uGlobalMenuBar;
class uGlobalMenuDocListener;

class uGlobalMenu: public uGlobalMenuObject
{
public:
  static uGlobalMenuObject* Create(uGlobalMenuObject *aParent,
                                   uGlobalMenuDocListener *aListener,
                                   nsIContent *aContent,
                                   uGlobalMenuBar *aMenuBar);

  ~uGlobalMenu();

  bool CanOpen();
  void OpenMenuDelayed();
  void Invalidate();
  void ContainerIsOpening();
  
  bool IsOpenOrOpening() { return !!(mFlags & UNITY_MENU_IS_OPEN_OR_OPENING); }

protected:
  void ObserveAttributeChanged(nsIDocument *aDocument,
                               nsIContent *aContent,
                               nsIAtom *aAttribute);
  void ObserveContentRemoved(nsIDocument *aDocument,
                             nsIContent *aContainer,
                             nsIContent *aChild,
                             PRInt32 aIndexInContainer);
  void ObserveContentInserted(nsIDocument *aDocument,
                              nsIContent *aContainer,
                              nsIContent *aChild,
                              PRInt32 aIndexInContainer);
  void Refresh();

private:
  uGlobalMenu();

  // Initialize the menu structure
  nsresult Init(uGlobalMenuObject *aParent,
                uGlobalMenuDocListener *aListener,
                nsIContent *aContent,
                uGlobalMenuBar *aMenuBar);
  bool InsertMenuObjectAt(uGlobalMenuObject *menuObj,
                          PRUint32 index);
  bool AppendMenuObject(uGlobalMenuObject *menuObj);
  bool RemoveMenuObjectAt(PRUint32 index);
  void InitializeDbusMenuItem();
  nsresult Build();
  void GetMenuPopupFromMenu(nsIContent **aResult);
  static bool MenuAboutToOpenCallback(DbusmenuMenuitem *menu,
                                      void *data);
  static bool MenuEventCallback(DbusmenuMenuitem *menu,
                                const gchar *name,
                                GVariant *value,
                                guint timestamp,
                                void *data);
  static gboolean DoOpen(gpointer user_data);
  void AboutToOpen();
  void OnOpen();
  void OnClose();
  void Activate();
  void Deactivate();
  void SetNeedsRebuild() { mFlags = mFlags | UNITY_MENU_NEEDS_REBUILDING; }
  void ClearNeedsRebuild() { mFlags = mFlags & ~UNITY_MENU_NEEDS_REBUILDING; }
  bool DoesNeedRebuild() { return !!(mFlags & UNITY_MENU_NEEDS_REBUILDING); }
  void FreeRecycleList() { mRecycleList = nsnull; }

  struct RecycleList
  {
    RecycleList(uGlobalMenu *aMenu, PRUint32 aMarker);
    ~RecycleList();

    void Empty();
    DbusmenuMenuitem* Shift();
    void Unshift(DbusmenuMenuitem *aItem);
    void Push(DbusmenuMenuitem *aItem);

    PRUint32 mMarker;
    nsTArray<DbusmenuMenuitem *> mList;
    uGlobalMenu *mMenu;
    nsRefPtr<nsRunnableMethod<uGlobalMenu, void, false> > mFreeEvent;
  };

  nsCOMPtr<nsIContent> mPopupContent;
  nsTArray< nsAutoPtr<uGlobalMenuObject> > mMenuObjects;
  nsAutoPtr<RecycleList> mRecycleList;
};

#endif

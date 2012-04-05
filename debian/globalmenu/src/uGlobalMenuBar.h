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

#ifndef _U_GLOBALMENUBAR_H
#define _U_GLOBALMENUBAR_H

#include <prtypes.h>
#include <nsTArray.h>
#include <nsAutoPtr.h>
#include <nsStringAPI.h>
#include <nsIDOMEventTarget.h>
#include <nsIContent.h>
#include <nsIDOMEventListener.h>

#include <libdbusmenu-glib/server.h>
#include <gtk/gtk.h>

#include "uMenuChangeObserver.h"
#include "uGlobalMenuObject.h"
#include "uGlobalMenuUtils.h"

// Meh. X.h defines this
#ifdef KeyPress
# undef KeyPress
#endif

class nsIObserver;
class nsIWidget;
class nsIDOMEvent;
class uGlobalMenuService;
class uGlobalMenu;
class uGlobalMenuBar;
class nsIDOMKeyEvent;

class uGlobalMenuBarListener: public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  uGlobalMenuBarListener(uGlobalMenuBar *aMenuBar):
                         mMenuBar(aMenuBar) { };
  ~uGlobalMenuBarListener() { };

private:
  uGlobalMenuBar *mMenuBar;
};

class uGlobalMenuBar: public uGlobalMenuObject,
                      public uMenuChangeObserver
{
public:
  NS_DECL_UMENUCHANGEOBSERVER

  static uGlobalMenuBar* Create(nsIWidget *aWindow,
                                nsIContent *aMenuBar);
  ~uGlobalMenuBar();

  // Checks if the menubar shares the same top level window as the 
  // specified nsIWidget
  bool WidgetHasSameToplevelWindow(nsIWidget *aWidget);

  // Returns whether the menu was opened via a keyboard shortcut
  bool OpenedByKeyboard() { return !!mOpenedByKeyboard; }

  bool IsRegistered() { return !!(mFlags & UNITY_MENUBAR_IS_REGISTERED); }

private:
  friend class uGlobalMenuService;

  void NotifyMenuBarRegistered();

private:
  friend class uGlobalMenuBarListener;

  void Focus();
  void Blur();
  nsresult KeyPress(nsIDOMEvent *aKeyEvent);
  nsresult KeyUp(nsIDOMEvent *aKeyEvent);
  nsresult KeyDown(nsIDOMEvent *aKeyEvent);

private:
  uGlobalMenuBar();
  // Initialize the menu structure
  nsresult Init(nsIWidget *aWindow,
                nsIContent *aMenuBar);

  void InitializeDbusMenuItem();
  GtkWidget* WidgetToGTKWindow(nsIWidget *aWidget);
  nsresult Build();
  PRUint32 GetModifiersFromEvent(nsIDOMKeyEvent *aKeyEvent);
  bool ShouldHandleKeyEvent(nsIDOMEvent *aKeyEvent);

  bool RemoveMenuObjectAt(PRUint32 index);
  bool InsertMenuObjectAt(uGlobalMenuObject *menu,
                          PRUint32 index);
  bool AppendMenuObject(uGlobalMenuObject *menu);
  bool ShouldParentStayVisible(nsIContent *aContent);
  bool IsParentOfMenuBar(nsIContent *aContent);
  void HideXULMenuBar();
  void ShowXULMenuBar();

  DbusmenuServer *mServer;
  GtkWidget *mTopLevel;

  nsCOMPtr<nsIContent> mHiddenElement;
  nsCOMPtr<nsIDOMEventTarget> mDocTarget;
  bool mRestoreHidden;
  bool mXULMenuHidden;
  nsRefPtr<uGlobalMenuBarListener> mEventListener;
  PRInt32 mAccessKey;
  PRUint32 mAccessKeyMask;
  bool mOpenedByKeyboard;
  nsAutoPtr<uGlobalMenuRequestAutoCanceller> mCancellable;

  // Should probably have a container class and subclass that
  nsTArray< nsAutoPtr<uGlobalMenuObject> > mMenuObjects;
};

#endif

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
 * Mike Conley <mconley@mozillamessaging.com>
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

#include <nsIAtom.h>
#include <nsDebug.h>
#include <nsIContent.h>

#include <glib-object.h>
#include <libdbusmenu-glib/server.h>

#include "uGlobalMenuDummy.h"
#include "uGlobalMenuBar.h"
#include "uWidgetAtoms.h"

#include "uDebug.h"

nsresult
uGlobalMenuDummy::ConstructDbusMenuItem()
{
  mDbusMenuItem = dbusmenu_menuitem_new();
  if (!mDbusMenuItem)
    return NS_ERROR_OUT_OF_MEMORY;

  dbusmenu_menuitem_property_set_bool(mDbusMenuItem,
                                      DBUSMENU_MENUITEM_PROP_VISIBLE,
                                      FALSE);

  return NS_OK;
}

uGlobalMenuDummy::uGlobalMenuDummy():
  uGlobalMenuObject(MenuDummy)
{
  MOZ_COUNT_CTOR(uGlobalMenuDummy);
}

uGlobalMenuDummy::~uGlobalMenuDummy()
{

  if (mDbusMenuItem)
    g_object_unref(mDbusMenuItem);

  MOZ_COUNT_DTOR(uGlobalMenuDummy);
}

nsresult
uGlobalMenuDummy::Init()
{
  mParent = nsnull;
  mMenuBar = nsnull;

  return ConstructDbusMenuItem();
}

/*static*/ uGlobalMenuObject*
uGlobalMenuDummy::Create()
{
  TRACE();

  uGlobalMenuDummy *menuitem = new uGlobalMenuDummy();
  if (!menuitem) {
    return nsnull;
  }

  if (NS_FAILED(menuitem->Init())) {
    delete menuitem;
    return nsnull;
  }

  return static_cast<uGlobalMenuObject *>(menuitem);
}

// GlobalMenuDummy is inert, and does not observe/react to content
// changes.  The following methods are included simply to fill out
// the interface.
void
uGlobalMenuDummy::ObserveAttributeChanged(nsIDocument *aDocument,
                                              nsIContent *aContent,
                                              nsIAtom *aAttribute)
{

}

void
uGlobalMenuDummy::ObserveContentRemoved(nsIDocument *aDocument,
                                            nsIContent *aContainer,
                                            nsIContent *aChild,
                                            PRInt32 aIndexInContainer)
{

}

void
uGlobalMenuDummy::ObserveContentInserted(nsIDocument *aDocument,
                                             nsIContent *aContainer,
                                             nsIContent *aChild,
                                             PRInt32 aIndexInContainer)
{

}

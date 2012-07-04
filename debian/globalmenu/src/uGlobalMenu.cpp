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

#include <nsDebug.h>
#include <nsIAtom.h>
#include <nsIDOMEvent.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMWindow.h>
#include <nsIDOMDocument.h>
#include <nsStringAPI.h>
#include <nsIDOMEventTarget.h>
#if MOZILLA_BRANCH_MAJOR_VERSION < 16
# include <nsIPrivateDOMEvent.h>
#endif
#include <nsPIDOMWindow.h>
#include <nsIDOMXULCommandEvent.h>
#include <nsIXPConnect.h>
#include <nsIScriptGlobalObject.h>
#include <nsIScriptContext.h>
#include <jsapi.h>
#include <mozilla/dom/Element.h>
#if MOZILLA_BRANCH_MAJOR_VERSION < 15
# include <nsIXBLService.h>
#endif

#include <glib-object.h>

#include "uGlobalMenuService.h"
#include "uGlobalMenu.h"
#include "uGlobalMenuBar.h"
#include "uGlobalMenuUtils.h"
#include "uWidgetAtoms.h"

#include "uDebug.h"

#include "compat.h"

uGlobalMenu::RecycleList::RecycleList(uGlobalMenu *aMenu,
                                      PRUint32 aMarker):
  mMarker(aMarker), mMenu(aMenu)
{
  mFreeEvent = NS_NewNonOwningRunnableMethod(mMenu,
                                             &uGlobalMenu::FreeRecycleList);
  NS_DispatchToCurrentThread(mFreeEvent);
}

uGlobalMenu::RecycleList::~RecycleList()
{
  Empty();
  mFreeEvent->Revoke();
}

void
uGlobalMenu::RecycleList::Empty()
{
  while(mList.Length() > 0) {
    dbusmenu_menuitem_child_delete(mMenu->GetDbusMenuItem(), Shift());
  }
}

DbusmenuMenuitem*
uGlobalMenu::RecycleList::Shift()
{
  if (mList.Length() == 0) {
    return nsnull;
  }

  ++mMarker;
  DbusmenuMenuitem *recycled = mList[0];
  mList.RemoveElementAt(0);

  return recycled;
}

void
uGlobalMenu::RecycleList::Unshift(DbusmenuMenuitem *aItem)
{
  if (mList.Length() == 0) {
    --mMarker;
  }

  mList.InsertElementAt(0, aItem);
}

void
uGlobalMenu::RecycleList::Push(DbusmenuMenuitem *aItem)
{
  mList.AppendElement(aItem);
}

/*static*/ bool
uGlobalMenu::MenuEventCallback(DbusmenuMenuitem *menu,
                               const gchar *name,
                               GVariant *value,
                               guint timestamp,
                               void *data)
{
  uGlobalMenu *self = static_cast<uGlobalMenu *>(data);
  if (!g_strcmp0("closed", name)) {
    self->OnClose();
    return true;
  }

  if (!g_strcmp0("opened", name)) {
    self->OnOpen();
    return true;
  }

  return false;
}

/*static*/ bool
uGlobalMenu::MenuAboutToOpenCallback(DbusmenuMenuitem *menu,
                                     void *data)
{
  uGlobalMenu *self = static_cast<uGlobalMenu *>(data);
  self->AboutToOpen();

  // We return false here for "needsUpdate", as we have no way of
  // knowing in advance if the menu structure is going to be updated.
  // The menu layout will still update on the client, but we won't block
  // opening the menu until it's happened
  return false;
}

static void
DispatchEvent(nsIContent *aContent, const nsAString& aType)
{
  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aContent);
  if (target) {
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aContent->OwnerDoc());
    if (domDoc) {
      nsCOMPtr<nsIDOMEvent> event;
      domDoc->CreateEvent(NS_LITERAL_STRING("Events"),
                          getter_AddRefs(event));
      if (event) {
        event->InitEvent(aType, true, true);
        nsCOMPtr<nsIPrivateDOMEvent> priv = do_QueryInterface(event);
        if (priv) {
          priv->SetTrusted(true);
        }
        bool dummy;
        target->DispatchEvent(event, &dummy);
      }
    }
  }
}

void
uGlobalMenu::Activate()
{
  mContent->SetAttr(kNameSpaceID_None, uWidgetAtoms::menuactive,
                    NS_LITERAL_STRING("true"), true);

  DispatchEvent(mContent, NS_LITERAL_STRING("DOMMenuItemActive"));
}

void
uGlobalMenu::Deactivate()
{
  mContent->UnsetAttr(kNameSpaceID_None, uWidgetAtoms::menuactive, true);

  DispatchEvent(mContent, NS_LITERAL_STRING("DOMMenuItemInactive"));
}

bool
uGlobalMenu::CanOpen()
{
    bool isHidden = IsHidden();
    bool isDisabled = mContent->AttrValueIs(kNameSpaceID_None,
                                            uWidgetAtoms::disabled,
                                            uWidgetAtoms::_true,
                                            eCaseMatters);

    return (!isHidden && !isDisabled);
}

static void
DispatchMouseEvent(nsIContent *aPopup, const nsAString& aType)
{
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aPopup->OwnerDoc());
  if (domDoc) {
    nsCOMPtr<nsIDOMEvent> event;
    domDoc->CreateEvent(NS_LITERAL_STRING("mouseevent"),
                        getter_AddRefs(event));
    if (event) {
      nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(event);
      if (mouseEvent) {
        nsCOMPtr<nsIDOMWindow> window;
        domDoc->GetDefaultView(getter_AddRefs(window));
        if (window) {
          mouseEvent->InitMouseEvent(aType, true, true, window, nsnull,
                                     0, 0, 0, 0, false, false, false, false,
                                     0, nsnull);
          nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aPopup);
          if (target) {
            nsCOMPtr<nsIPrivateDOMEvent> priv = do_QueryInterface(event);
            if (priv) {
              priv->SetTrusted(true);
            }
            bool dummy;
            // XXX: dummy == false means that we should prevent the
            //      the menu from opening, but there's no way to do this
            target->DispatchEvent(event, &dummy);
          }
        }
      }
    }
  }
}

void
uGlobalMenu::AboutToOpen()
{
  TRACETM();

  // XXX: We ignore the first AboutToOpen on top-level menus, because Unity
  //      sends this signal on all top-levels when the window opens.
  //      This isn't useful for us and it doesn't finish the job by sending
  //      open/close events, so we end up in a state where we resend the
  //      entire menu structure over dbus on every page navigation
  if (!(mFlags & UNITY_MENU_READY)) {
    LOGTM("Ignoring first AboutToOpen");
    SetFlags(UNITY_MENU_READY);
    return;
  }

  if (DoesNeedRebuild()) {
    Build();
  }

  SetFlags(UNITY_MENU_IS_OPEN_OR_OPENING);

  // If there is no popup content, then there is nothing to do, and it's
  // unsafe to proceed anyway
  if (!mPopupContent) {
    LOGTM("Menu has no popup content");
    return;
  }

  // Tell children we are opening. This will cause invalidated children
  // to refresh
  PRUint32 count = mMenuObjects.Length();
  for (PRUint32 i = 0; i < count; i++) {
    mMenuObjects[i]->ContainerIsOpening();
  }

  // XXX: This should happen when the pointer hovers over the menu entry,
  //      but we don't have that information right now. We synthesize it for
  //      menus, but this doesn't work for menuitems at all
  Activate();

  DispatchMouseEvent(mPopupContent, NS_LITERAL_STRING("popupshowing"));
}

void
uGlobalMenu::OnOpen()
{
  if (!IsOpenOrOpening()) {
    // If we didn't receive an AboutToOpen, then generate it ourselves
    AboutToOpen();
  }

  mContent->SetAttr(kNameSpaceID_None, uWidgetAtoms::open, NS_LITERAL_STRING("true"), true);

  // If there is no popup content, then there is nothing to do, and it's
  // unsafe to proceed anyway
  if (!mPopupContent) {
    return;
  }

  DispatchMouseEvent(mPopupContent, NS_LITERAL_STRING("popupshown"));
}

void
uGlobalMenu::OnClose()
{
  mContent->UnsetAttr(kNameSpaceID_None, uWidgetAtoms::open, true);

  PRUint32 count = mMenuObjects.Length();
  for (PRUint32 i = 0; i < count; i++) {
    mMenuObjects[i]->ContainerIsClosing();
  }

  ClearFlags(UNITY_MENU_IS_OPEN_OR_OPENING);

  // If there is no popup content, then there is nothing to do, and it's
  // unsafe to proceed anyway
  if (!mPopupContent) {
    return;
  }

  DispatchMouseEvent(mPopupContent, NS_LITERAL_STRING("popuphiding"));
  DispatchMouseEvent(mPopupContent, NS_LITERAL_STRING("popuphidden"));

  Deactivate();
}

void
uGlobalMenu::Refresh()
{
  TRACETM();

  ClearFlags(UNITY_MENUOBJECT_IS_DIRTY);

  SyncLabelFromContent();
  SyncSensitivityFromContent();
  SyncVisibilityFromContent();
  SyncIconFromContent();
}

void
uGlobalMenu::InitializeDbusMenuItem()
{
  if (!mDbusMenuItem) {
    mDbusMenuItem = dbusmenu_menuitem_new();
    if (!mDbusMenuItem) {
      return;
    }
  } else {
    OnlyKeepProperties(static_cast<uMenuObjectProperties>(eLabel | eEnabled |
                                                          eVisible | eIconData |
                                                          eChildDisplay));
  }

  // This happens automatically when we add children, but we have to
  // do this manually for menus which don't initially have children,
  // so we can receive about-to-show which triggers a build of the menu
  dbusmenu_menuitem_property_set(mDbusMenuItem,
                                 DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY,
                                 DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU);

  g_signal_connect(G_OBJECT(mDbusMenuItem), "about-to-show",
                   G_CALLBACK(MenuAboutToOpenCallback), this);
  g_signal_connect(G_OBJECT(mDbusMenuItem), "event",
                   G_CALLBACK(MenuEventCallback), this);

  Refresh();
}

void
uGlobalMenu::GetMenuPopupFromMenu(nsIContent **aResult)
{
  if (!aResult)
    return;

  *aResult = nsnull;

#if MOZILLA_BRANCH_MAJOR_VERSION < 15
  // Taken from widget/src/cocoa/nsMenuX.mm. Not sure if we need this
  nsIXBLService *xblService = uGlobalMenuService::GetXBLService();
  if (!xblService)
    return;

  PRInt32 dummy;
  nsCOMPtr<nsIAtom> tag;
  xblService->ResolveTag(mContent, &dummy, getter_AddRefs(tag));

  if (tag == uWidgetAtoms::menupopup) {
    *aResult = mContent;
    NS_ADDREF(*aResult);
    return;
  }
#else
  // FIXME: What are we meant to do here? Are there any scenario's where this
  //        is actually broken? (I guess a menu could have a binding that
  //        extends a menupopup, but that doesn't seem to happen anywhere
  //        by default)
#endif

  PRUint32 count = mContent->GetChildCount();

  for (PRUint32 i = 0; i < count; i++) {
    PRInt32 dummy;
    nsIContent *child = mContent->GetChildAt(i);
#if MOZILLA_BRANCH_MAJOR_VERSION < 15
    nsCOMPtr<nsIAtom> tag;
    xblService->ResolveTag(child, &dummy, getter_AddRefs(tag));
    if (tag == uWidgetAtoms::menupopup) {
#else
    // XXX: See comment above
    if (child->Tag() == uWidgetAtoms::menupopup) {
#endif
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }
}

static bool
IsRecycledItemCompatible(DbusmenuMenuitem *aRecycled,
                         uGlobalMenuObject *aNewItem)
{
  // If the recycled item was a separator, it can only be reused as a separator
  if ((g_strcmp0(dbusmenu_menuitem_property_get(aRecycled,
                                                DBUSMENU_MENUITEM_PROP_TYPE),
                 "separator") == 0) !=
      (aNewItem->GetType() == eMenuSeparator)) {
    return false;
  }

  // Everything else is fine
  return true;
}

bool
uGlobalMenu::InsertMenuObjectAt(uGlobalMenuObject *menuObj,
                                PRUint32 index)
{
  NS_ASSERTION(index <= mMenuObjects.Length(), "Invalid index");
  if (index > mMenuObjects.Length()) {
    return false;
  }

  PRUint32 correctedIndex = index;

  DbusmenuMenuitem *recycled = nsnull;
  if (mRecycleList && mRecycleList->mList.Length() > 0) {
    if (index < mRecycleList->mMarker) {
      ++mRecycleList->mMarker;
    } else if (index > mRecycleList->mMarker) {
      correctedIndex += mRecycleList->mList.Length();
    } else {
      // If this node is being inserted in to a gap left by previously
      // removed nodes, then recycle one that we just removed
      recycled = mRecycleList->Shift();
      if (!IsRecycledItemCompatible(recycled, menuObj)) {
        recycled = nsnull;
        mRecycleList = nsnull;
      }
    }
  }

  gboolean res = TRUE;
  if (recycled) {
    menuObj->SetDbusMenuItem(recycled);
  } else {
    res = dbusmenu_menuitem_child_add_position(mDbusMenuItem,
                                               menuObj->GetDbusMenuItem(),
                                               correctedIndex);
  }

  return res && mMenuObjects.InsertElementAt(index, menuObj);
}

bool
uGlobalMenu::AppendMenuObject(uGlobalMenuObject *menuObj)
{
  DbusmenuMenuitem *recycled = nsnull;
  if (mRecycleList && mRecycleList->mList.Length() > 0 &&
      mRecycleList->mMarker == mMenuObjects.Length()) {
    // If any nodes were just removed from the end of the menu, then recycle
    // one now
    recycled = mRecycleList->Shift();
    if (!IsRecycledItemCompatible(recycled, menuObj)) {
      recycled = nsnull;
      mRecycleList = nsnull;
    }
  }

  gboolean res = TRUE;
  if (recycled) {
    menuObj->SetDbusMenuItem(recycled);
  } else {
    res = dbusmenu_menuitem_child_append(mDbusMenuItem,
                                         menuObj->GetDbusMenuItem());
  }

  return res && mMenuObjects.AppendElement(menuObj);
}

bool
uGlobalMenu::RemoveMenuObjectAt(PRUint32 index)
{
  NS_ASSERTION(index < mMenuObjects.Length(), "Invalid index");
  if (index >= mMenuObjects.Length()) {
    return false;
  }

  // We add contiguous blocks of removed nodes to a recycle list, so that
  // we can reuse them again if they can be reinserted in to the menu without
  // changing its structure. The list is cleaned in an idle event, so nodes
  // must be removed and inserted without running the event loop if they are
  // to benefit from this recycling feature.
  // This feature allows menu contents to be refreshed by removing all children
  // and inserting new ones, without altering the overall structure. It is used
  // by the history menu in Firefox
  if (!mRecycleList) {
    mRecycleList = new RecycleList(this, index);
  } else if (mRecycleList->mList.Length() > 0 &&
             (index < mRecycleList->mMarker - 1 ||
              index > mRecycleList->mMarker)) {
    // If this node is not adjacent to any previously removed nodes, then
    // free the existing nodes already and restart the process
    mRecycleList->Empty();
    mRecycleList->mMarker = index;
  }

  if (index == mRecycleList->mMarker) {
    mRecycleList->Push(mMenuObjects[index]->GetDbusMenuItem());
  } else {
    mRecycleList->Unshift(mMenuObjects[index]->GetDbusMenuItem());
  }

  mMenuObjects.RemoveElementAt(index);

  return true;
}

nsresult
uGlobalMenu::Build()
{
  TRACETM();

  PRUint32 count = mMenuObjects.Length();
  for (PRUint32 i = 0; i < count; i++) {
    RemoveMenuObjectAt(0);
  }

  // Removing all of the children causes dbusmenu to convert us from a
  // submenu to a normal menuitem. Adding children changes this back again.
  // We can avoid the shell ever seeing this by manually making ourself
  // a submenu again before spinning the event loop
  dbusmenu_menuitem_property_set(mDbusMenuItem,
                                 DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY,
                                 DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU);

  if (mPopupContent && mPopupContent != mContent) {
    mListener->UnregisterForContentChanges(mPopupContent, this);
  }

  GetMenuPopupFromMenu(getter_AddRefs(mPopupContent));

  if (!mPopupContent) {
    // The menu has no popup, so there are no menuitems here
    return NS_OK;
  }

  // Wrap the native menupopup node, as this results in style resolution
  // and attachment of XBL bindings, which normally doesn't happen because
  // we are a child of an element with "display: none"
  // Borrowed from widget/src/cocoa/nsMenuX.mm, we need this to make
  // some menus in Thunderbird work
  nsIDocument *doc = mPopupContent->GetCurrentDoc();
  if (doc) {
    nsIXPConnect *xpconnect = uGlobalMenuService::GetXPConnect();
    if (xpconnect) {
      nsIScriptGlobalObject *sgo = doc->GetScriptGlobalObject();
      nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
      JSObject *global = sgo->GetGlobalJSObject();
      if (scriptContext && global) {
        JSContext *cx = (JSContext *)scriptContext->GetNativeContext();
        if (cx) {
          nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
          xpconnect->WrapNative(cx, global,
                                mPopupContent, NS_GET_IID(nsISupports),
                                getter_AddRefs(wrapper));
        }
      }
    }
  }

  if (mContent != mPopupContent) {
    nsresult rv = mListener->RegisterForContentChanges(mPopupContent, this);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to register for popup content changes");
      return rv;
    }
  }

  ClearNeedsRebuild();

  count = mPopupContent->GetChildCount();

  for (PRUint32 i = 0; i < count; i++) {
    nsIContent *child = mPopupContent->GetChildAt(i);
    uGlobalMenuObject *menuObject =
      NewGlobalMenuItem(static_cast<uGlobalMenuObject *>(this),
                        mListener, child, mMenuBar);
    bool res = false;
    if (menuObject) {
      res = AppendMenuObject(menuObject);
    }
    NS_WARN_IF_FALSE(res, "Failed to append menuitem. Marking menu invalid");
    if (!res) {
      SetNeedsRebuild();
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult
uGlobalMenu::Init(uGlobalMenuObject *aParent,
                  uGlobalMenuDocListener *aListener,
                  nsIContent *aContent,
                  uGlobalMenuBar *aMenuBar)
{
  NS_ENSURE_ARG(aParent);
  NS_ENSURE_ARG(aListener);
  NS_ENSURE_ARG(aContent);
  NS_ENSURE_ARG(aMenuBar);

  mParent = aParent;
  mListener = aListener;
  mContent = aContent;
  mMenuBar = aMenuBar;

  SetNeedsRebuild();

  // See the hack comment above for why this workaround is here
  if (mParent->GetType() != eMenuBar || mMenuBar->IsRegistered()) {
    SetFlags(UNITY_MENU_READY);
  }

  nsresult rv = mListener->RegisterForContentChanges(mContent, this);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to register for content changes");
    return rv;
  }

  return NS_OK;
}

uGlobalMenu::uGlobalMenu(): uGlobalMenuObject(eMenu)
{
  MOZ_COUNT_CTOR(uGlobalMenu);
}

uGlobalMenu::~uGlobalMenu()
{
  if (mListener) {
    mListener->UnregisterForContentChanges(mContent, this);
    if (mPopupContent && mContent != mPopupContent) {
      mListener->UnregisterForContentChanges(mPopupContent, this);
    }
  }

  DestroyIconLoader();

  if (mDbusMenuItem) {
    g_signal_handlers_disconnect_by_func(mDbusMenuItem,
                                         FuncToVoidPtr(MenuAboutToOpenCallback),
                                         this);
    g_signal_handlers_disconnect_by_func(mDbusMenuItem,
                                         FuncToVoidPtr(MenuEventCallback),
                                         this);
    g_object_unref(mDbusMenuItem);
  }

  if (mRecycleList) {
    // The recycle list doesn't hold a strong ref to our dbusmenuitem or
    // any of it's children. As we've just dropped our ref, if there are
    // any items on the recycle list, forget them now. If we don't forget
    // them, we will crash when calling the recycle list destructor
    mRecycleList->mList.Clear();
  }

  MOZ_COUNT_DTOR(uGlobalMenu);
}

/*static*/ uGlobalMenuObject*
uGlobalMenu::Create(uGlobalMenuObject *aParent,
                    uGlobalMenuDocListener *aListener,
                    nsIContent *aContent,
                    uGlobalMenuBar *aMenuBar)
{
  TRACEC(aContent);

  uGlobalMenu *menu = new uGlobalMenu();
  if (!menu) {
    return nsnull;
  }

  if (NS_FAILED(menu->Init(aParent, aListener, aContent, aMenuBar))) {
    delete menu;
    return nsnull;
  }

  return static_cast<uGlobalMenuObject *>(menu);
}

void
uGlobalMenu::Invalidate()
{
  uGlobalMenuObject::Invalidate();

  // If we are visible on screen, now we go and invalidate children. If not,
  // then we invalidate them when we appear on screen in ContainerIsOpening().
  // If we need a rebuild, we just skip this (unless we are a direct descendant
  // of the menubar, when we rebuild to avoid the issue mentioned in the
  // comment below....)
  if (IsContainerOnScreen()) {
    if (mParent->GetType() == eMenuBar && DoesNeedRebuild()) {
      Build();
    } else if (!DoesNeedRebuild()) {
      for (PRUint32 i = 0; i < mMenuObjects.Length(); i++) {
        if (mParent->GetType() == eMenuBar) {
          // This is a bit of a hack. When a menu is opened with the keyboard,
          // some of our children unhide themselves. If we just invalidate here,
          // then these items won't really appear until this menu gets the
          // about-to-show signal, when we tell our children to refresh themselves,
          // which is after we ask the menu to open. This causes a strange issue
          // in the Firefox History menu, where these additional items appear at
          // the top of the menu after it has appeared on screen, causing
          // keyboard focus to be on the wrong menu item
          mMenuObjects[i]->Invalidate();
          mMenuObjects[i]->ContainerIsOpening();
          mMenuObjects[i]->ContainerIsClosing();
        } else {
          mMenuObjects[i]->Invalidate();
        }
      }
    }
  }
} 

void
uGlobalMenu::ContainerIsOpening()
{
  if (IsDirty() && !DoesNeedRebuild()) {
    for (PRUint32 i = 0; i < mMenuObjects.Length(); i++) {
      mMenuObjects[i]->Invalidate();
    }
  }

  uGlobalMenuObject::ContainerIsOpening();
}

/*static*/ gboolean
uGlobalMenu::DoOpen(gpointer user_data)
{
  DbusmenuMenuitem *menuitem = static_cast<DbusmenuMenuitem *>(user_data);
  dbusmenu_menuitem_show_to_user(menuitem, 0);
  g_object_unref(menuitem);
  return FALSE;
}

void
uGlobalMenu::OpenMenuDelayed()
{
  if (!CanOpen()) {
    return;
  }

  // Here, we open the menu after a short delay. This avoids an issue
  // where opening the History menu in Firefox with the keyboard causes
  // extra items to appear at the top of the menu, but keyboard focus is
  // not on the first item
  g_timeout_add(100, DoOpen, g_object_ref(mDbusMenuItem));
}

void
uGlobalMenu::ObserveAttributeChanged(nsIDocument *aDocument,
                                     nsIContent *aContent,
                                     nsIAtom *aAttribute)
{
  TRACETM();
  NS_ASSERTION(aContent == mContent || aContent == mPopupContent,
               "Received an event that wasn't meant for us!");

  if (aAttribute == uWidgetAtoms::open) {
    return;
  }

  if (IsDirty()) {
    LOGTM("Previously marked as invalid");
    return;
  }

  if (!IsContainerOnScreen()) {
    LOGTM("Parent isn't open or opening. Marking invalid");
    Invalidate();
    return;
  }

  if (aContent == mContent) {
    if (aAttribute == uWidgetAtoms::disabled) {
      SyncSensitivityFromContent();
    } else if (aAttribute == uWidgetAtoms::label || 
               aAttribute == uWidgetAtoms::accesskey) {
      SyncLabelFromContent();
    } else if (aAttribute == uWidgetAtoms::image) {
      SyncIconFromContent();
    }

    SyncVisibilityFromContent();
    if (aAttribute != uWidgetAtoms::image) {
      SyncIconFromContent();
    }
  }

  // Attribute changes can change the style of children, so
  // we invalidate them
  for (PRUint32 i = 0; i < mMenuObjects.Length(); i++) {
    mMenuObjects[i]->Invalidate();
  }
}

void
uGlobalMenu::ObserveContentRemoved(nsIDocument *aDocument,
                                   nsIContent *aContainer,
                                   nsIContent *aChild,
                                   PRInt32 aIndexInContainer)
{
  TRACETM();
  NS_ASSERTION(aContainer == mContent || aContainer == mPopupContent,
               "Received an event that wasn't meant for us!");

  if (DoesNeedRebuild()) {
    LOGTM("Previously marked as needing a rebuild");
    return;
  }

  if (!IsOpenOrOpening()) {
    LOGTM("Not open or opening - Marking as needing a rebuild");
    SetNeedsRebuild();
    return;
  }

  if (aContainer == mPopupContent) {
    bool res = RemoveMenuObjectAt(aIndexInContainer);
    NS_WARN_IF_FALSE(res, "Failed to remove menuitem - marking menu as needing a rebuild");
    if (!res) {
      SetNeedsRebuild();
    }
  } else {
    Build();
  }
}

void
uGlobalMenu::ObserveContentInserted(nsIDocument *aDocument,
                                    nsIContent *aContainer,
                                    nsIContent *aChild,
                                    PRInt32 aIndexInContainer)
{
  TRACETM();
  NS_ASSERTION(aContainer == mContent || aContainer == mPopupContent,
               "Received an event that wasn't meant for us!");

  if (DoesNeedRebuild()) {
    LOGTM("Previously marked as needing a rebuild");
    return;
  }

  if (!IsOpenOrOpening()) {
    LOGTM("Not open or opening - Marking as needing a rebuild");
    SetNeedsRebuild();
    return;
  }

  if (aContainer == mPopupContent) {
    uGlobalMenuObject *newItem =
      NewGlobalMenuItem(static_cast<uGlobalMenuObject *>(this),
                        mListener, aChild, mMenuBar);
    bool res = false;
    if (newItem) {
      res = InsertMenuObjectAt(newItem, aIndexInContainer);
    }
    NS_WARN_IF_FALSE(res, "Failed to insert menuitem - marking menu as needing a rebuild");
    if (!res) {
      SetNeedsRebuild();
    } 
  } else {
    Build();
  }
}

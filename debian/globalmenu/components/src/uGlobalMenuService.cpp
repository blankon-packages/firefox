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

#include <nsCOMPtr.h>
#include <nsIContent.h>
#include <nsIObserverService.h>
#include <nsStringAPI.h>
#include <nsDebug.h>
#include <nsComponentManagerUtils.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsServiceManagerUtils.h>
#include <nsIWidget.h>
#include <nsIWindowMediator.h>
#include <nsIBaseWindow.h>
#include <nsIXULWindow.h>
#include <prenv.h>

#include <glib-object.h>

#include "uGlobalMenuService.h"
#include "uWidgetAtoms.h"

#include "uDebug.h"

class RegisterWindowCbData
{
public:
  RegisterWindowCbData(uGlobalMenuBar *aMenu,
                       uGlobalMenuRequestAutoCanceller *aCanceller):
                       mMenu(aMenu),
                       mCanceller(aCanceller)
  {
    mCancellable = mCanceller->GetCancellable();
    if (mCancellable) {
      g_object_ref(mCancellable);
      mID = g_cancellable_connect(mCancellable, G_CALLBACK(Cancelled), this, nsnull);
    }
  }

  static void Cancelled(GCancellable *aCancellable,
                        gpointer userdata)
  {
    RegisterWindowCbData *cbdata =
      static_cast<RegisterWindowCbData *>(userdata);

    // If the request was cancelled, then invalidate pointers to objects
    // that might not exist anymore

    cbdata->mMenu = nsnull;
    cbdata->mCanceller = nsnull;
  }

  uGlobalMenuBar* GetMenuBar() { return mMenu; }

  ~RegisterWindowCbData()
  {
    if (mCancellable) {
      g_cancellable_disconnect(mCancellable, mID);
      g_object_unref(mCancellable);
    }
  }

private:
  uGlobalMenuBar *mMenu;
  GCancellable *mCancellable;
  uGlobalMenuRequestAutoCanceller *mCanceller;
  PRUint32 mID;
};

NS_IMPL_ISUPPORTS2(uGlobalMenuService, uIGlobalMenuService, nsIWindowMediatorListener)

/*static*/ void
uGlobalMenuService::ProxyCreatedCallback(GObject *object,
                                         GAsyncResult *res,
                                         gpointer userdata)
{
  uGlobalMenuService *self = static_cast<uGlobalMenuService *>(userdata);

  GError *error = NULL;
  GDBusProxy *proxy = g_dbus_proxy_new_for_bus_finish(res, &error);

  if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    // We should only get cancelled if the service was destroyed.
    // In this case, it is no longer safe to proceed
    return;
  }

  self->mCancellable->Destroy();
  self->mCancellable = nsnull;

  self->mDbusProxy = proxy;

  if (!proxy) {
    NS_WARNING("Failed to create proxy for AppMenu registrar");
    self->SetOnline(PR_FALSE);
    return;
  }

  self->mNOCHandlerID = g_signal_connect(self->mDbusProxy, "notify::g-name-owner",
                                         G_CALLBACK(NameOwnerChangedCallback),
                                         userdata);

  char *owner = g_dbus_proxy_get_name_owner(self->mDbusProxy);
  self->SetOnline(owner ? PR_TRUE : PR_FALSE);

  if (error) {
    g_error_free(error);
  }
  g_free(owner);
}

/*static*/ void
uGlobalMenuService::NameOwnerChangedCallback(GObject *object,
                                             GParamSpec *pspec,
                                             gpointer userdata)
{
  uGlobalMenuService *self = static_cast<uGlobalMenuService *>(userdata);

  char *owner = g_dbus_proxy_get_name_owner(self->mDbusProxy);
  self->SetOnline(owner ? PR_TRUE : PR_FALSE);
  g_free(owner);
}

/*static*/ void
uGlobalMenuService::RegisterWindowCallback(GObject *object,
                                           GAsyncResult *res,
                                           gpointer userdata)
{
  RegisterWindowCbData *data =
    static_cast<RegisterWindowCbData *>(userdata);

  uGlobalMenuBar *menu = data->GetMenuBar();
  GDBusProxy *proxy = G_DBUS_PROXY(object);

  GError *error = NULL;
  // RegisterWindowCbData owns a reference to the proxy
  GVariant *result = g_dbus_proxy_call_finish(proxy, res, &error);
  if (result) {
    g_variant_unref(result);
  }

  if (error && g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
    // The call is cancelled if uGlobalMenuService goes away or the
    // panel service goes offline. In either case, the menu has now
    // been destroyed
    delete data;
    return;
  }

  if (menu && !PR_GetEnv("GLOBAL_MENU_DEBUG")) {
    // This check is probably bogus. menu should only be invalid if
    // the request was cancelled, in which case, we should have
    // returned already
    menu->SetMenuBarRegistered(error ? PR_FALSE : PR_TRUE);
  }

  if (error) {
    g_error_free(error);
  }

  delete data;
}

void
uGlobalMenuService::DestroyMenus()
{
  PRUint32 count = mMenus.Length();
  for (PRUint32 j = 0; j < count; j++) {
    mMenus.RemoveElementAt(0);
  }
}

void
uGlobalMenuService::DestroyMenuForWidget(nsIWidget *aWidget)
{
  for (PRUint32 i = 0; i < mMenus.Length(); i++) {
    if (mMenus[i]->WidgetHasSameToplevelWindow(aWidget)) {
      mMenus.RemoveElementAt(i);
      return;
    }
  }
}

void
uGlobalMenuService::SetOnline(PRBool aOnline)
{
  if (mOnline != !!aOnline) {
    mOnline = !!aOnline;
    nsCOMPtr<nsIObserverService> os =
      do_GetService("@mozilla.org/observer-service;1");
    if (os) {
      os->NotifyObservers(nsnull, mOnline ? "native-menu-service:online" : "native-menu-service:offline", 0);
    }

    if (!mOnline) {
      DestroyMenus();
    }
  }
}

PRBool
uGlobalMenuService::WidgetHasGlobalMenu(nsIWidget *aWidget)
{
  for (PRUint32 i = 0; i < mMenus.Length(); i++) {
    if (mMenus[i]->WidgetHasSameToplevelWindow(aWidget))
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
uGlobalMenuService::Init()
{
  nsresult rv;
  rv = uWidgetAtoms::RegisterAtoms();
  NS_ENSURE_SUCCESS(rv, rv);

  mCancellable = uGlobalMenuRequestAutoCanceller::Create();

  GDBusProxyFlags flags = static_cast<GDBusProxyFlags>(
                          G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                          G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS | 
                          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START);

  g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                           flags,
                           NULL,
                           "com.canonical.AppMenu.Registrar",
                           "/com/canonical/AppMenu/Registrar",
                           "com.canonical.AppMenu.Registrar",
                           mCancellable->GetCancellable(),
                           ProxyCreatedCallback,
                           this);

  nsCOMPtr<nsIWindowMediator> wm =
      do_GetService("@mozilla.org/appshell/window-mediator;1");
  NS_ENSURE_TRUE(wm, NS_ERROR_OUT_OF_MEMORY);

  wm->AddListener(this);
  return rv;
}

uGlobalMenuService::~uGlobalMenuService()
{
  nsCOMPtr<nsIWindowMediator> wm =
      do_GetService("@mozilla.org/appshell/window-mediator;1");
 
  if (wm) {
    wm->RemoveListener(this);
  }

  if (mDbusProxy) {
    g_signal_handler_disconnect(mDbusProxy, mNOCHandlerID);
    g_object_unref(mDbusProxy);
  }
}

NS_IMETHODIMP
uGlobalMenuService::CreateGlobalMenuBar(nsIWidget  *aParent, 
                                        nsIContent *aMenuBarNode)
{
  NS_ENSURE_ARG(aParent);
  NS_ENSURE_ARG(aMenuBarNode);

  NS_ENSURE_TRUE(mOnline, NS_ERROR_FAILURE);

  // Sanity check to make sure we don't register more than one menu
  // for each top-level window
  if (WidgetHasGlobalMenu(aParent))
    return NS_ERROR_FAILURE;

  uGlobalMenuBar *menu = uGlobalMenuBar::Create(aParent, aMenuBarNode);
  NS_ENSURE_TRUE(menu, NS_ERROR_FAILURE);

  mMenus.AppendElement(menu);

  return NS_OK;
}

/* [noscript, notxpcom] void registerGlobalMenuBar (in uGlobalMenuBarPtr menuBar); */
NS_IMETHODIMP_(void)
uGlobalMenuService::RegisterGlobalMenuBar(uGlobalMenuBar *aMenuBar,
                                          uGlobalMenuRequestAutoCanceller *aCanceller)
{
  if (mOnline != PR_TRUE)
    return;

  if (!aMenuBar)
    return;

  if (!aCanceller) {
    return;
  }

  PRUint32 xid = aMenuBar->GetWindowID();
  nsCAutoString path(aMenuBar->GetMenuPath());
  if (xid == 0 || path.IsEmpty())
    return;

  RegisterWindowCbData *data =
    new RegisterWindowCbData(aMenuBar,
                             aCanceller);
  if (!data) {
    return;
  }

  g_dbus_proxy_call(mDbusProxy,
                    "RegisterWindow",
                    g_variant_new("(uo)", xid, path.get()),
                    G_DBUS_CALL_FLAGS_NONE,
                    -1,
                    aCanceller->GetCancellable(),
                    RegisterWindowCallback,
                    data);
}

/* void registerNotification (in nsIObserver observer); */
NS_IMETHODIMP
uGlobalMenuService::RegisterNotification(nsIObserver *aObserver)
{
  NS_ENSURE_ARG(aObserver);

  nsCOMPtr<nsIObserverService> os = 
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE(os, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  rv = os->AddObserver(aObserver, "native-menu-service:online", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->AddObserver(aObserver, "native-menu-service:offline", PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->AddObserver(aObserver, "native-menu-service:popup-open", PR_FALSE);
  return rv;
}

/* void unregisterNotification (in nsIObserver observer); */
NS_IMETHODIMP
uGlobalMenuService::UnregisterNotification(nsIObserver *aObserver)
{
  NS_ENSURE_ARG(aObserver);

  nsCOMPtr<nsIObserverService> os = 
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE(os, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  rv = os->RemoveObserver(aObserver, "native-menu-service:online");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->RemoveObserver(aObserver, "native-menu-service:offline");
  NS_ENSURE_SUCCESS(rv, rv);

  rv = os->RemoveObserver(aObserver, "native-menu-service:popup-open");
  return rv;
}

/* readonly attribute boolean online; */
NS_IMETHODIMP
uGlobalMenuService::GetOnline(PRBool *online)
{
  NS_ENSURE_ARG_POINTER(online);
  *online = !!mOnline;
  return NS_OK;
}

NS_IMETHODIMP
uGlobalMenuService::OnWindowTitleChange(nsIXULWindow *window,
                                        const PRUnichar *newTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuService::OnOpenWindow(nsIXULWindow *window)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuService::OnCloseWindow(nsIXULWindow *window)
{
  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(window);
  NS_ENSURE_TRUE(baseWindow, NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIWidget> widget;
  baseWindow->GetMainWidget(getter_AddRefs(widget));
  NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

  DestroyMenuForWidget(widget);

  return NS_OK;
}

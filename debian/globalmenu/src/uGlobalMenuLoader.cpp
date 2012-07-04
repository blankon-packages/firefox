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

#include <nsServiceManagerUtils.h>
#include <nsIWindowMediator.h>
#include <nsIXULWindow.h>
#include <nsIDocShell.h>
#include <nsIWidget.h>
#include <nsIBaseWindow.h>
#include <nsIContentViewer.h>
#include <nsIDOMDocument.h>
#include <nsIDOMNode.h>
#include <nsIDOMNodeList.h>
#include <nsIContent.h>
#include <nsIWebProgress.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIDOMWindow.h>
#include <nsPIDOMWindow.h>
#if MOZILLA_BRANCH_MAJOR_VERSION == 13
# include <nsIDOMNSElement.h>
#endif
#include <nsIDOMElement.h>
#include <nsIDOMDOMTokenList.h>

#include "uIGlobalMenuService.h"
#include "uGlobalMenuLoader.h"

#include "uDebug.h"

#include "compat.h"

#define XUL_NS "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"

// XXX: The sole purpose of this class is to listen for new nsIXULWindows
//      and do the task that xpfe/appshell/src/nsWebShellWindow.cpp
//      would be doing if this extension were part of Mozilla core. The reason
//      it is an entirely separate entity is so that the menubar service
//      implementation is as close as possible to how it might look inside Mozilla

bool
uGlobalMenuLoader::RegisterMenuFromDS(nsIDocShell *aDocShell)
{
  nsCOMPtr<nsIContentViewer> cv;
  aDocShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv) {
    LOG("No content viewer available for %p yet", (void *)aDocShell);
    return false;
  }

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(cv->GetDocument());
  if (!domDoc) {
    NS_ERROR("Content viewer does not have a document");
    return false;
  }

  nsCOMPtr<nsIDOMNodeList> elements;
  nsresult rv = domDoc->GetElementsByTagNameNS(NS_LITERAL_STRING(XUL_NS),
                                               NS_LITERAL_STRING("menubar"),
                                               getter_AddRefs(elements));
  PRUint32 length;
  if (NS_FAILED(rv) || !elements ||
      NS_FAILED(elements->GetLength(&length)) || length == 0) {
    LOG("%p has no menubar", (void *)aDocShell);
    return false;
  }

  nsCOMPtr<nsIDOMNode> menubar;
  elements->Item(0, getter_AddRefs(menubar));
  if (!menubar) {
    NS_ERROR("Null item in nsIDOMNodeList. Is this meant to happen?");
    return false;
  }

  nsCOMPtr<nsIDOMNSElement> menubarElem = do_QueryInterface(menubar);
  if (!menubarElem) {
    NS_ERROR("Node is not an element, so it can't be a menubar");
    return false;
  }

  nsCOMPtr<nsIDOMDOMTokenList> classes;
  menubarElem->GetClassList(getter_AddRefs(classes));
  if (classes) {
    bool ignore = false;
    classes->Contains(NS_LITERAL_STRING("menubar-keep-in-window"), &ignore);
    if (ignore) {
      LOG("Keeping menubar for %p in window", (void *)aDocShell);
      return false;
    }
  }

  nsCOMPtr<nsIBaseWindow> baseWindow = do_GetInterface(aDocShell);
  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));

  nsCOMPtr<nsIContent> menubarContent = do_QueryInterface(menubar);

  // XXX: Should we do anything with errors here?
  rv = mService->CreateGlobalMenuBar(mainWidget, menubarContent);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create menubar");
    return false;
  }

  return true;
}

void
uGlobalMenuLoader::RegisterAllMenus()
{
  nsCOMPtr<nsIWindowMediator> wm =
      do_GetService("@mozilla.org/appshell/window-mediator;1");
  if (!wm) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> iter;
  wm->GetXULWindowEnumerator(nsnull, getter_AddRefs(iter));
  if (!iter) {
    return;
  }

  bool hasMore;
  iter->HasMoreElements(&hasMore);

  while (hasMore) {
    nsCOMPtr<nsISupports> elem;
    iter->GetNext(getter_AddRefs(elem));
    iter->HasMoreElements(&hasMore);
    if (!elem)
      continue;

    nsCOMPtr<nsIXULWindow> xulWindow = do_QueryInterface(elem);
    if (!xulWindow)
      continue;

    OnOpenWindow(xulWindow);
  }
}

NS_IMPL_ISUPPORTS3(uGlobalMenuLoader, uIGlobalMenuServiceObserver, nsIWebProgressListener, nsISupportsWeakReference)

nsresult
uGlobalMenuLoader::Init()
{
  mService = do_GetService("@canonical.com/globalmenu-service;1");
  if (!mService) {
    NS_WARNING("Failed to get menu service");
    return NS_ERROR_FAILURE;
  }

  mService->RegisterNotification(this);

  nsCOMPtr<nsIWindowMediator> wm =
      do_GetService("@mozilla.org/appshell/window-mediator;1");
  if (!wm) {
    NS_WARNING("Failed to get window mediator service");
    return NS_ERROR_FAILURE;
  }

  wm->AddListener(this);

  bool online;
  mService->GetOnline(&online);
  if (online) {
    RegisterAllMenus();
  }

  return NS_OK;
}

uGlobalMenuLoader::~uGlobalMenuLoader()
{
  mService->UnregisterNotification(this);

  nsCOMPtr<nsIWindowMediator> wm =
      do_GetService("@mozilla.org/appshell/window-mediator;1");
 
  if (wm) {
    wm->RemoveListener(this);
  }
}

NS_IMETHODIMP
uGlobalMenuLoader::OnMenuServiceOnlineChange(bool aOnline)
{
  if (aOnline) {
    RegisterAllMenus();
  }

  return NS_OK;
}

/* void onWindowTitleChange (in nsIXULWindow window, in wstring newTitle); */
NS_IMETHODIMP
uGlobalMenuLoader::OnWindowTitleChange(nsIXULWindow *window,
                                       const PRUnichar *newTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void onOpenWindow (in nsIXULWindow window); */
NS_IMETHODIMP
uGlobalMenuLoader::OnOpenWindow(nsIXULWindow *window)
{
  nsCOMPtr<nsIDocShell> docShell;
  window->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) {
    NS_WARNING("Incoming window doesn't have a docshell");
    return NS_ERROR_FAILURE;
  }

  bool res = RegisterMenuFromDS(docShell);

  if (!res) {
    // If we've been called off a window open event from the window mediator,
    // then the document probably hasn't loaded yet. To fix this, we set up a progress
    // listener on the docshell, so we can do the actual menu load once the
    // document has finished loading
    nsCOMPtr<nsIWebProgress> progress = do_GetInterface(docShell);
    if (progress) {
       progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_NETWORK);
    }
  }

  return NS_OK;
}

/* void onCloseWindow (in nsIXULWindow window); */
NS_IMETHODIMP
uGlobalMenuLoader::OnCloseWindow(nsIXULWindow *window)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuLoader::OnStateChange(nsIWebProgress *aWebProgress,
                                 nsIRequest *aRequest,
                                 PRUint32 aStateFlags,
                                 nsresult aStatus)
{
  // Borrowed from nsWebShellWindow.cpp

  // If the notification is not about a document finishing, then just
  // ignore it...
  if (!(aStateFlags & nsIWebProgressListener::STATE_STOP) || 
      !(aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)) {
    return NS_OK;
  }

  // If this document notification is for a frame then ignore it...
  nsCOMPtr<nsIDOMWindow> eventWin;
  aWebProgress->GetDOMWindow(getter_AddRefs(eventWin));
  nsCOMPtr<nsPIDOMWindow> eventPWin(do_QueryInterface(eventWin));
  if (eventPWin) {
    nsPIDOMWindow *rootPWin = eventPWin->GetPrivateRoot();
    if (eventPWin != rootPWin)
      return NS_OK;
  }

  aWebProgress->RemoveProgressListener(this);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(aWebProgress);
  if (!docShell) {
    NS_ERROR("No docshell?");
    return NS_ERROR_FAILURE;
  }

  RegisterMenuFromDS(docShell);

  return NS_OK;
}

NS_IMETHODIMP
uGlobalMenuLoader::OnProgressChange(nsIWebProgress *aWebProgress,
                                    nsIRequest *aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress,
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuLoader::OnLocationChange(nsIWebProgress *aWebProgress,
                                    nsIRequest *aRequest,
                                    nsIURI *aLocation,
                                    PRUint32 aFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuLoader::OnStatusChange(nsIWebProgress *aWebProgress,
                                  nsIRequest *aRequest,
                                  nsresult aStatus,
                                  const PRUnichar *aMessage)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuLoader::OnSecurityChange(nsIWebProgress *aWebProgress,
                                    nsIRequest *aRequest,
                                    PRUint32 aState)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

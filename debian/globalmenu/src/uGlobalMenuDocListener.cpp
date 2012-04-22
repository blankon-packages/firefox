/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Thomas K. Dyas <tom.dyas@gmail.com>
 *   Chris Coulson <chris.coulson@canonical.com>
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

#include <prtypes.h>
#include <nsIDocument.h>
#include <nsIAtom.h>
#include <nsINode.h>
#include <mozilla/dom/Element.h>
#include <nsIContent.h>
#include <nsIDocument.h>

#include "uGlobalMenuDocListener.h"

#include "uDebug.h"

NS_IMPL_ISUPPORTS1(uGlobalMenuDocListener, nsIMutationObserver)

nsresult
uGlobalMenuDocListener::Init(nsIContent *rootNode)
{
  NS_ENSURE_ARG(rootNode);

  mDocument = rootNode->OwnerDoc();
  if (!mDocument)
    return NS_ERROR_FAILURE;
  mDocument->AddMutationObserver(this);

  return NS_OK;
}

void
uGlobalMenuDocListener::Destroy()
{
  if (mDocument) {
    mDocument->RemoveMutationObserver(this);
  }
}

uGlobalMenuDocListener::uGlobalMenuDocListener() :
  mDocument(nsnull)
{
  mContentToObserverTable.Init();
}

uGlobalMenuDocListener::~uGlobalMenuDocListener()
{
  NS_ASSERTION(mContentToObserverTable.Count() == 0 &&
               mFallbackObservers.Length() == 0,
               "Some nodes forgot to unregister listeners");
}

void
uGlobalMenuDocListener::CharacterDataWillChange(nsIDocument *aDocument,
                                                nsIContent *aContent,
              	                                CharacterDataChangeInfo *aInfo)
{

}

void
uGlobalMenuDocListener::CharacterDataChanged(nsIDocument *aDocument,
                                             nsIContent *aContent,
                                             CharacterDataChangeInfo *aInfo)
{

}

void
uGlobalMenuDocListener::AttributeWillChange(nsIDocument *aDocument,
                                            mozilla::dom::Element *aElement,
                                            PRInt32 aNameSpaceID,
                                            nsIAtom *aAttribute,
                                            PRInt32 aModType)
{

}

void
uGlobalMenuDocListener::AttributeChanged(nsIDocument *aDocument,
                                         mozilla::dom::Element *aElement,
                                         PRInt32 aNameSpaceID,
                                         nsIAtom *aAttribute,
                                         PRInt32 aModType)
{
  if (!aElement)
    return;

  nsTArray<uMenuChangeObserver *> *listeners =
    GetListenersForContent(aElement, false);

  if (listeners) {
    for (PRUint32 i = 0; i < listeners->Length(); i++) {
      listeners->ElementAt(i)->ObserveAttributeChanged(aDocument, aElement,
                                                       aAttribute);
    }
  }

  if (!listeners || listeners->Length() == 0) {
    for (PRUint32 i = 0; i < mFallbackObservers.Length(); i++) {
      mFallbackObservers[i]->ObserveAttributeChanged(aDocument, aElement,
                                                     aAttribute);
    }
  }
}

void
uGlobalMenuDocListener::ContentAppended(nsIDocument *aDocument,
                                        nsIContent *aContainer,
                                        nsIContent *aFirstNewContent,
                                        PRInt32 aNewIndexInContainer)
{
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    ContentInserted(aDocument, aContainer, cur, aNewIndexInContainer);
    aNewIndexInContainer++;
  }
}

void
uGlobalMenuDocListener::ContentInserted(nsIDocument *aDocument,
                                        nsIContent *aContainer,
                                        nsIContent *aChild,
                                        PRInt32 aIndexInContainer)
{
  if (!aContainer)
    return;

  nsTArray<uMenuChangeObserver *> *listeners =
    GetListenersForContent(aContainer, false);

  if (listeners) {
    for (PRUint32 i = 0; i < listeners->Length(); i++) {
      listeners->ElementAt(i)->ObserveContentInserted(aDocument, aContainer,
                                                      aChild, aIndexInContainer);
    }
  }

  if (!listeners || listeners->Length() == 0) {
    for (PRUint32 i = 0; i < mFallbackObservers.Length(); i++) {
      mFallbackObservers[i]->ObserveContentInserted(aDocument, aContainer,
                                                    aChild, aIndexInContainer);
    }
  }
}

void
uGlobalMenuDocListener::ContentRemoved(nsIDocument *aDocument,
                                       nsIContent *aContainer,
                                       nsIContent *aChild,
                                       PRInt32 aIndexInContainer,
                                       nsIContent *aPreviousSibling)
{
  if (!aContainer)
    return;

  nsTArray<uMenuChangeObserver *> *listeners =
    GetListenersForContent(aContainer, false);

  if (listeners) {
    for (PRUint32 i = 0; i < listeners->Length(); i++) {
      listeners->ElementAt(i)->ObserveContentRemoved(aDocument, aContainer,
                                                     aChild, aIndexInContainer);
    }
  }

  if (!listeners || listeners->Length() == 0) {
    for (PRUint32 i = 0; i < mFallbackObservers.Length(); i++) {
      mFallbackObservers[i]->ObserveContentRemoved(aDocument, aContainer, aChild,
                                                   aIndexInContainer);
    }
  }
}

void
uGlobalMenuDocListener::NodeWillBeDestroyed(const nsINode *aNode)
{
  mDocument = nsnull;
}

void
uGlobalMenuDocListener::ParentChainChanged(nsIContent *aContent)
{

}

nsresult
uGlobalMenuDocListener::RegisterForContentChanges(nsIContent *aContent,
                                                  uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aContent);
  NS_ENSURE_ARG(aMenuObject);

  nsTArray<uMenuChangeObserver *> *listeners =
    GetListenersForContent(aContent, true);

  listeners->AppendElement(aMenuObject);
  return NS_OK;
}

nsresult
uGlobalMenuDocListener::UnregisterForContentChanges(nsIContent *aContent,
                                                    uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aContent);
  NS_ENSURE_ARG(aMenuObject);

  nsTArray<uMenuChangeObserver *> *listeners =
    GetListenersForContent(aContent, false);
  if (!listeners) {
    return NS_ERROR_FAILURE;
  }

  if (listeners->RemoveElement(aMenuObject)) {
    if (listeners->Length() == 0) {
      mContentToObserverTable.Remove(aContent);
    }

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
uGlobalMenuDocListener::RegisterFallbackListener(uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aMenuObject);
  mFallbackObservers.AppendElement(aMenuObject);
  return NS_OK;
}

nsresult
uGlobalMenuDocListener::UnregisterFallbackListener(uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aMenuObject);

  if (mFallbackObservers.RemoveElement(aMenuObject)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsTArray<uMenuChangeObserver *>*
uGlobalMenuDocListener::GetListenersForContent(nsIContent *aContent,
                                               bool aCreate)
{
  nsTArray<uMenuChangeObserver *> *listeners;
  if (!mContentToObserverTable.Get(aContent, &listeners) && aCreate) {
    listeners = new nsTArray<uMenuChangeObserver *>();
    mContentToObserverTable.Put(aContent, listeners);
  }

  return listeners;
}
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

  mDocument = rootNode->GetOwnerDoc();
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
#ifdef DEBUG_chrisccoulson
  mCount(0),
#endif
  mDocument(nsnull)
{
  mContentToObserverTable.Init();
}

uGlobalMenuDocListener::~uGlobalMenuDocListener()
{
  NS_ASSERTION(mCount == 0, "Some nodes forgot to unregister listeners");
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

  nsTArray<uMenuChangeObserver *> listeners;
  GetListeners(aElement, listeners);

  for (PRUint32 i = 0; i < listeners.Length(); i++) {
    listeners[i]->ObserveAttributeChanged(aDocument, aElement, aAttribute);
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

  nsTArray<uMenuChangeObserver *> listeners;
  GetListeners(aContainer, listeners);

  for (PRUint32 i = 0; i < listeners.Length(); i++) {
    listeners[i]->ObserveContentInserted(aDocument, aContainer, aChild,
                                         aIndexInContainer);
  }

  if (listeners.Length() == 0) {
    for (PRUint32 i = 0; i < mGlobalObservers.Length(); i++) {
      mGlobalObservers[i]->ObserveContentInserted(aDocument, aContainer,
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

  nsTArray<uMenuChangeObserver *> listeners;
  GetListeners(aContainer, listeners);

  for (PRUint32 i = 0; i < listeners.Length(); i++) {
    listeners[i]->ObserveContentRemoved(aDocument, aContainer, aChild,
                                        aIndexInContainer);
  }

  if (listeners.Length() == 0) {
    for (PRUint32 i = 0; i < mGlobalObservers.Length(); i++) {
      mGlobalObservers[i]->ObserveContentRemoved(aDocument, aContainer, aChild,
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

  nsTArray<uMenuChangeObserver *> *listeners;
  if (!mContentToObserverTable.Get(aContent, &listeners)) {
    listeners = new nsTArray<uMenuChangeObserver *>();
    mContentToObserverTable.Put(aContent, listeners);
  }

  for (PRUint32 i = 0; i < listeners->Length(); i++) {
    if (listeners->ElementAt(i) == aMenuObject) {
      return NS_ERROR_FAILURE;
    }
  }

  listeners->AppendElement(aMenuObject);
#ifdef DEBUG_chrisccoulson
  mCount++;
#endif
  return NS_OK;
}

nsresult
uGlobalMenuDocListener::UnregisterForContentChanges(nsIContent *aContent,
                                                    uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aContent);
  NS_ENSURE_ARG(aMenuObject);

  nsTArray<uMenuChangeObserver *> *listeners;
  if (!mContentToObserverTable.Get(aContent, &listeners)) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 length = listeners->Length();
  for (PRUint32 i = 0; i < length; i++) {
    if (listeners->ElementAt(i) == aMenuObject) {
      listeners->RemoveElementAt(i);
#ifdef DEBUG_chrisccoulson
      mCount--;
#endif
    }
  }

  nsresult rv = listeners->Length() < length ? NS_OK : NS_ERROR_FAILURE;

  if (listeners->Length() == 0) {
    mContentToObserverTable.Remove(aContent);
  }

  return rv;
}

nsresult
uGlobalMenuDocListener::RegisterForAllChanges(uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aMenuObject);

  PRUint32 count = mGlobalObservers.Length();
  for (PRUint32 i = 0; i < count; i ++) {
    if (mGlobalObservers[i] == aMenuObject) {
      // Don't add more than once
      return NS_ERROR_FAILURE;
    }
  }

  mGlobalObservers.AppendElement(aMenuObject);
  return NS_OK;
}

nsresult
uGlobalMenuDocListener::UnregisterForAllChanges(uMenuChangeObserver *aMenuObject)
{
  NS_ENSURE_ARG(aMenuObject);

  PRUint32 count = mGlobalObservers.Length();
  for (PRUint32 i = 0; i < count; i++) {
    if (mGlobalObservers[i] == aMenuObject) {
      mGlobalObservers.RemoveElementAt(i);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

void
uGlobalMenuDocListener::GetListeners(nsIContent *aContent,
                                     nsTArray<uMenuChangeObserver *>& _result)
{
  nsTArray<uMenuChangeObserver *> *listeners;
  if (mContentToObserverTable.Get(aContent, &listeners)) {
    _result.ReplaceElementsAt(0, _result.Length(), listeners->Elements(),
                              listeners->Length());
  }
}

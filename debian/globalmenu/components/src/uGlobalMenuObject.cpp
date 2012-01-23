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

#include <prtypes.h>
#include <nsStringAPI.h>
#include <nsIURI.h>
#include <nsILoadGroup.h>
#include <nsServiceManagerUtils.h>
#include <imgIContainer.h>
#include <nsNetError.h>
#include <nsNetUtil.h>
#include <nsIImageToPixbuf.h>
#include <nsIDOMNSElement.h>
#include <nsIDOMDOMTokenList.h>
#include <nsIDOMDocument.h>
#include <nsIDOMWindow.h>
#include <nsIDOMElement.h>
#include <nsIDOMCSSStyleDeclaration.h>
#include <nsIDOMCSSValue.h>
#include <nsIDOMCSSPrimitiveValue.h>
#include <nsIDOMRect.h>
#include <nsICaseConversion.h>
#include <nsUnicharUtilCIID.h>

#include <libdbusmenu-gtk/menuitem.h>
#include <gtk/gtk.h>

#include "uGlobalMenuObject.h"
#include "uGlobalMenuBar.h"
#include "uWidgetAtoms.h"

#include "uDebug.h"
#include "compat.h"

#define MAX_LABEL_NCHARS 40

typedef nsresult (nsIDOMRect::*GetRectSideMethod)(nsIDOMCSSPrimitiveValue**);

NS_IMPL_ISUPPORTS3(uGlobalMenuIconLoader, imgIDecoderObserver, imgIContainerObserver, nsIRunnable)

// Yes, we're abusing PRPackedBool a bit here. We initialize it to a value
// that is neither true or false, so that we don't need another static member
// to indicate the intialization status of it.
PRPackedBool uGlobalMenuIconLoader::sImagesInMenus = -1;
nsCOMPtr<imgILoader> uGlobalMenuIconLoader::sLoader = 0;

PRBool
uGlobalMenuIconLoader::ShouldShowIcon()
{
  // Ideally, we want to get the visibility of the XUL image in our menu item,
  // but that is anonymous content which is only created when the frame is drawn
  // (which obviously never happens here).
  // As an alternative, we get the user setting for menus-have-icons from
  // nsILookAndFeel. If menu icons are to be hidden, we hide everything except
  // for menuitems with the menuitem-with-favicon class. This is basically
  // how the visibility gets set anyway (see chrome://toolkit/content/xul.css),
  // which should work in most cases. But, I guess a theme could override this,
  // and then we ignore the users theme settings. Oh well......

  if (sImagesInMenus == static_cast<PRPackedBool>(-1)) {
    // We could get the correct GtkSettings by getting the GdkScreen that our
    // top-level window is on. However, I don't think this matters, as
    // nsILookAndFeel never had per-screen settings
    GtkSettings *settings = gtk_settings_get_default();
    gboolean menus_have_icons;
    g_object_get(settings, "gtk-menu-images", &menus_have_icons, NULL);

    sImagesInMenus = !!menus_have_icons;
  }

  if (sImagesInMenus) {
    return PR_TRUE;
  }

  return mMenuItem->WithFavicon();
}

void
uGlobalMenuIconLoader::LoadIcon()
{
  NS_DispatchToCurrentThread(this);
}

static PRInt32
GetDOMRectSide(nsIDOMRect* aRect, GetRectSideMethod aMethod)
{
  nsCOMPtr<nsIDOMCSSPrimitiveValue> dimensionValue;
  (aRect->*aMethod)(getter_AddRefs(dimensionValue));
  if (!dimensionValue)
    return -1;

  PRUint16 primitiveType;
  nsresult rv = dimensionValue->GetPrimitiveType(&primitiveType);
  if (NS_FAILED(rv) || primitiveType != nsIDOMCSSPrimitiveValue::CSS_PX)
    return -1;

  float dimension = 0;
  rv = dimensionValue->GetFloatValue(nsIDOMCSSPrimitiveValue::CSS_PX,
                                     &dimension);
  if (NS_FAILED(rv))
    return -1;

  return NSToIntRound(dimension);
}

NS_IMETHODIMP
uGlobalMenuIconLoader::Run()
{
  TRACE_WITH_MENUOBJECT(mMenuItem);
  // Some of this is borrowed from widget/src/cocoa/nsMenuItemIconX.mm
  if (mIconRequest) {
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nsnull;
  }

  if (!mMenuItem) {
    // Our menu item got destroyed already
    return NS_OK;
  }

  mMenuItem->GetContent(getter_AddRefs(mContent));

  nsIDocument *doc = mContent->GetCurrentDoc();
  if (!doc) {
    // We might have been removed from the menu, in which case we will
    // no longer be in a document
    return NS_OK;
  }

  if (!ShouldShowIcon()) {
    ClearIcon();
    return NS_OK;
  }

  mIconLoaded = PR_FALSE;

  nsAutoString uriString;
  PRBool hasImage = mContent->GetAttr(kNameSpaceID_None, uWidgetAtoms::image,
                                      uriString);

  nsresult rv;
  nsCOMPtr<nsIDOMRect> domRect;

  if (!hasImage) {
    DEBUG_WITH_MENUOBJECT(mMenuItem, "Menuitem does not have an image");
    nsCOMPtr<nsIDOMCSSStyleDeclaration> cssStyleDecl;
    nsCOMPtr<nsIDOMWindow> domWin;
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
    if (domDoc) {
      domDoc->GetDefaultView(getter_AddRefs(domWin));
      if (domWin) {
        nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mContent);
        if (domElement) {
          domWin->GetComputedStyle(domElement, EmptyString(),
                                   getter_AddRefs(cssStyleDecl));
        }
      }
    }

    if (!cssStyleDecl) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDOMCSSValue> cssValue;
    nsCOMPtr<nsIDOMCSSPrimitiveValue> primitiveValue;
    PRUint16 primitiveType;
    cssStyleDecl->GetPropertyCSSValue(NS_LITERAL_STRING("list-style-image"),
                                      getter_AddRefs(cssValue));
    if (cssValue) {
      primitiveValue = do_QueryInterface(cssValue);
      if (primitiveValue) {
        primitiveValue->GetPrimitiveType(&primitiveType);
        if (primitiveType == nsIDOMCSSPrimitiveValue::CSS_URI) {
          rv = primitiveValue->GetStringValue(uriString);
          if (NS_SUCCEEDED(rv)) {
            hasImage = PR_TRUE;
          }
        } else {
          NS_WARNING("list-style-image has wrong primitive type");
        }
      }
    }

    if (!hasImage) {
      return NS_OK;
    }

    cssStyleDecl->GetPropertyCSSValue(NS_LITERAL_STRING("-moz-image-region"),
                                      getter_AddRefs(cssValue));
    if (cssValue) {
      primitiveValue = do_QueryInterface(cssValue);
      if (primitiveValue) {
        primitiveValue->GetPrimitiveType(&primitiveType);
        if (primitiveType == nsIDOMCSSPrimitiveValue::CSS_RECT) {
          primitiveValue->GetRectValue(getter_AddRefs(domRect));
        }
      }
    }
  }

  DEBUG_WITH_MENUOBJECT(mMenuItem, "Icon URI: %s", DEBUG_CSTR_FROM_UTF16(uriString));
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), uriString);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create new URI");
    ClearIcon();
    return NS_OK;
  }

  if (!sLoader) {
    sLoader = do_GetService("@mozilla.org/image/loader;1");
  }

  NS_ASSERTION(sLoader, "No icon loader");
  if (!sLoader) {
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();

#if MOZILLA_BRANCH_MAJOR_VERSION >= 8
  rv = sLoader->LoadImage(uri, nsnull, nsnull, nsnull, loadGroup, this,
#else
  rv = sLoader->LoadImage(uri, nsnull, nsnull, loadGroup, this,
#endif
                          nsnull, nsIRequest::LOAD_NORMAL, nsnull,
                          nsnull, nsnull, getter_AddRefs(mIconRequest));
  NS_ENSURE_SUCCESS(rv, rv);

  mImageRect.SetEmpty();

  if (domRect) {
    PRInt32 bottom = GetDOMRectSide(domRect, &nsIDOMRect::GetBottom);
    PRInt32 right = GetDOMRectSide(domRect, &nsIDOMRect::GetRight);
    PRInt32 top = GetDOMRectSide(domRect, &nsIDOMRect::GetTop);
    PRInt32 left = GetDOMRectSide(domRect, &nsIDOMRect::GetLeft);

    if (top < 0 || left < 0 || bottom <= top || right <= left) {
      return NS_ERROR_FAILURE;
    }

    mImageRect.SetRect(left, top, right - left, bottom - top);
  }

  return NS_OK;
}

void
uGlobalMenuIconLoader::ClearIcon()
{
  dbusmenu_menuitem_property_remove(mMenuItem->GetDbusMenuItem(),
                                    DBUSMENU_MENUITEM_PROP_ICON_DATA);
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStartRequest(imgIRequest *aRequest)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStartDecode(imgIRequest *aRequest)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStartContainer(imgIRequest *aRequest, imgIContainer *aContainer)
{
  TRACE_WITH_MENUOBJECT(mMenuItem);
  aContainer->RequestDecode();
  return NS_OK;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStartFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnDataAvailable(imgIRequest *aRequest,
                                       MOZ_API_BOOL aCurrentFrame,
                                       const nsIntRect *aRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStopFrame(imgIRequest *aRequest, PRUint32 aFrame)
{
  TRACE_WITH_MENUOBJECT(mMenuItem);
  if (aRequest != mIconRequest) {
    return NS_ERROR_FAILURE;
  }

  if (mIconLoaded) {
    DEBUG_WITH_MENUOBJECT(mMenuItem, "Icon is already loaded");
    return NS_OK;
  }

  mIconLoaded = PR_TRUE;

  nsCOMPtr<imgIContainer> img;
  aRequest->GetImage(getter_AddRefs(img));
  if (!img) {
    NS_WARNING("Failed to get image");
    return NS_ERROR_FAILURE;
  }

  PRInt32 origWidth;
  PRInt32 origHeight;
  img->GetWidth(&origWidth);
  img->GetHeight(&origHeight);

  PRBool needsClip = PR_FALSE;

  if (!mImageRect.IsEmpty()) {
    if (mImageRect.XMost() > origWidth || mImageRect.YMost() > origHeight) {
      NS_WARNING("-moz-image-region is larger than image");
      return NS_ERROR_FAILURE;
    }

    if (!(mImageRect.x == 0 && mImageRect.y == 0 &&
         mImageRect.width == origWidth && mImageRect.height == origHeight)) {
      needsClip = PR_TRUE;
    }
  }

  if ((!needsClip && (origWidth > 100 || origHeight > 100)) ||
     (needsClip && (mImageRect.width > 100 || mImageRect.height > 100))) {
    /* The icon data needs to go across DBus. Make sure the icon
     * data isn't too large, else our connection gets terminated and
     * GDbus helpfully aborts the application. Thank you :)
     */
    NS_WARNING("Icon data too large");
    ClearIcon();
    return NS_OK;
  }

  nsCOMPtr<imgIContainer> clippedImg;
  if (needsClip) {
    nsresult rv = img->ExtractFrame(0, mImageRect, 0,
                                    getter_AddRefs(clippedImg));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to clip icon");
      return NS_ERROR_FAILURE;
    }
  }

  nsCOMPtr<nsIImageToPixbuf> converter =
    do_GetService("@mozilla.org/widget/image-to-gdk-pixbuf;1");
  NS_ASSERTION(converter, "No image converter");
  if (!converter) {
    return NS_ERROR_FAILURE;
  }

  GdkPixbuf *pixbuf =
    converter->ConvertImageToPixbuf(clippedImg ? clippedImg : img);
  if (pixbuf) {
    dbusmenu_menuitem_property_set_image(mMenuItem->GetDbusMenuItem(),
                                         DBUSMENU_MENUITEM_PROP_ICON_DATA,
                                         pixbuf);
    g_object_unref(pixbuf);
  } else {
    ClearIcon();
  }

  return NS_OK;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStopContainer(imgIRequest *aRequest,
                                       imgIContainer *aContainer)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStopDecode(imgIRequest *aRequest, nsresult status,
                                    const PRUnichar *statusArg)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnStopRequest(imgIRequest *aRequest, MOZ_API_BOOL aIsLastPart)
{
  TRACE_WITH_MENUOBJECT(mMenuItem);
  return NS_OK;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::OnDiscard(imgIRequest *aRequest)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
uGlobalMenuIconLoader::FrameChanged(imgIContainer *aContainer,
                                    const nsIntRect *aDirtyRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
uGlobalMenuIconLoader::Destroy()
{
  TRACE_WITH_MENUOBJECT(mMenuItem);
  if (mIconRequest) {
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nsnull;
  }

  mMenuItem = nsnull;
}

void
uGlobalMenuObject::GetContent(nsIContent **_retval)
{
  if (!_retval) {
    return;
  }
  *_retval = mContent;
  NS_IF_ADDREF(*_retval);
}

void
uGlobalMenuObject::SyncLabelFromContent(nsIContent *aContent)
{
  TRACE_WITH_THIS_MENUOBJECT();
  // Gecko stores the label and access key in separate attributes
  // so we need to convert label="Foo"/accesskey="F" in to
  // label="_Foo" for dbusmenu

  nsAutoString label;
  if (aContent && aContent->GetAttr(kNameSpaceID_None,
                                    uWidgetAtoms::label, label)) {
    UGM_BLOCK_EVENTS_FOR_CURRENT_SCOPE();
    DEBUG_WITH_CONTENT(aContent, "Content has label \"%s\"", DEBUG_CSTR_FROM_UTF16(label));
    mContent->SetAttr(kNameSpaceID_None, uWidgetAtoms::label, label, MOZ_API_TRUE);
  } else {
    mContent->GetAttr(kNameSpaceID_None, uWidgetAtoms::label, label);
  }

  nsAutoString accesskey;
  mContent->GetAttr(kNameSpaceID_None, uWidgetAtoms::accesskey, accesskey);

  const PRUnichar *tmp = accesskey.BeginReading();
  PRUnichar keyUpper;
  PRUnichar keyLower;
  // XXX: I think we need to link against libxul.so to get ToLowerCase
  //      and ToUpperCase from nsUnicharUtils.h
  nsCOMPtr<nsICaseConversion> converter =
    do_GetService(NS_UNICHARUTIL_CONTRACTID);
  if (converter) {
    converter->ToUpper(*tmp, &keyUpper);
    converter->ToLower(*tmp, &keyLower);
  } else {
    if (*tmp < 256) {
      keyUpper = toupper(char(*tmp));
      keyLower = tolower(char(*tmp));
    } else {
      NS_WARNING("accesskey matching is case-sensitive when it shouldn't be");
      keyUpper = *tmp;
      keyLower = *tmp;
    }
  }

  PRUnichar *cur = label.BeginWriting();
  PRUnichar *end = label.EndWriting();
  int length = label.Length();
  int pos = 0;
  PRBool foundAccessKey = PR_FALSE;

  while (cur < end) {
    if (*cur != PRUnichar('_')) {
      if ((*cur != keyLower && *cur != keyUpper) || foundAccessKey) {
        cur++;
        pos++;
        continue;
      }
      foundAccessKey = PR_TRUE;
    }

    length += 1;
    label.SetLength(length);
    int newLength = label.Length();
    if (length != newLength)
      break; 
     
    cur = label.BeginWriting() + pos;
    end = label.EndWriting();
    memmove(cur + 1, cur, (length - 1 - pos) * sizeof(PRUnichar));
//                   \^/
    *cur = PRUnichar('_'); // Yeah!
//                    v

    cur += 2;
    pos += 2;
  }

  // Ellipsize long labels. I've picked an arbitrary length here
  if (length > MAX_LABEL_NCHARS) {
    cur = label.BeginWriting();
    for (PRUint32 i = 1; i < 4; i++) {
      *(cur + (MAX_LABEL_NCHARS - i)) = PRUnichar('.');
    }
    *(cur + MAX_LABEL_NCHARS) = nsnull;
    label.SetLength(MAX_LABEL_NCHARS);
  }

  nsCAutoString clabel;
  CopyUTF16toUTF8(label, clabel);
  DEBUG_WITH_THIS_MENUOBJECT("Setting label to \"%s\"", clabel.get());
  dbusmenu_menuitem_property_set(mDbusMenuItem,
                                 DBUSMENU_MENUITEM_PROP_LABEL,
                                 clabel.get());
}

void
uGlobalMenuObject::SyncLabelFromContent()
{
  SyncLabelFromContent(nsnull);
}

// Synchronize the 'hidden' attribute on the DOM node with the
// 'visible' property on the dbusmenu node
void
uGlobalMenuObject::SyncVisibilityFromContent()
{
  TRACE_WITH_THIS_MENUOBJECT();
  mContentVisible = !IsHidden();
  PRBool realVis = (!mMenuBar || !ShouldShowOnlyForKb() ||
                    mMenuBar->OpenedByKeyboard()) ?
                    mContentVisible : PR_FALSE;
  DEBUG_WITH_THIS_MENUOBJECT("Setting %s", realVis ? "visible" : "hidden");

  dbusmenu_menuitem_property_set_bool(mDbusMenuItem,
                                      DBUSMENU_MENUITEM_PROP_VISIBLE,
                                      realVis);
}

void
uGlobalMenuObject::SyncSensitivityFromContent(nsIContent *aContent)
{
  TRACE_WITH_THIS_MENUOBJECT();

  nsIContent *content;
  if (aContent) {
    content = aContent;
  } else {
    content = mContent;
  }
  PRBool disabled = content->AttrValueIs(kNameSpaceID_None,
                                         uWidgetAtoms::disabled,
                                         uWidgetAtoms::_true,
                                         eCaseMatters);
  DEBUG_WITH_THIS_MENUOBJECT("Setting %s", disabled ? "disabled" : "enabled");

  if (aContent) {
    UGM_BLOCK_EVENTS_FOR_CURRENT_SCOPE();
    if (disabled) {
      mContent->SetAttr(kNameSpaceID_None, uWidgetAtoms::disabled,
                        NS_LITERAL_STRING("true"), MOZ_API_TRUE);
    } else {
      mContent->UnsetAttr(kNameSpaceID_None, uWidgetAtoms::disabled, MOZ_API_TRUE);
    }
  }

  dbusmenu_menuitem_property_set_bool(mDbusMenuItem,
                                      DBUSMENU_MENUITEM_PROP_ENABLED,
                                      !disabled);
}

void
uGlobalMenuObject::SyncSensitivityFromContent()
{
  SyncSensitivityFromContent(nsnull);
}

void
uGlobalMenuObject::UpdateInfoFromContentClass()
{
  TRACE_WITH_THIS_MENUOBJECT();
  nsCOMPtr<nsIDOMNSElement> element(do_QueryInterface(mContent));
  if (!element) {
    return;
  }

  nsCOMPtr<nsIDOMDOMTokenList> classes;
  element->GetClassList(getter_AddRefs(classes));
  if (!classes) {
    return;
  }

  MOZ_API_BOOL tmp;
  classes->Contains(NS_LITERAL_STRING("show-only-for-keyboard"),
                    &tmp);
  mShowOnlyForKb = !!tmp;

  classes->Contains(NS_LITERAL_STRING("menuitem-with-favicon"),
                    &tmp);
  mWithFavicon = !!tmp;

  DEBUG_WITH_THIS_MENUOBJECT("show-only-for-keyboard class? %s", mShowOnlyForKb ? "Yes" : "No");
  DEBUG_WITH_THIS_MENUOBJECT("menuitem-with-favicon class? %s", mWithFavicon ? "Yes" : "No");
}

PRBool
uGlobalMenuObject::IsHidden()
{
  return mContent->AttrValueIs(kNameSpaceID_None, uWidgetAtoms::hidden,
                               uWidgetAtoms::_true, eCaseMatters) ||
         mContent->AttrValueIs(kNameSpaceID_None, uWidgetAtoms::collapsed,
                               uWidgetAtoms::_true, eCaseMatters);
}

void
uGlobalMenuObject::UpdateVisibility()
{
  TRACE_WITH_THIS_MENUOBJECT();
  if (!mMenuBar) {
    return;
  }

  PRBool newVis = (!ShouldShowOnlyForKb() || mMenuBar->OpenedByKeyboard()) ?
                   mContentVisible : PR_FALSE;

  DEBUG_WITH_THIS_MENUOBJECT("Setting %s", newVis ? "visible" : "hidden");
  dbusmenu_menuitem_property_set_bool(mDbusMenuItem,
                                      DBUSMENU_MENUITEM_PROP_VISIBLE,
                                      newVis);
}

void
uGlobalMenuObject::SyncIconFromContent()
{
  TRACE_WITH_THIS_MENUOBJECT();
  if (!mIconLoader) {
    mIconLoader = new uGlobalMenuIconLoader(this);
  }

  mIconLoader->LoadIcon();
}

void
uGlobalMenuObject::DestroyIconLoader()
{
  if (mIconLoader) {
    mIconLoader->Destroy();
  }
}
